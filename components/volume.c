/* See LICENSE file for copyright and license details. */
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../slstatus.h"
#include "../util.h"

#if defined(__OpenBSD__) | defined(__FreeBSD__)
#include <poll.h>
#include <sndio.h>
#include <stdlib.h>
#include <sys/queue.h>

struct control {
    LIST_ENTRY(control) next;
    unsigned int addr;
#define CTRL_NONE 0
#define CTRL_LEVEL 1
#define CTRL_MUTE 2
    unsigned int type;
    unsigned int maxval;
    unsigned int val;
};

static LIST_HEAD(, control) controls = LIST_HEAD_INITIALIZER(controls);
static struct pollfd *pfds;
static struct sioctl_hdl *hdl;
static int initialized;

/*
 * Call-back to obtain the description of all audio controls.
 */
static void ondesc(void *unused, struct sioctl_desc *desc, int val) {
    struct control *c, *ctmp;
    unsigned int type = CTRL_NONE;

    if (desc == NULL)
        return;

    /* Delete existing audio control with the same address. */
    LIST_FOREACH_SAFE(c, &controls, next, ctmp) {
        if (desc->addr == c->addr) {
            LIST_REMOVE(c, next);
            free(c);
            break;
        }
    }

    /* Only match output.level and output.mute audio controls. */
    if (desc->group[0] != 0 || strcmp(desc->node0.name, "output") != 0)
        return;
    if (desc->type == SIOCTL_NUM && strcmp(desc->func, "level") == 0)
        type = CTRL_LEVEL;
    else if (desc->type == SIOCTL_SW && strcmp(desc->func, "mute") == 0)
        type = CTRL_MUTE;
    else
        return;

    c = malloc(sizeof(struct control));
    if (c == NULL) {
        warn("sndio: failed to allocate audio control\n");
        return;
    }

    c->addr = desc->addr;
    c->type = type;
    c->maxval = desc->maxval;
    c->val = val;
    LIST_INSERT_HEAD(&controls, c, next);
}

/*
 * Call-back invoked whenever an audio control changes.
 */
static void onval(void *unused, unsigned int addr, unsigned int val) {
    struct control *c;

    LIST_FOREACH(c, &controls, next) {
        if (c->addr == addr)
            break;
    }
    c->val = val;
}

static void cleanup(void) {
    struct control *c;

    if (hdl) {
        sioctl_close(hdl);
        hdl = NULL;
    }

    free(pfds);
    pfds = NULL;

    while (!LIST_EMPTY(&controls)) {
        c = LIST_FIRST(&controls);
        LIST_REMOVE(c, next);
        free(c);
    }
}

static int init(void) {
    hdl = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
    if (hdl == NULL) {
        warn("sndio: cannot open device");
        goto failed;
    }

    if (!sioctl_ondesc(hdl, ondesc, NULL)) {
        warn("sndio: cannot set control description call-back");
        goto failed;
    }

    if (!sioctl_onval(hdl, onval, NULL)) {
        warn("sndio: cannot set control values call-back");
        goto failed;
    }

    pfds = calloc(sioctl_nfds(hdl), sizeof(struct pollfd));
    if (pfds == NULL) {
        warn("sndio: cannot allocate pollfd structures");
        goto failed;
    }

    return 1;
failed:
    cleanup();
    return 0;
}

const char *vol_perc(const char *unused) {
    struct control *c;
    int n, v, value;

    if (!initialized)
        initialized = init();

    if (hdl == NULL)
        return NULL;

    n = sioctl_pollfd(hdl, pfds, POLLIN);
    if (n > 0) {
        n = poll(pfds, n, 0);
        if (n > 0) {
            if (sioctl_revents(hdl, pfds) & POLLHUP) {
                warn("sndio: disconnected");
                cleanup();
                initialized = 0;
                return NULL;
            }
        }
    }

    value = 100;
    LIST_FOREACH(c, &controls, next) {
        if (c->type == CTRL_MUTE && c->val == 1)
            value = 0;
        else if (c->type == CTRL_LEVEL) {
            v = (c->val * 100 + c->maxval / 2) / c->maxval;
            /* For multiple channels return the minimum. */
            if (v < value)
                value = v;
        }
    }

    return bprintf("%d", value);
}
#else
#if __has_include(<alsa/asoundlib.h>)
#include <alsa/asoundlib.h>

int retrieve_volume(const char *card, ALSAVolume *avolume) {
    int s;
    long curr_volume;
    snd_mixer_t *mixer;
    snd_mixer_elem_t *mixer_elem;
    snd_mixer_selem_id_t *melem_id;
    static int mix_index = 0;

    snd_mixer_selem_id_alloca(&melem_id);
    snd_mixer_selem_id_set_index(melem_id, mix_index);
    snd_mixer_selem_id_set_name(melem_id, "Master");

    int res = 0;
    s = snd_mixer_open(&mixer, 0);

    if (s != 0)
        return -1;

    s = snd_mixer_attach(mixer, card);

    if (s != 0) {
        res = -1;
        goto cleanup;
    }

    s = snd_mixer_selem_register(mixer, NULL, NULL);

    if (s != 0) {
        res = -1;
        goto cleanup;
    }

    s = snd_mixer_load(mixer);

    if (s != 0) {
        res = -1;
        goto cleanup;
    }

    mixer_elem = snd_mixer_find_selem(mixer, melem_id);

    if (!mixer_elem) {
        res = -1;
        goto cleanup;
    }

    long max_vol, min_vol;

    snd_mixer_selem_get_playback_volume_range(mixer_elem, &min_vol, &max_vol);

    s = snd_mixer_selem_get_playback_volume(mixer_elem, 0, &curr_volume);

    if (s != 0) {
        res = -1;
        goto cleanup;
    }

    snd_mixer_selem_get_playback_switch(mixer_elem, 0, &avolume->is_muted);

    if (s != 0) {
        res = -1;
        goto cleanup;
    }

    curr_volume -= min_vol;
    max_vol -= min_vol;
    min_vol = 0;
    curr_volume = 100 * curr_volume / max_vol;
    avolume->volume = curr_volume;

cleanup:
    snd_mixer_close(mixer);
    return res;
}

const char *volume_perc(const char *cardname) {
    int s;
    ALSAVolume curr_vol = {0};
    s = retrieve_volume(cardname, &curr_vol);

    if (s == -1)
        return "?";

    return bprintf("%-3ld", curr_vol.volume);
}

const char *volume_status(const char *cardname) {
    int s;
    ALSAVolume curr_vol = {0};
    s = retrieve_volume(cardname, &curr_vol);

    if (s == -1)
        return "?";

    if (!curr_vol.is_muted)
        return "󰖁";

    if (curr_vol.volume >= 70) {
        return "󰕾";
    } else if (curr_vol.volume >= 30) {
        return "󰖀";
    } else {
        return "󰕿";
    }

    return "?";
}
#endif
#endif
