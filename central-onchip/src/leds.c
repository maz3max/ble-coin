#include "leds.h"
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>

#define LED0_PORT DT_ALIAS_LED0_GPIOS_CONTROLLER
#define LED0 DT_ALIAS_LED0_GPIOS_PIN
#define LED1R_PORT DT_GPIO_LEDS_LED1_RED_GPIOS_CONTROLLER
#define LED1R DT_GPIO_LEDS_LED1_RED_GPIOS_PIN
#define LED1G_PORT DT_GPIO_LEDS_LED1_GREEN_GPIOS_CONTROLLER
#define LED1G DT_GPIO_LEDS_LED1_GREEN_GPIOS_PIN
#define LED1B_PORT DT_GPIO_LEDS_LED1_BLUE_GPIOS_CONTROLLER
#define LED1B DT_GPIO_LEDS_LED1_BLUE_GPIOS_PIN

void leds_init() {
    // LED0 SETUP
    struct device *dev = device_get_binding(LED0_PORT);
    gpio_pin_configure(dev, LED0, GPIO_DIR_OUT);
    gpio_pin_write(dev, LED0, 1);
    // LED1 SETUP
    dev = device_get_binding(LED1R_PORT);
    gpio_pin_configure(dev, LED1R, GPIO_DIR_OUT);
    gpio_pin_write(dev, LED1R, 1);
    dev = device_get_binding(LED1G_PORT);
    gpio_pin_configure(dev, LED1G, GPIO_DIR_OUT);
    gpio_pin_write(dev, LED1G, 1);
    dev = device_get_binding(LED1B_PORT);
    gpio_pin_configure(dev, LED1B, GPIO_DIR_OUT);
    gpio_pin_write(dev, LED1B, 1);
}

void led0_set(uint8_t on) {
    struct device *dev = device_get_binding(LED0_PORT);
    gpio_pin_write(dev, LED0, 1 - on);
}

void led1_set(uint8_t r_on, uint8_t b_on, uint8_t g_on) {
    struct device *dev = device_get_binding(LED1R_PORT);
    gpio_pin_write(dev, LED1R, 1 - r_on);
    dev = device_get_binding(LED1G_PORT);
    gpio_pin_write(dev, LED1G, 1 - b_on);
    dev = device_get_binding(LED1B_PORT);
    gpio_pin_write(dev, LED1B, 1 - g_on);
}
