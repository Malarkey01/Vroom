/* =========================================================================
 *  MainWindow.h — declares the top-level home screen for Vroom
 * -------------------------------------------------------------------------
 *  create_main_window()
 *      Builds a full-screen GTK window that shows three big buttons:
 *          • Android Auto   — spawns the external “autoapp” process
 *          • Vehicle Info   — opens the live OBD-II dashboard window
 *          • Settings       — opens the settings dialog (volume, brightness)
 * ========================================================================= */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtk/gtk.h>

/* Build and return the fully initialised main window.                     */
GtkWidget *create_main_window(void);

#endif /* MAINWINDOW_H */
