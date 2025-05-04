/* =========================================================================
 *  Popup.c — implementation of the transient HUD popup
 * ========================================================================= */
#include "Popup.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>     /* strlen, strcpy */

/* -------------------------------------------------------------------------
 *  Style constants
 * ------------------------------------------------------------------------- */
static const char POPUP_FONT_DESC[]   = "Sans 36";
static const char POPUP_TEXT_COLOR[]  = "#FFFFFF";
static const int  POPUP_WINDOW_WIDTH  = 300;
static const int  POPUP_WINDOW_HEIGHT = 100;

/* -------------------------------------------------------------------------
 *  Forward declarations
 * ------------------------------------------------------------------------- */
static gboolean create_popup_in_mainloop(gpointer user_data);
static gboolean hide_popup_callback     (gpointer w);

/* -------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */
void show_temp_popup(const char *message)
/* Queue the popup creation on the GTK main thread. */
{
    char *msg_copy = malloc(strlen(message) + 1);
    strcpy(msg_copy, message);
    g_idle_add(create_popup_in_mainloop, msg_copy);
}

/* -------------------------------------------------------------------------
 *  Helpers
 * ------------------------------------------------------------------------- */
static gboolean create_popup_in_mainloop(gpointer user_data)
/* Build and display the popup window (runs in GTK thread). */
{
    char *message = (char *)user_data;

    GtkWidget *popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(popup), "Popup");
    gtk_window_set_default_size(GTK_WINDOW(popup),
                                POPUP_WINDOW_WIDTH,
                                POPUP_WINDOW_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(popup), GTK_WIN_POS_CENTER_ALWAYS);

    GtkWidget *label = gtk_label_new(NULL);
    char *markup = g_strdup_printf(
        "<span font_desc='%s' foreground='%s'>%s</span>",
        POPUP_FONT_DESC, POPUP_TEXT_COLOR, message);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);

    gtk_container_add(GTK_CONTAINER(popup), label);
    gtk_widget_show_all(popup);

    /* Auto-hide after 1.5 s */
    g_timeout_add(1500, hide_popup_callback, popup);

    free(message);
    return G_SOURCE_REMOVE;
}

static gboolean hide_popup_callback(gpointer w)
/* Destroy the popup and stop the timeout. */
{
    gtk_widget_destroy(GTK_WIDGET(w));
    return G_SOURCE_REMOVE;
}
