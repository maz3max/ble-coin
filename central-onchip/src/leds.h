#pragma once

#include <stdint.h>

/**
 * initialize LEDs
 */
void leds_init();

void led0_set(uint8_t on);

void led1_set(uint8_t r_on, uint8_t b_on, uint8_t g_on);