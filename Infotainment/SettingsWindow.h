/* =========================================================================
 *  SettingsWindow.h — full-screen volume / brightness dialog
 * -------------------------------------------------------------------------
 *  open_settings_window(parent)
 *      Creates a modal full-screen window with:
 *          • Brightness slider   (0-31 → 0-100 %)
 *          • Volume slider       (0-100 %)
 *          • Audio-output combo  (lists PulseAudio sinks)
 *          • “Back” button       (closes the dialog)
 *
 *  settings_update_brightness_slider(val_0-31)
 *  settings_update_volume_slider(val_0-100)
 *      Thread-safe helpers for the rotary encoder ISR.  They queue an
 *      idle callback so the sliders follow hardware changes in real time.
 * ========================================================================= */
#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <gtk/gtk.h>

void open_settings_window(GtkWindow *parent);

void settings_update_brightness_slider(int brightness_0_to_31);
void settings_update_volume_slider    (int volume_0_to_100);

#endif /* SETTINGSWINDOW_H */
