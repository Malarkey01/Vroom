/* =========================================================================
 *  BacklightManager.h — LCD back-light control via Linux sysfs
 * -------------------------------------------------------------------------
 *  The display panel exposes a brightness register at
 *      /sys/class/backlight/11-0045/brightness
 *  accepted values: 0 (dark) … 31 (full).
 *
 *  This header offers two small, blocking helpers for that register.
 * ========================================================================= */
#ifndef BACKLIGHTMANAGER_H
#define BACKLIGHTMANAGER_H

/* -------------------------------------------------------------------------
 *  read_backlight_brightness
 *  ------------------------------------------------------------------------
 *  Returns the current brightness (0-31) read from sysfs.
 *  If the file cannot be opened, the function falls back to 31 (100 %).
 * ------------------------------------------------------------------------- */
int read_backlight_brightness(void);

/* -------------------------------------------------------------------------
 *  set_backlight_brightness
 *  ------------------------------------------------------------------------
 *  Writes a new brightness value to sysfs.  Any input outside 0-31 is
 *  clamped to the legal range.  Requires sudo privileges configured so
 *  the echo redirection can run without a password prompt.
 * ------------------------------------------------------------------------- */
void set_backlight_brightness(int brightness_0_to_31);

#endif /* BACKLIGHTMANAGER_H */
