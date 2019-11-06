#pragma once

#define KX022_ADDR_GND 0x1e
#define KX022_ADDR_VCC 0x1f

#include <zephyr.h>
#include <drivers/i2c.h>

// initialize sensor
void kx022_init(struct device *dev, u16_t addr);
// returns true if who-am-i register has expected value
bool kx022_verify_id(struct device *dev, u16_t addr);

// output has to be 6 bytes long
void kx022_read_acc(struct device *dev, u16_t addr, u8_t *output);