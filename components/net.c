/* See LICENSE file for copyright and license details. */
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

const char *
get_essid(void)
{
    int             ret;
    struct ifaddrs *ifs;
    struct ifaddrs *ifp;

    const char *ifname = NULL;

    ret = getifaddrs(&ifs);

    if (ret != 0)
        return NULL;

    for (ifp = &ifs[0]; ifp != NULL; ifp = ifp->ifa_next) {
        sa_family_t family = ifp->ifa_addr->sa_family;

        if (family == AF_INET6)
            continue;

        if (family == AF_INET) {
#if defined(__linux__)
            if (strstr(ifp->ifa_name, "wl") != NULL)
                ifname = ifp->ifa_name;
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
            if (strstr(ifp->ifa_name, "iwn") != NULL || strstr(ifp->ifa_name, "rtwn") != NULL
                ifname = ifp->ifa_name;
#endif
        }
    }

    freeifaddrs(ifs);

    return ifname;
}
