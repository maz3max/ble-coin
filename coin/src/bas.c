#include <zephyr.h>

#include <bluetooth/gatt.h>

#include <device.h>
#include <gpio.h>
#include <adc.h>
#include <hal/nrf_saadc.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(bas);

static u8_t battery = 0U;
static struct device *adc_dev = NULL;

#define ADC_DEVICE_NAME DT_ADC_0_NAME
#define ADC_RESOLUTION 10
#define ADC_GAIN ADC_GAIN_1_3
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#define ADC_1ST_CHANNEL_ID 0
#define ADC_1ST_CHANNEL_INPUT NRF_SAADC_INPUT_AIN0

#define BAT_LOW 3
#define BAT_PORT DT_ALIAS_LED0_GPIOS_CONTROLLER

static const struct adc_channel_cfg m_1st_channel_cfg = {
        .gain = ADC_GAIN,
        .reference = ADC_REFERENCE,
        .acquisition_time = ADC_ACQUISITION_TIME,
        .channel_id = ADC_1ST_CHANNEL_ID,
        .input_positive = ADC_1ST_CHANNEL_INPUT,
};

static struct device *init_adc(void) {
    int ret;
    adc_dev = device_get_binding(ADC_DEVICE_NAME);

    if (!adc_dev) {
        LOG_ERR("couldn't get ADC dev binding");
    }

    ret = adc_channel_setup(adc_dev, &m_1st_channel_cfg);
    if (ret != 0) {
        LOG_ERR("adc channel config failed with code %i", ret);
    }

    return adc_dev;
}

static u8_t get_batt_percentage() {
    struct device *dev = device_get_binding(BAT_PORT);
    gpio_pin_configure(dev, BAT_LOW, GPIO_DIR_OUT);
    gpio_pin_write(dev, BAT_LOW, 0);
    nrf_saadc_value_t val = 0;
    if (!adc_dev) {
        LOG_ERR("OH NOES! NO ADC AVAILABLE!");
    } else {
        int ret;
        const struct adc_sequence sequence = {
                .channels = BIT(ADC_1ST_CHANNEL_ID),
                .buffer = &val,
                .buffer_size = sizeof(nrf_saadc_value_t),
                .resolution = ADC_RESOLUTION,
                .oversampling = true,
        };
        ret = adc_read(adc_dev, &sequence);
        if (ret != 0) {
            LOG_ERR("ADC read failed with code %i", ret);
        }
    }
    gpio_pin_configure(dev, BAT_LOW, GPIO_DIR_IN);

    LOG_INF("read ADC val: %i", val);

    float batt_percentage_f = 0.3515625f * (float) val - 200;
    u8_t batt_percentage =
            batt_percentage_f < 0 ? 0 :
            batt_percentage_f > 100 ? 100 :
            (u8_t) batt_percentage_f;
    return batt_percentage;
}

static ssize_t read_blvl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, u16_t len, u16_t offset) {
    battery = get_batt_percentage();

    const char *value = (const char *) &battery;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
                             sizeof(*value));
}

/* Battery Service Declaration */
BT_GATT_SERVICE_DEFINE(bas_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
                       BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL,
                                              BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_READ, read_blvl, NULL, &battery),
);

uint8_t bas_init() {
    LOG_INF("initialize battery service");
    init_adc();
    battery = get_batt_percentage();
    return battery;
}
