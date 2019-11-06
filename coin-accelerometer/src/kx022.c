#include "kx022.h"
#include "kx022_regs.h"

#include <logging/log.h>

LOG_MODULE_REGISTER(kx022);

void kx022_init(struct device *dev, u16_t addr) {
    // go into setup mode
    i2c_reg_write_byte(dev, addr, KX022_REG_CNTL1, 0);
    // set output data rate to 12.5Hz
    i2c_reg_write_byte(dev, addr, KX022_REG_ODCNTL, 0);
    // enable INT1 non-latching active-low
    i2c_reg_write_byte(dev, addr, KX022_REG_INC1, KX022_BIT_IEL1 | KX022_BIT_IEN1);
    // enable INT1 on data ready
    i2c_reg_write_byte(dev, addr, KX022_REG_INC4, KX022_BIT_DRDYI1);
    // set low-power (8-Bit resolution), sensitivity and enable sensor
    i2c_reg_write_byte(dev, addr, KX022_REG_CNTL1, KX022_BIT_PC1 | KX022_BIT_GSEL1 | KX022_BIT_RES);
    k_sleep(250);
}

bool kx022_verify_id(struct device *dev, u16_t addr) {
    u8_t id = 0;
    //read who-am-i value
    i2c_reg_read_byte(dev, addr, KX022_REG_Who_AM_I, &id);
    return KX022_WHO_AM_I_VAL == id;
}

void kx022_read_acc(struct device *dev, u16_t addr, u8_t *output) {
    i2c_burst_read(dev, addr, KX022_REG_XOUTL, output, 6);
}