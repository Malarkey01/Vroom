/* =========================================================================
 *  RotaryEncoder.h - GPIO driven quadrature knob with push-button
 * -------------------------------------------------------------------------
 *  start_rotary_thread()
 *      Performs a one-shot GPIO setup:
 *          • configures A, B and SW pins as inputs with pull-ups
 *          • attaches wiringPi edge-triggered ISRs for rotation and press
 *      Returns immediately — the rest of the work happens inside the
 *      interrupt handlers, so no additional threads are spawned.
 * ========================================================================= */
#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H

void start_rotary_thread(void);

#endif /* ROTARYENCODER_H */
