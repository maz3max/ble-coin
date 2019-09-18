#include "io.h"
#include <gpio.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(io);

#define LED_PORT DT_GPIO_LEDS_LED0_GPIO_CONTROLLER
#define LED DT_GPIO_LEDS_LED0_GPIO_PIN
#define BTN DT_GPIO_KEYS_SW0_GPIO_PIN

static struct gpio_callback btn_cb = {0};

static struct device *dev;

struct k_timer blink_timer;

static const int aggressive_blink_ms = 125;
static const int quick_blink_ms = 250;
static const int slow_blink_ms = 500;

void set_blink_intensity(blink_state_t intensity) {
    switch (intensity) {
        case BI_ON:
            LOG_INF("turn LED on");
            k_timer_stop(&blink_timer);
            gpio_pin_write(dev, LED, 1);
            break;
        case BI_AGGRESSIVE:
            LOG_INF("aggressive LED blink");
            k_timer_start(&blink_timer, K_MSEC(aggressive_blink_ms), K_MSEC(aggressive_blink_ms));
            break;
        case BI_QUICK:
            LOG_INF("quick LED blink");
            k_timer_start(&blink_timer, K_MSEC(quick_blink_ms), K_MSEC(quick_blink_ms));
            break;
        case BI_SLOW:
            LOG_INF("slow LED blink");
            k_timer_start(&blink_timer, K_MSEC(slow_blink_ms), K_MSEC(slow_blink_ms));
            break;
        default:
        case BI_OFF:
            LOG_INF("turn LED off");
            k_timer_stop(&blink_timer);
            gpio_pin_write(dev, LED, 0);
            break;
    }
}

static int blink_counter = 0;

static void blink_expiry_function(struct k_timer *timer_id) {
    gpio_pin_write(dev, LED, (blink_counter++) % 2);
}

static void button_pressed(struct device *dev, struct gpio_callback *cb, u32_t pins) {
    //LOG_INF("button pressed");
}

void io_init() {
    LOG_INF("initialize LED and button");
    dev = device_get_binding(LED_PORT);

    gpio_pin_configure(dev, LED, GPIO_DIR_OUT);
    gpio_pin_configure(dev, BTN,
                       GPIO_DIR_IN | GPIO_PUD_PULL_UP | GPIO_INT_LEVEL |
                       GPIO_INT | GPIO_INT_ACTIVE_LOW);
    gpio_init_callback(&btn_cb, button_pressed, BIT(BTN));
    gpio_add_callback(dev, &btn_cb);
    gpio_pin_enable_callback(dev, BTN);

    k_timer_init(&blink_timer, blink_expiry_function, NULL);
    set_blink_intensity(BI_ON);
}
