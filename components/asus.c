/* See LICENSE file for copyright and license details. */

#if defined(__ASUS_AURA__)

#include "../slstatus.h"

#ifndef _STRING_H
#include <string.h>
#endif

const char *get_gpu_mode(const char *arg) {
    const char *cmd_result = run_command("/usr/bin/supergfxctl -g");

    if (strstr(cmd_result, "Integrated") != NULL)
        return "";

    if (strstr(cmd_result, "Hybrid") != NULL)
        return "󰕘";

    if (strstr(cmd_result, "AsusMuxDgpu") != NULL)
        return "󱓟";

    return "?";
}

const char *get_fan_mode(const char *arg) {
    const char *cmd_result =
        run_command("/usr/bin/asusctl profile -p | cut -d' ' -f4");

    if (strstr(cmd_result, "Quiet") != NULL)
        return "";

    if (strstr(cmd_result, "Balanced") != NULL)
        return "󰶥";

    if (strstr(cmd_result, "Performance") != NULL)
        return "󱪂";

    return "?";
}
#endif
