#include "i2c_driver.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include <stdint.h>
#include <stdbool.h>

#define TMP117_SLAVE_ADDRESS	0x48

//Register Map
#define TMP117_TEMP_RESULT		0x00
#define TMP117_CONFIGURATION	0x01
#define TMP117_THIGH_LIMIT		0x02
#define TMP117_TLOW_LIMIT		0x03
#define TMP117_EEPROM_UL		0x04
#define TMP117_EEPROM1			0x05
#define TMP117_EEPROM2			0x06
#define TMP117_TEMP_OFFSET		0x07
#define TMP117_EEPROM3			0x08
#define TMP117_DEVICE_ID		0x0F

void tmp117_write_reg(uint8_t register_address, uint8_t msb, uint8_t lsb);
void tmp117_init(void);
void tmp117_read_temp(void);
bool tmp117_init_successful(void);