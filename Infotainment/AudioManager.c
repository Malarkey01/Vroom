/* =========================================================================
 *  AudioManager.c — PulseAudio utility layer for Vroom Infotainment
 * ========================================================================= */
#include "AudioManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 *  g_current_sink
 *  ------------------------------------------------------------------------
 *  Keeps track of the last sink chosen by set_default_sink() so that the UI
 *  can query or modify it without reparsing `pactl info` each time.
 * ------------------------------------------------------------------------- */
static char *g_current_sink = NULL;

/* ------------------------------------------------------------------------- */
GSList *get_audio_sinks(void)
/* -------------------------------------------------------------------------
 *  Parse `pactl list short sinks` and return a GSList of sink names.
 * ------------------------------------------------------------------------- */
{
    GSList *sink_list = NULL;
    FILE   *fp = popen("pactl list short sinks", "r");
    if (!fp) {
        g_printerr("AudioManager: cannot list sinks.\n");
        return NULL;
    }

    char line[256];
    while (fgets(line, sizeof line, fp)) {
        /* Format: <index>\t<name>\t... — we want the <name> field */
        char *tab = strchr(line, '\t');
        if (!tab) continue;

        char *sink_name = tab + 1;
        if ((tab = strchr(sink_name, '\t'))) *tab = '\0';
        g_strchomp(sink_name);                     /* trim newline */
        sink_list = g_slist_append(sink_list, g_strdup(sink_name));
    }
    pclose(fp);
    return sink_list;
}

/* ------------------------------------------------------------------------- */
void set_default_sink(const char *sink_name)
/* -------------------------------------------------------------------------
 *  Sets the PulseAudio default sink and records it locally.
 * ------------------------------------------------------------------------- */
{
    if (!sink_name) return;

    /* Remember the new sink name */
    free(g_current_sink);
    g_current_sink = strdup(sink_name);

    /* Tell PulseAudio */
    char *cmd = g_strdup_printf("pactl set-default-sink %s", sink_name);
    system(cmd);
    g_free(cmd);
}

/* ------------------------------------------------------------------------- */
const char *get_current_sink(void)
/* -------------------------------------------------------------------------
 *  Returns the cached sink name, or NULL if none has been set yet.
 * ------------------------------------------------------------------------- */
{
    return g_current_sink;
}

/* ------------------------------------------------------------------------- */
int get_sink_volume_percent(const char *sink_name)
/* -------------------------------------------------------------------------
 *  Reads the volume percentage of the given sink via `pactl`.
 * ------------------------------------------------------------------------- */
{
    if (!sink_name) return -1;

    char *cmd = g_strdup_printf("pactl get-sink-volume %s", sink_name);
    FILE *fp  = popen(cmd, "r");
    g_free(cmd);

    if (!fp) {
        g_printerr("AudioManager: cannot read sink volume.\n");
        return -1;
    }

    char line[256];
    int  volume = -1;
    while (fgets(line, sizeof line, fp)) {
        /* Look for a “NN%” token and grab the digits */
        char *pct = strchr(line, '%');
        if (!pct) continue;

        char *start = pct;
        while (start > line && isdigit((unsigned char)*(start - 1)))
            --start;

        volume = atoi(start);
        break;
    }
    pclose(fp);
    return volume;
}

/* ------------------------------------------------------------------------- */
void set_sink_volume_percent(const char *sink_name, int volume)
/* -------------------------------------------------------------------------
 *  Clamps the volume to 0-100 % and applies it with `pactl`.
 * ------------------------------------------------------------------------- */
{
    if (!sink_name) return;
    if (volume < 0)   volume = 0;
    if (volume > 100) volume = 100;

    char *cmd = g_strdup_printf("pactl set-sink-volume %s %d%%",
                                sink_name, volume);
    system(cmd);
    g_free(cmd);
}
