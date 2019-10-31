#include "kx022.h"
#include "kx022_regs.h"

void kx022_init(struct device *dev, u16_t addr){
	// go into setup mode
	i2c_reg_write_byte(dev, addr, KX022_REG_CNTL1, 0);
	// set ouput data rate to 12.5Hz
	i2c_reg_write_byte(dev, addr, KX022_REG_ODCNTL, 0);
	// set low-power (8-Bit sensitivity), resolution and enable sensor
	i2c_reg_write_byte(dev, addr, KX022_REG_CNTL1, KX022_BIT_PC1 | KX022_BIT_GSEL1);
	//TODO: wait 250ms?
}

bool kx022_verify_id(struct device *dev, u16_t addr){
	u8_t id = 0;
	//read who-am-i value
	i2c_reg_read_byte(dev, KX022_ADDR_GND, KX022_REG_Who_AM_I, &id);
	return KX022_WHO_AM_I_VAL == id;
}

void kx022_read_acc(struct device *dev, u16_t addr, u8_t* output){
	i2c_burst_read(dev, addr, KX022_REG_XOUTL, output, 6);
}
