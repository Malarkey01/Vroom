/* =========================================================================
 *  RotaryEncoder.c — interrupt-based knob for volume / brightness
 * -------------------------------------------------------------------------
 *  • A/B pins form a quadrature encoder; SW pin is a momentary button
 *  • Short press toggles “Volume mode” ↔ “Brightness mode”
 *  • Long press (≥1 s) sends SIGTERM to a running autoapp instance
 *  • Rotation adjusts volume (±5 %) or brightness (±5 units)
 *    and updates both the HUD popup and the Settings sliders.
 * ========================================================================= */
#include "RotaryEncoder.h"
#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "Popup.h"
#include "AudioManager.h"
#include "BacklightManager.h"
#include "SettingsWindow.h"

/* ---------------------------------------------------------------------- */
/*  Constants                                                             */
/* ---------------------------------------------------------------------- */
static const int    VOLUME_STEP_PERCENT      = 5;
static const int    BRIGHTNESS_STEP_ABSOLUTE = 5;
static const double LONG_PRESS_THRESHOLD_SEC = 1.0;    /* 1-second hold */

static const int ROTARY_A_PIN = 8;
static const int ROTARY_B_PIN = 9;
static const int ROTARY_SW_PIN = 7;

/* ---------------------------------------------------------------------- */
/*  Module-wide state                                                     */
/* ---------------------------------------------------------------------- */
static volatile bool   g_isVolumeMode   = true;
static volatile bool   g_buttonPressed  = false;
static volatile time_t g_pressTimestamp = 0;

static volatile uint8_t lastAB = 0;
static volatile int8_t  qAcc   = 0;

/* Forward declarations for ISRs */
static void rotary_isr (void);
static void button_isr (void);

/* ---------------------------------------------------------------------- */
/*  Public API                                                            */
/* ---------------------------------------------------------------------- */
void start_rotary_thread(void)
{
    if (wiringPiSetup() < 0) {
        fprintf(stderr, "[Rotary] wiringPiSetup() failed.\n");
        return;
    }

    pinMode(ROTARY_A_PIN,  INPUT);
    pinMode(ROTARY_B_PIN,  INPUT);
    pinMode(ROTARY_SW_PIN, INPUT);

    pullUpDnControl(ROTARY_A_PIN,  PUD_UP);
    pullUpDnControl(ROTARY_B_PIN,  PUD_UP);
    pullUpDnControl(ROTARY_SW_PIN, PUD_UP);

    lastAB = (digitalRead(ROTARY_A_PIN) << 1) | digitalRead(ROTARY_B_PIN);

    if (wiringPiISR(ROTARY_A_PIN,  INT_EDGE_BOTH,  rotary_isr) < 0 ||
        wiringPiISR(ROTARY_B_PIN,  INT_EDGE_BOTH,  rotary_isr) < 0 ||
        wiringPiISR(ROTARY_SW_PIN, INT_EDGE_BOTH,  button_isr)  < 0)
    {
        fprintf(stderr, "[Rotary] Failed to attach one or more ISRs.\n");
    }
}

/* ---------------------------------------------------------------------- */
/*  Helpers                                                               */
/* ---------------------------------------------------------------------- */
static inline void change_volume(int delta)
{
    const char *sink = get_current_sink();
    if (!sink) return;

    int oldVol = get_sink_volume_percent(sink);
    if (oldVol < 0) return;

    set_sink_volume_percent(sink, oldVol + delta);

    int finalVol = get_sink_volume_percent(sink);
    if (finalVol >= 0)
        settings_update_volume_slider(finalVol);
}

static inline void change_brightness(int delta)
{
    int oldBri = read_backlight_brightness();
    set_backlight_brightness(oldBri + delta);

    int finalBri = read_backlight_brightness();
    settings_update_brightness_slider(finalBri);
}

/* ---------------------------------------------------------------------- */
/*  Quadrature ISR                                                        */
/* ---------------------------------------------------------------------- */
static void rotary_isr(void)
{
    uint8_t curAB      = (digitalRead(ROTARY_A_PIN) << 1) | digitalRead(ROTARY_B_PIN);
    uint8_t transition = (lastAB << 2) | curAB;

    int8_t dir = 0;
    switch (transition) {
        case 0x1: case 0x7: case 0xE: case 0x8: dir = +1; break;
        case 0x2: case 0xB: case 0xD: case 0x4: dir = -1; break;
        default: break;
    }

    if (dir != 0)
        qAcc += dir;

    if (qAcc <= -4) {
        qAcc = 0;
        if (g_isVolumeMode)
            change_volume(+VOLUME_STEP_PERCENT);
        else
            change_brightness(+BRIGHTNESS_STEP_ABSOLUTE);
    }
    else if (qAcc >= 4) {
        qAcc = 0;
        if (g_isVolumeMode)
            change_volume(-VOLUME_STEP_PERCENT);
        else
            change_brightness(-BRIGHTNESS_STEP_ABSOLUTE);
    }

    lastAB = curAB;
}

/* ---------------------------------------------------------------------- */
/*  Push-button ISR                                                       */
/* ---------------------------------------------------------------------- */
static void button_isr(void)
{
    int level = digitalRead(ROTARY_SW_PIN);

    /* Release ------------------------------------------------------------ */
    if (level == HIGH && g_buttonPressed) {
        g_buttonPressed = false;
        double held = difftime(time(NULL), g_pressTimestamp);

        if (held >= LONG_PRESS_THRESHOLD_SEC) {
            /* Long press → kill Android Auto if running */
            if (system("pgrep -x autoapp >/dev/null") == 0)
                system("pkill -TERM autoapp");
        } else {
            /* Short press → toggle mode + HUD popup */
            g_isVolumeMode = !g_isVolumeMode;
            show_temp_popup(g_isVolumeMode ? "Volume" : "Brightness");
        }
    }
    /* Press -------------------------------------------------------------- */
    else if (level == LOW && !g_buttonPressed) {
        g_buttonPressed  = true;
        g_pressTimestamp = time(NULL);
    }
}
