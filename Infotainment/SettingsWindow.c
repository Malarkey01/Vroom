/* =========================================================================
 *  SettingsWindow.c — modal dialog for audio + back-light settings
 * -------------------------------------------------------------------------
 *  Widgets
 *  -------
 *      Back button           (top-left)
 *      Brightness slider     (row 0)
 *      Audio sink combo box  (row 1)
 *      Volume slider         (row 2)
 *
 *  Behaviour
 *  ---------
 *      • Sliders write through to AudioManager / BacklightManager.
 *      • Combo sets the default PulseAudio sink and syncs the volume slider.
 *      • Esc or Back closes the window.
 *      • Cursor hidden for kiosk UX.
 * ========================================================================= */
#include "SettingsWindow.h"
#include "AudioManager.h"
#include "BacklightManager.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */
static gboolean on_key_press          (GtkWidget *, GdkEventKey *, gpointer);
static void     hide_cursor_on_realize(GtkWidget *, gpointer);
static void     on_back_clicked       (GtkButton *, gpointer);
static gboolean on_bri_released       (GtkWidget *, GdkEventButton *, gpointer);
static gboolean on_vol_released       (GtkWidget *, GdkEventButton *, gpointer);
static void on_settings_destroy(GtkWidget *, gpointer);
static void     on_sink_changed       (GtkComboBoxText *, gpointer);

static GtkWidget *create_img_button   (const char *path, int w, int h);

/* Slider update helpers for the rotary encoder */
typedef struct { int value; } IntVal;
static gboolean update_bri_idle(gpointer);
static gboolean update_vol_idle(gpointer);

/* Widget pointers the rotary code can poke */
static GtkWidget *g_bri_scale = NULL;
static GtkWidget *g_vol_scale = NULL;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */
void open_settings_window(GtkWindow *parent)
{
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Settings");
    gtk_window_fullscreen(GTK_WINDOW(win));
    gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);

    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(win, "realize",        G_CALLBACK(hide_cursor_on_realize), NULL);

    /* Main vertical box */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    /* Back button ---------------------------------------------------- */
    GtkWidget *back = create_img_button("images/Back.png", 100, 100);
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_clicked), win);
    gtk_widget_set_halign(back, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), back, FALSE, FALSE, 0);

    /* Grid for controls --------------------------------------------- */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing   (GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 10);

    /* Row 0 — Brightness slider ------------------------------------- */
    GtkWidget *lbl_bri = gtk_label_new("Brightness");
    gtk_widget_set_name(lbl_bri, "custom-label");
    gtk_grid_attach(GTK_GRID(grid), lbl_bri, 0, 0, 1, 1);

    GtkWidget *bri_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                    0, 100, 1);
    gtk_widget_set_name(bri_scale, "brightness-scale");
    gtk_widget_set_size_request(bri_scale, 600, -1);
    int bri_raw = read_backlight_brightness();
    gtk_range_set_value(GTK_RANGE(bri_scale), bri_raw * 100.0 / 31.0);
    gtk_widget_add_events(bri_scale, GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(bri_scale, "button-release-event",
                     G_CALLBACK(on_bri_released), NULL);
    gtk_grid_attach(GTK_GRID(grid), bri_scale, 1, 0, 1, 1);
    g_bri_scale = bri_scale;   /* save for rotary updates */

    /* Row 1 — Audio sink combo -------------------------------------- */
    GtkWidget *lbl_sink = gtk_label_new("Audio Output");
    gtk_widget_set_name(lbl_sink, "custom-label");
    gtk_grid_attach(GTK_GRID(grid), lbl_sink, 0, 1, 1, 1);

    GtkWidget *combo = gtk_combo_box_text_new();
    GSList *sinks = get_audio_sinks();
    for (GSList *l = sinks; l; l = l->next)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), l->data);
    g_slist_free_full(sinks, g_free);
    g_signal_connect(combo, "changed", G_CALLBACK(on_sink_changed), bri_scale);
    gtk_grid_attach(GTK_GRID(grid), combo, 1, 1, 1, 1);

    /* Row 2 — Volume slider ----------------------------------------- */
    GtkWidget *lbl_vol = gtk_label_new("Volume");
    gtk_widget_set_name(lbl_vol, "custom-label");
    gtk_grid_attach(GTK_GRID(grid), lbl_vol, 0, 2, 1, 1);

    GtkWidget *vol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                    0, 100, 1);
    gtk_widget_set_name(vol_scale, "volume-scale");
    gtk_widget_set_size_request(vol_scale, 600, -1);
    const char *sink = get_current_sink();
    int vol = sink ? get_sink_volume_percent(sink) : 0;
    gtk_range_set_value(GTK_RANGE(vol_scale), vol);
    gtk_widget_add_events(vol_scale, GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(vol_scale, "button-release-event",
                     G_CALLBACK(on_vol_released), NULL);
    gtk_grid_attach(GTK_GRID(grid), vol_scale, 1, 2, 1, 1);
    g_vol_scale = vol_scale;

    gtk_widget_show_all(win);
    /* When the window is destroyed, invalidate the global slider handles */
    g_signal_connect(win, "destroy", G_CALLBACK(on_settings_destroy), NULL);
}

/* ------------------------------------------------------------------ */
/*  Rotary-encoder helpers                                            */
/* ------------------------------------------------------------------ */
void settings_update_brightness_slider(int raw_0_31)
{
    IntVal *d = g_new(IntVal, 1); d->value = raw_0_31;
    g_idle_add(update_bri_idle, d);
}
void settings_update_volume_slider(int pct_0_100)
{
    IntVal *d = g_new(IntVal, 1); d->value = pct_0_100;
    g_idle_add(update_vol_idle, d);
}

/* ------------------------------------------------------------------ */
/*  Internal callbacks                                                */
/* ------------------------------------------------------------------ */
static gboolean update_bri_idle(gpointer data)
{
    if (g_bri_scale && GTK_IS_RANGE(g_bri_scale))
        gtk_range_set_value(GTK_RANGE(g_bri_scale),
            ((IntVal*)data)->value * 100.0 / 31.0);   /* 0-31  →  0-100 % */
    g_free(data);
    return G_SOURCE_REMOVE;
}

/* Volume slider from rotary encoder */
static gboolean update_vol_idle(gpointer data)
{
    if (g_vol_scale && GTK_IS_RANGE(g_vol_scale))
        gtk_range_set_value(GTK_RANGE(g_vol_scale),
            (gdouble)((IntVal*)data)->value);         /* already 0-100 % */
    g_free(data);
    return G_SOURCE_REMOVE;
}

/* ------------------------------------------------------------------ */
/*  Settings window destroy handler                              */
/* ------------------------------------------------------------------ */
static void on_settings_destroy(GtkWidget *, gpointer)
{
    /* Widgets are gone — stop the rotary callbacks from touching them */
    g_bri_scale = NULL;
    g_vol_scale = NULL;
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer)
{
    if (e->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(w);
        return TRUE;
    }
    return FALSE;
}

static void hide_cursor_on_realize(GtkWidget *w, gpointer)
{
    GdkWindow *gw = gtk_widget_get_window(w);
    GdkDisplay *d = gdk_window_get_display(gw);

    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
    gdk_pixbuf_fill(pb, 0);
    // GdkCursor *c = gdk_cursor_new_from_pixbuf(d, pb, 0, 0);
    // gdk_window_set_cursor(gw, c);
    // g_object_unref(c);
    GdkCursor *c = gdk_cursor_new_from_pixbuf(d, pb, 0, 0);
    gdk_window_set_cursor(gw, c);
    g_object_unref(pb);
}

static void on_back_clicked(GtkButton *, gpointer win)
{ gtk_widget_destroy(GTK_WIDGET(win)); }

/* Brightness slider → BacklightManager */
static gboolean on_bri_released(GtkWidget *s, GdkEventButton *, gpointer)
{
    double pct = gtk_range_get_value(GTK_RANGE(s));
    int raw = (int)(pct * 31.0 / 100.0 + 0.5);
    set_backlight_brightness(raw);
    return FALSE;
}

/* Volume slider → AudioManager */
static gboolean on_vol_released(GtkWidget *s, GdkEventButton *, gpointer)
{
    const char *sink = get_current_sink();
    if (sink) {
        int val = (int)(gtk_range_get_value(GTK_RANGE(s)) + 0.5);
        set_sink_volume_percent(sink, val);
    }
    return FALSE;
}

/* Sink combo → set default + refresh volume slider */
static void on_sink_changed(GtkComboBoxText *c, gpointer vol_scale)
{
    gchar *sink = gtk_combo_box_text_get_active_text(c);
    if (!sink) return;

    set_default_sink(sink);
    g_free(sink);

    const char *cur = get_current_sink();
    if (cur) {
        int v = get_sink_volume_percent(cur);
        gtk_range_set_value(GTK_RANGE(vol_scale), v);
    }
}

/* ------------------------------------------------------------------ */
/*  Small helper: image button                                        */
/* ------------------------------------------------------------------ */
static GtkWidget *create_img_button(const char *path, int w, int h)
{
    GtkWidget *btn = gtk_button_new();
    GdkPixbuf *pb  = gdk_pixbuf_new_from_file_at_scale(path, w, h, TRUE, NULL);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    g_object_unref(pb);
    gtk_button_set_image(GTK_BUTTON(btn), img);
    gtk_button_set_always_show_image(GTK_BUTTON(btn), TRUE);
    return btn;
}
