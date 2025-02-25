/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__linux__)
#include <net/if.h>

const char *
get_essid(void)
{
    int sock, ret;

    struct ifconf ifc;
    struct ifreq *ifr;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
        return NULL;

    ret = ioctl(sock, SIOCGIFCONF, &ifc, sizeof(struct ifconf));

    if (ret != 0) {
        perror("SIOCGIFCONF");
        return NULL;
    }

    ifc.ifc_buf = malloc(ifc.ifc_len);

    if (ifc.ifc_buf == NULL)
        return NULL;

    ret = ioctl(sock, SIOCGIFCONF, &ifc, sizeof(struct ifconf));

    if (ret != 0) {
        perror("SIOCGIFCONF");
        return NULL;
    }

    ifr = ifc.ifc_req;

    const char *ifname = NULL;

    for (int i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
        if (strstr(ifr->ifr_name, "wl") != NULL) {
            ifname = ifr->ifr_name;
        }
    }

    free(ifc.ifc_buf);
    return ifname;
}

#endif
