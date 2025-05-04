/* =========================================================================
 *  AudioManager.h — simple PulseAudio helper routines
 * -------------------------------------------------------------------------
 *  All functions here rely on the `pactl` CLI tool that ships with
 *  PulseAudio.  They are blocking wrappers intended for UI-driven use
 *  (Settings window, rotary encoder, etc.).
 * ========================================================================= */
#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <glib.h>   /* GSList */

/* -------------------------------------------------------------------------
 *  get_audio_sinks
 *  ------------------------------------------------------------------------
 *  Returns a newly-allocated GSList of `char*` sink names discovered via
 *  `pactl list short sinks`.  The caller is responsible for freeing each
 *  string as well as the list itself with
 *      g_slist_free_full(list, g_free);
 * ------------------------------------------------------------------------- */
GSList *get_audio_sinks(void);

/* -------------------------------------------------------------------------
 *  set_default_sink
 *  ------------------------------------------------------------------------
 *  Makes the given PulseAudio sink the default output device and records
 *  its name so that other helper routines can reuse it quickly.
 * ------------------------------------------------------------------------- */
void set_default_sink(const char *sink_name);

/* -------------------------------------------------------------------------
 *  audio_manager_init
 *  ------------------------------------------------------------------------
 *  Discover the current PulseAudio *default sink* at startup
 * ------------------------------------------------------------------------- */
void audio_manager_init(void);

/* -------------------------------------------------------------------------
 *  get_current_sink
 *  ------------------------------------------------------------------------
 *  Returns the sink name that was most recently passed to
 *  set_default_sink().  The string is owned by the AudioManager and must
 *  not be freed or modified by the caller.
 * ------------------------------------------------------------------------- */
const char *get_current_sink(void);

/* -------------------------------------------------------------------------
 *  get_sink_volume_percent
 *  ------------------------------------------------------------------------
 *  Reads the volume of the specified sink (0-100 %) using
 *      pactl get-sink-volume <sink>
 *  Returns −1 on error.
 * ------------------------------------------------------------------------- */
int get_sink_volume_percent(const char *sink_name);

/* -------------------------------------------------------------------------
 *  set_sink_volume_percent
 *  ------------------------------------------------------------------------
 *  Sets the volume of the specified sink to the given percentage
 *  (values are clamped to 0-100 %) with
 *      pactl set-sink-volume <sink> <n>%
 * ------------------------------------------------------------------------- */
void set_sink_volume_percent(const char *sink_name, int volume);

#endif /* AUDIOMANAGER_H */
