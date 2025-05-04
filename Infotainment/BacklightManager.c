/* =========================================================================
 *  BacklightManager.c — thin wrapper around the back-light sysfs node
 * ========================================================================= */
#include "BacklightManager.h"
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------- */
/*  Constants                                                             */
/* ---------------------------------------------------------------------- */
static const char BACKLIGHT_SYSFS_PATH[] = "/sys/class/backlight/11-0045/brightness";
static const int  BACKLIGHT_MAX_VALUE    = 31;   /* panel’s hard limit */

/* ---------------------------------------------------------------------- */
int read_backlight_brightness(void)
/* ----------------------------------------------------------------------
 *  Reads the integer inside the sysfs node, clamps it to [0-31], and
 *  returns the result.  On failure (e.g., file missing), the function
 *  assumes full brightness so the UI sliders still display something.
 * ---------------------------------------------------------------------- */
{
    FILE *fp = fopen(BACKLIGHT_SYSFS_PATH, "r");
    if (!fp)
        return BACKLIGHT_MAX_VALUE;                /* fall-back */

    int val = BACKLIGHT_MAX_VALUE;
    fscanf(fp, "%d", &val);
    fclose(fp);

    if (val < 0)                   val = 0;
    if (val > BACKLIGHT_MAX_VALUE) val = BACKLIGHT_MAX_VALUE;
    return val;
}

/* ---------------------------------------------------------------------- */
void set_backlight_brightness(int brightness)
/* ----------------------------------------------------------------------
 *  Clamps `brightness` to [0-31] and pushes it into the sysfs file via a
 *  shell one-liner.  The write uses `sudo sh -c 'echo … > path'`, so the
 *  user running Vroom must have a matching entry in /etc/sudoers.
 * ---------------------------------------------------------------------- */
{
    if (brightness < 0)                   brightness = 0;
    if (brightness > BACKLIGHT_MAX_VALUE) brightness = BACKLIGHT_MAX_VALUE;

    char cmd[128];
    snprintf(cmd, sizeof cmd,
             "sudo sh -c 'echo %d > %s'",
             brightness, BACKLIGHT_SYSFS_PATH);
    system(cmd);
}
