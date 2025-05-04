/* =========================================================================
 *  VehicleInfoWindow.c — fullscreen GTK window for live car data
 * -------------------------------------------------------------------------
 *  • Spawns the helper script (see SCRIPT_PATH) and reads one-line JSON
 *    frames (≤4 KiB ⇒ atomic pipe writes).
 *  • Updates eight value labels and a status label in real time.
 *  • Tracks best / worst inter-frame latency, printing milestones to stdout.
 *  • Retries the script every RETRY_INTERVAL_SEC until a connection is made.
 * ========================================================================= */
#include "VehicleInfoWindow.h"

#include <json-glib/json-glib.h>
#include <gdk/gdkkeysyms.h>
#include <float.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Settings                                                          */
/* ------------------------------------------------------------------ */
static const char SCRIPT_PATH[]      = "Infotainment/scripts/obd_reader.py";
static const int  RETRY_INTERVAL_SEC = 10;

static const char *columns[] = {
    "RPM", "SPEED", "ENGINE LOAD",
    "THROTTLE POSITION", "INTAKE PRESSURE",
    "TIMING ADVANCE", "FUEL LEVEL", "CONTROL MODULE VOLTAGE"
};

/* ------------------------------------------------------------------ */
/*  Context                                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    GtkWidget  *value_lbls[G_N_ELEMENTS(columns)];
    GtkWidget  *status_label;
    gboolean    connected;

    GPid        pid;          /* child PID */
    GIOChannel *io;           /* child's stdout */
    guint       retry_tag;

    gint64   start_time;
    gint64   last_time;
    gdouble  best_delta;
    gdouble  worst_delta;
} VehicleCtx;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */
static void     set_status(VehicleCtx *ctx, gboolean ok);
static void     set_status_markup(VehicleCtx *ctx, const char *markup);
static gboolean spawn_reader(VehicleCtx *ctx);
static gboolean parse_line_cb(GIOChannel *, GIOCondition, gpointer);
static void     on_obd_child_exit(GPid, gint, gpointer);
static void     on_back_clicked(GtkWidget *, gpointer);
static gboolean on_key_press(GtkWidget *, GdkEventKey *, gpointer);
static void     on_destroy(GtkWidget *, gpointer);
static void     hide_cursor_on_realize(GtkWidget *, gpointer);

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */
GtkWidget *create_vehicle_info_window(GtkWindow *parent)
{
    VehicleCtx *ctx = g_new0(VehicleCtx, 1);
    ctx->best_delta  = DBL_MAX;

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Vehicle Info");
    gtk_window_fullscreen(GTK_WINDOW(win));
    g_object_set_data_full(G_OBJECT(win), "vctx", ctx, g_free);

    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    }

    g_signal_connect(win, "destroy",         G_CALLBACK(on_destroy),     ctx);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_key_press),   NULL);
    g_signal_connect(win, "realize",         G_CALLBACK(hide_cursor_on_realize), NULL);

    /*   Layout  */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    /* Top bar — Back & status */
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

    GtkWidget *back = gtk_button_new();
    {
        GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_scale(
            "Infotainment/images/Back.png", 100, 100, TRUE, NULL);
        GtkWidget *img = gtk_image_new_from_pixbuf(pix);
        g_object_unref(pix);
        gtk_button_set_image(GTK_BUTTON(back), img);
        gtk_button_set_always_show_image(GTK_BUTTON(back), TRUE);
    }
    gtk_widget_set_halign(back, GTK_ALIGN_START);
    gtk_widget_set_valign(back, GTK_ALIGN_START);
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_clicked), win);
    gtk_box_pack_start(GTK_BOX(bar), back, FALSE, FALSE, 0);

    ctx->status_label = gtk_label_new(NULL);
    gtk_label_set_use_markup(GTK_LABEL(ctx->status_label), TRUE);
    set_status(ctx, FALSE);  /* starts as “Connecting” */
    gtk_widget_set_halign(ctx->status_label, GTK_ALIGN_END);
    gtk_widget_set_valign(ctx->status_label, GTK_ALIGN_START);
    gtk_box_pack_end(GTK_BOX(bar), ctx->status_label, FALSE, FALSE, 10);

    /* PID grid */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 10);

    /* 
     * Make 8 rows, each with "PID Name" on the left,
     * and "Value" on the right. Modify the font size & color as you wish.
     */
    for (guint i = 0; i < G_N_ELEMENTS(columns); i++) {
        GtkWidget *key = gtk_label_new(NULL);
        gchar *km = g_strdup_printf(
            "<span font_desc='Sans 38' foreground='#FFFFFF'>%s</span>", columns[i]);
        gtk_label_set_markup(GTK_LABEL(key), km);
        g_free(km);
        gtk_widget_set_halign(key, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), key, 0, i, 1, 1);

        ctx->value_lbls[i] = gtk_label_new(
            "<span font_desc='Sans 38' foreground='#00AAFF'>--</span>");
        gtk_widget_set_halign(ctx->value_lbls[i], GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(grid), ctx->value_lbls[i], 1, i, 1, 1);
    }

    /* Kick off the Python helper */
    spawn_reader(ctx);

    return win;
}

/* ------------------------------------------------------------------ */
/*  Status helpers                                                    */
/* ------------------------------------------------------------------ */
static void set_status(VehicleCtx *ctx, gboolean ok)
{
    if (ok)
        /* Connected in green*/
        set_status_markup(ctx,
            "<span font_desc='Sans 38' foreground='#00FF00'>Connected</span>");
    else
        /* Back to "Connecting" in bluish*/
        set_status_markup(ctx,
            "<span font_desc='Sans 38' foreground='#00DDFF'>Connecting</span>");
    ctx->connected = ok;
}

static void set_status_markup(VehicleCtx *ctx, const char *markup)
{
    if (ctx && GTK_IS_LABEL(ctx->status_label))
        gtk_label_set_markup(GTK_LABEL(ctx->status_label), markup);
}

/* ------------------------------------------------------------------ */
/*  Spawn & I/O                                                       */
/* ------------------------------------------------------------------ */
static gboolean spawn_reader(VehicleCtx *ctx)
{
    if (ctx->io) return G_SOURCE_REMOVE;            /* already running */

    gint stdout_fd = -1;
    gchar *argv[] = {"python3", (gchar *)SCRIPT_PATH, NULL};

    if (!g_spawn_async_with_pipes(
            NULL, argv, NULL,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
            NULL, NULL, &ctx->pid,
            NULL, &stdout_fd, NULL, NULL))
    {
        g_printerr("[OBD] Failed to spawn helper.\n");
        ctx->retry_tag = g_timeout_add_seconds(RETRY_INTERVAL_SEC,
                                               (GSourceFunc)spawn_reader, ctx);
        return G_SOURCE_REMOVE;
    }

    g_child_watch_add(ctx->pid, on_obd_child_exit, ctx);

    ctx->io = g_io_channel_unix_new(stdout_fd);
    g_io_channel_set_encoding(ctx->io, NULL, NULL);     /* raw bytes */
    g_io_add_watch(ctx->io, G_IO_IN | G_IO_HUP | G_IO_ERR, parse_line_cb, ctx);

    return G_SOURCE_REMOVE;
}

static gboolean parse_line_cb(GIOChannel *ch, GIOCondition cond, gpointer data)
{
    VehicleCtx *ctx = data;

    if (cond & (G_IO_HUP | G_IO_ERR)) {
        set_status(ctx, FALSE);
        if (ctx->io) {
            g_io_channel_shutdown(ctx->io, FALSE, NULL);
            g_io_channel_unref(ctx->io);
            ctx->io = NULL;
        }
        if (!ctx->retry_tag)
            ctx->retry_tag = g_timeout_add_seconds(
                RETRY_INTERVAL_SEC, (GSourceFunc)spawn_reader, ctx);
        return G_SOURCE_REMOVE;
    }

    gchar *line = NULL;
    gsize  len  = 0;
    if (g_io_channel_read_line(ch, &line, &len, NULL, NULL)
            != G_IO_STATUS_NORMAL || !line)
    {
        g_free(line);
        return TRUE;                                     /* wait for more */
    }

    JsonParser *jp = json_parser_new();
    if (json_parser_load_from_data(jp, line, len, NULL)) {
        JsonObject *obj = json_node_get_object(json_parser_get_root(jp));

        /* mark connection */
        if (json_object_get_size(obj) > 0 && !ctx->connected)
            set_status(ctx, TRUE);

        for (guint i = 0; i < G_N_ELEMENTS(columns); i++) {
            if (!json_object_has_member(obj, columns[i])) continue;
            double v = json_object_get_double_member(obj, columns[i]);
            gchar *txt = NULL;
            switch (i) {
                case 0:  txt = g_strdup_printf("%.0f",     v);                  break; /* RPM */
                case 1:  txt = g_strdup_printf("%.0f mph", v * 0.621371);        break; /* SPEED */
                case 4:  txt = g_strdup_printf("%.0f kPa", v);                   break; /* PRESSURE */
                case 5:  txt = g_strdup_printf("%.1f°",    v);                   break; /* ADVANCE */
                case 7:  txt = g_strdup_printf("%.1f V",   v);                   break; /* VOLTAGE */
                default: txt = g_strdup_printf("%.1f %%",  v);                   break;
            }
            gchar *markup = g_strdup_printf(
                "<span font_desc='Sans 38' foreground='#00AAFF'>%s</span>", txt);
            gtk_label_set_markup(GTK_LABEL(ctx->value_lbls[i]), markup);
            g_free(markup);
            g_free(txt);
        }
    }
    g_object_unref(jp);
    g_free(line);

    /* ── latency stats ── */
    gint64 now = g_get_monotonic_time();
    if (ctx->last_time) {
        gdouble delta = (now - ctx->last_time) / 1e6;
        if (delta < ctx->best_delta) {
            ctx->best_delta = delta;
            g_print("[OBD] new best  %.3f ms\n", delta * 1e3);
        }
        if (delta > ctx->worst_delta) {
            ctx->worst_delta = delta;
            g_print("[OBD] new worst %.3f ms\n", delta * 1e3);
        }
    } else {
        ctx->start_time = now;
    }
    ctx->last_time = now;
    return TRUE;
}

static void on_obd_child_exit(GPid pid, gint, gpointer data)
{
    VehicleCtx *ctx = data;
    g_spawn_close_pid(pid);
    ctx->pid = 0;
    if (!ctx->retry_tag)
        ctx->retry_tag = g_timeout_add_seconds(
            RETRY_INTERVAL_SEC, (GSourceFunc)spawn_reader, ctx);
}

static void on_back_clicked(GtkWidget *, gpointer win)
{ gtk_widget_destroy(GTK_WIDGET(win)); }

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer)
{
    if (e->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(w);
        return TRUE;
    }
    return FALSE;
}

static void on_destroy(GtkWidget *w, gpointer data)
{
    VehicleCtx *ctx = data;
    if (ctx->pid)  kill(ctx->pid, SIGTERM);
    if (ctx->io)   g_io_channel_unref(ctx->io);
    if (ctx->retry_tag) g_source_remove(ctx->retry_tag);

    /* Session summary */
    if (ctx->start_time && ctx->last_time)
        g_print("[OBD] session %.1f s   best %.3f ms   worst %.3f ms\n",
                (ctx->last_time - ctx->start_time) / 1e6,
                ctx->best_delta * 1e3, ctx->worst_delta * 1e3);
}

static void hide_cursor_on_realize(GtkWidget *w, gpointer)
{
    GdkWindow *gw = gtk_widget_get_window(w);
    GdkDisplay *d = gdk_window_get_display(gw);
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
    gdk_pixbuf_fill(pb, 0);
    GdkCursor *c = gdk_cursor_new_from_pixbuf(d, pb, 0, 0);
    gdk_window_set_cursor(gw, c);
    g_object_unref(c);
    g_object_unref(pb);
}
