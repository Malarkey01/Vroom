/* =========================================================================
 *  Popup.h — tiny transient message window
 * -------------------------------------------------------------------------
 *  show_temp_popup(message)
 *      Creates a 300 × 100 px window with large white text centered on
 *      screen.  The popup disappears automatically after ~1.5 s.
 *
 *      Thread-safe: if this function is called from a non-GTK thread
 *      (e.g., rotary-encoder ISR thread), the popup is scheduled on the
 *      GTK main loop via g_idle_add().
 * ========================================================================= */
#ifndef POPUP_H
#define POPUP_H

void show_temp_popup(const char *message);

#endif /* POPUP_H */
