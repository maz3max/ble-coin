#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum blink_state_t {
    BI_ON = 0,
    BI_AGGRESSIVE,
    BI_QUICK,
    BI_SLOW,
    BI_OFF,
    BI_SOS
} blink_state_t;

/**
 * Initializes the LED and the Button (with an empty callback function just for wakeup).
 * Also sets up a timer for use for the blinking function.
 */
void io_init();

/**
 * Sets the interval of the LED blinking function.
 * @param intensity
 */
void set_blink_intensity(blink_state_t intensity);

#ifdef __cplusplus
}
#endif
