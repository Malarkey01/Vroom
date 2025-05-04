/* =========================================================================
 *  main.c — entry point for the Vroom Infotainment GUI
 * -------------------------------------------------------------------------
 *  1. Initialise GTK.
 *  2. Launch the rotary-encoder helper (GPIO interrupt thread).
 *  3. Build and display the main menu window.
 *  4. Enter the GTK main loop and wait for events forever.
 * ========================================================================= */
#include <gtk/gtk.h>
#include "MainWindow.h"
#include "RotaryEncoder.h"
#include "AudioManager.h"

int main(int argc, char *argv[])
{
    /* GTK must be initialised before any widgets are created */
    gtk_init(&argc, &argv);

    /* Prime AudioManager so the rotary knob has a sink from the start */
    audio_manager_init();

    /* Rotary encoder: sets up GPIO interrupts + helper thread */
    start_rotary_thread();

    /* Build the full-screen home screen */
    GtkWidget *main_window = create_main_window();
    gtk_widget_show_all(main_window);

    /* Hand control to GTK until the user quits */
    gtk_main();
    return 0;
}
