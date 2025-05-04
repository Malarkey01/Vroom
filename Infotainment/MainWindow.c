/* =========================================================================
 *  MainWindow.c — Vroom “home” screen
 * -------------------------------------------------------------------------
 *  • Full-screen GtkWindow with three 300×300-px buttons
 *        1) Android Auto   → spawns external autoapp process
 *        2) Vehicle Info   → opens live OBD-II dashboard window
 *        3) Settings       → opens modal Settings window
 *  • Assets live in Infotainment/images/
 *  • Esc or window close quits; cursor hidden on realise.
 * ========================================================================= */
#include "MainWindow.h"
#include "SettingsWindow.h"       /* open_settings_window()            */
#include "VehicleInfoWindow.h"    /* create_vehicle_info_window()      */
#include "Popup.h"                /* transient on-screen messages      */

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */
static const int BUTTON_WIDTH  = 300;
static const int BUTTON_HEIGHT = 300;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */
static void load_global_css            (void);
static void hide_cursor_on_realize     (GtkWidget *, gpointer);
static gboolean on_key_press_event     (GtkWidget *, GdkEventKey *, gpointer);
static void on_window_destroy          (GtkWidget *, gpointer);

static GtkWidget *create_button_with_image          (const gchar *img,
                                                     gint w, gint h);
static GtkWidget *create_button_with_label_and_image(const gchar *img,
                                                     const gchar *label,
                                                     GCallback    clicked_cb);

static void on_autoapp_child_exit      (GPid, gint, gpointer);
static void on_AndroidAuto_button_clicked (GtkWidget *, gpointer);
static void on_vehicle_info_button_clicked (GtkWidget *, gpointer);
static void on_settings_button_clicked     (GtkWidget *, gpointer);

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */
GtkWidget *create_main_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Vroom Infotainment");
    gtk_window_fullscreen(GTK_WINDOW(window));
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 480);

    g_signal_connect(window, "destroy",         G_CALLBACK(on_window_destroy), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_event), NULL);
    g_signal_connect(window, "realize",         G_CALLBACK(hide_cursor_on_realize), NULL);

    load_global_css();

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_container_add(GTK_CONTAINER(window), main_box);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);

    GtkWidget *android_box = create_button_with_label_and_image(
        "images/AndroidAuto.png", "Android Auto",
        G_CALLBACK(on_AndroidAuto_button_clicked));

    GtkWidget *vehicle_box = create_button_with_label_and_image(
        "images/Vehicle.png", "Vehicle Info",
        G_CALLBACK(on_vehicle_info_button_clicked));

    GtkWidget *settings_box = create_button_with_label_and_image(
        "images/Settings.png", "Settings",
        G_CALLBACK(on_settings_button_clicked));

    gtk_box_pack_start(GTK_BOX(main_box), android_box, TRUE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(main_box), vehicle_box, TRUE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(main_box), settings_box, TRUE, TRUE, 20);

    return window;                 /* main.c will gtk_widget_show_all() */
}

/* ------------------------------------------------------------------ */
/*  Window-level callbacks                                            */
/* ------------------------------------------------------------------ */
static void on_window_destroy(GtkWidget *w, gpointer)
{
    if (gtk_widget_get_window(w))
        gdk_window_set_cursor(gtk_widget_get_window(w), NULL);
    gtk_main_quit();
}

static gboolean on_key_press_event(GtkWidget *, GdkEventKey *e, gpointer)
{
    if (e->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

static void hide_cursor_on_realize(GtkWidget *w, gpointer)
{
    GdkWindow  *gw = gtk_widget_get_window(w);
    GdkDisplay *d  = gdk_window_get_display(gw);

    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
    gdk_pixbuf_fill(pb, 0x00000000);
    GdkCursor *c = gdk_cursor_new_from_pixbuf(d, pb, 0, 0);

    gdk_window_set_cursor(gw, c);
    g_object_unref(c);
    g_object_unref(pb);
}

/* ------------------------------------------------------------------ */
/*  Simple global CSS                                                 */
/* ------------------------------------------------------------------ */
static void load_global_css(void)
{
    static const gchar *css =
        "window        { background:#333; }\n"
        "#custom-label { color:#FFF; font-size:40px; }\n"
        "scale slider  { min-width:40px; min-height:40px; }";

    GtkCssProvider *prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(prov);
}

/* ------------------------------------------------------------------ */
/*  Button helpers                                                    */
/* ------------------------------------------------------------------ */
static GtkWidget *create_button_with_image(const gchar *img, gint w, gint h)
{
    GtkWidget *btn = gtk_button_new();
    GdkPixbuf *pb  = gdk_pixbuf_new_from_file_at_scale(img, w, h, TRUE, NULL);
    GtkWidget *im  = gtk_image_new_from_pixbuf(pb);
    gtk_button_set_image(GTK_BUTTON(btn), im);
    gtk_button_set_always_show_image(GTK_BUTTON(btn), TRUE);
    g_object_unref(pb);
    return btn;
}

static void on_button_pressed(GtkWidget *w, gpointer)  { gtk_widget_set_size_request(w, 320, 320); }
static void on_button_released(GtkWidget *w, gpointer) { gtk_widget_set_size_request(w, BUTTON_WIDTH, BUTTON_HEIGHT); }

static GtkWidget *create_button_with_label_and_image(const gchar *img,
                                                     const gchar *label,
                                                     GCallback    clicked_cb)
{
    GtkWidget *box    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *button = create_button_with_image(img, BUTTON_WIDTH, BUTTON_HEIGHT);

    g_signal_connect(button, "clicked",  clicked_cb, NULL);
    g_signal_connect(button, "pressed",  G_CALLBACK(on_button_pressed),  NULL);
    g_signal_connect(button, "released", G_CALLBACK(on_button_released), NULL);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_name(lbl, "custom-label");

    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), lbl,    FALSE, FALSE, 0);
    return box;
}

/* ------------------------------------------------------------------ */
/*  Android Auto                                                      */
/* ------------------------------------------------------------------ */
static void on_autoapp_child_exit(GPid pid, gint status, gpointer user_data)
{
    GtkWindow *main = GTK_WINDOW(user_data);
    g_spawn_close_pid(pid);
    g_print("autoapp exited status=%d\n", status);
    gtk_widget_show(GTK_WIDGET(main));
}

static void on_AndroidAuto_button_clicked(GtkWidget *w, gpointer)
{
    GtkWindow *main = GTK_WINDOW(gtk_widget_get_toplevel(w));
    gtk_widget_hide(GTK_WIDGET(main));

    GError *err = NULL;
    GPid    pid;
    gchar  *argv[] = {"autoapp", NULL};

    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, &pid, &err)) {
        g_printerr("Failed to launch autoapp: %s\n", err->message);
        g_clear_error(&err);
        gtk_widget_show(GTK_WIDGET(main));
        return;
    }
    g_child_watch_add(pid, (GChildWatchFunc)on_autoapp_child_exit, main);
}

/* ------------------------------------------------------------------ */
/*  Vehicle Info & Settings                                           */
/* ------------------------------------------------------------------ */
static void on_vehicle_info_button_clicked(GtkWidget *w, gpointer)
{
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(w));
    GtkWidget *vw = create_vehicle_info_window(parent);
    gtk_widget_show_all(vw);
}

static void on_settings_button_clicked(GtkWidget *w, gpointer)
{
    GtkWindow *top = GTK_WINDOW(gtk_widget_get_toplevel(w));
    open_settings_window(top);
}
