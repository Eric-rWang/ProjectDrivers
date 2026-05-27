#include "tmp117_driver.h"
#include "ble_driver.h"

bool tmp117_init_success = false;

void tmp117_write_reg(uint8_t register_address, uint8_t msb, uint8_t lsb) {
	uint8_t reg_data[3] = {register_address, msb, lsb};
	twi_write(TMP117_SLAVE_ADDRESS, reg_data, sizeof(reg_data), false);
}

void tmp117_init(void) {
	//Soft reset.
	tmp117_write_reg(TMP117_CONFIGURATION, 0x00, 0x02);
	nrf_delay_ms(5);

	//Set the mode to continuous conversion. (0x00 to MOD[1:0])
	//Set the conversion cycle time to 250ms (0x02 to CONV[2:0])
	//Set the number of averages to 8. (0x01 to AVG[1:0])
	tmp117_write_reg(TMP117_CONFIGURATION, (0x00 << 2) | 0x01, 0x01 << 5);

	//Check if initialization was successful.
	uint8_t test[2];
	twi_read(TMP117_SLAVE_ADDRESS, TMP117_CONFIGURATION, test, 2);
	if(! ((test[0] == (0x03 << 2) | 0x01) && (test[1] == (0x01 << 5)))) {
		NRF_LOG_INFO("TMP117 initialization FAILED.");
		return;
	}
	
	twi_read(TMP117_SLAVE_ADDRESS, TMP117_DEVICE_ID, test, 2);
	uint16_t device_id = (((uint16_t)test[0] & 0x0F) << 8) | test[1];
	if(device_id != 0x117) {
		NRF_LOG_INFO("TMP117 incorrect device ID (0x%02x).", device_id);
		return;
	}

	tmp117_init_success = true;
	NRF_LOG_INFO("TMP117 initialized successfully.");
}

//const uint8_t tmp_test[2] = {0, 12};

/**
 * @brief Read the Temperature Register from TMP117.
 * @param samples Output array. Assumed to be size 2 (16 bits).
 */
void tmp117_read_temp(void) {
	uint8_t samples[2];
	twi_read(TMP117_SLAVE_ADDRESS, TMP117_TEMP_RESULT, samples, 2);

	nus_add_to_buffer(samples, 2);
}

bool tmp117_init_successful(void) {
	return tmp117_init_success;
}