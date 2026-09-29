#include "mc3635_driver.h"
#include "i2c_driver.h"
#include "nrf_delay.h"
#include "ble_driver.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

bool mc3635_init_success = false;

void mc3635_twi_write(uint8_t address, uint8_t data) {
	uint8_t reg[2] = {address, data};

	twi_write(MC3635_SLAVE_ADDRESS, reg, sizeof(reg), false);
}

void mc3635_init(void) {
	//Place device in STANDBY
	mc3635_twi_write(MC3635_MODE_REG, 0x01);

	//System reset (Write to bit 6 (0x40) of register 0x24)
    mc3635_twi_write(MC3635_RESET_REG, 0x40);
    nrf_delay_ms(5);

	//Enable I2C mode (Write 0x40 to 0x0D)
    mc3635_twi_write(MC3635_FEAT_REG_1, 0x40);  // enables 2 wire i2c interface
    

    mc3635_twi_write(MC3635_INIT_REG_1, 0x42);  // recommended by datasheet write fixed value
    mc3635_twi_write(MC3635_DMX_REG,    0x01);  // recommended by datasheet write fixed value
    mc3635_twi_write(MC3635_DMY_REG,    0x80);  // recommended by datasheet write fixed value
    mc3635_twi_write(MC3635_INIT_REG_2, 0x00);  // recommended by datasheet write fixed value
    mc3635_twi_write(MC3635_INIT_REG_3, 0x00);  // recommended by datasheet write fixed value
    
	// Burst fifo read and value overwrite allowed
    mc3635_twi_write(MC3635_FEAT_REG_2, 0x23);  
	
	// 25Hz in ULP mode, 28 Hz in LP mode
    mc3635_twi_write(MC3635_RATE_REG_1, 0x07);  
    
	// 2g range and 12bit resolution
	mc3635_twi_write(MC3635_RR_CTR_REG, 0x08);  

	// FIFO reset
    mc3635_twi_write(MC3635_FIFO_CTRL,  0x80);   

	// Interrupt set to push-pull and for FIFO Threshold
    //mc3635_twi_write(INT_CTRL, 0x41);    

	// Clear any interrupts
    mc3635_twi_write(MC3635_STAT_REG_2, 0x00);        

	// ULP mode
    //mc3635_twi_write(MC3635_PWR_MODE,   0x03);  //ULP
	//mc3635_twi_write(MC3635_PWR_MODE,   0x00);  //LP
	mc3635_twi_write(MC3635_PWR_MODE,   0x04);  //PREC

	// FIFO enabled and threshold set to SAMPLES
    mc3635_twi_write(MC3635_FIFO_CTRL,  0x40 | (MC3635_FIFO_SAMPLES & 0x1F));  

	//Test if reading/writing was successful.
	uint8_t test;
	twi_read(MC3635_SLAVE_ADDRESS, MC3635_FEAT_REG_2, &test, 1);
	if (test != 0x23) {
		NRF_LOG_INFO("Register 0x0E should be 0x23. 0x%02x was read instead.", test);
		NRF_LOG_INFO("MC3635 initialization failed.");
		mc3635_init_success = false;
		return;
	}

	//Put device in CWAKE mode.
	mc3635_twi_write(MC3635_MODE_REG, 0x05);

	mc3635_init_success = true;
	NRF_LOG_INFO("MC3635 initialized successfully.");
	NRF_LOG_FLUSH();
}

/*
const uint8_t mc_test[MC3635_PACKET_SIZE_BYTES] = {	0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11,
													0, 9, 0, 10, 0, 11};
*/

void mc3635_read_data(void) {
    uint8_t acc_data[8];						//Each time we read the FIFO
	uint8_t samples[MC3635_PACKET_SIZE_BYTES];	//All the samples in the FIFO
	uint8_t status;
	twi_read(MC3635_SLAVE_ADDRESS, MC3635_STAT_REG_1, &status, 1);
	status = (status >> 4) & 0x1;

	int i = 0;
	//Read until FIFO is empty.
	while( !status ) {
		twi_read(MC3635_SLAVE_ADDRESS, MC3635_ACC_DATA, acc_data, 8);
		//uint8_t test[8] = {0, 1, 2, 3, 4, 5, 6, 7};
		
		//Copy the contents of the accelerometer data to the output array
		if (i < MC3635_PACKET_SIZE_BYTES / 6)
			memcpy(samples + i*6, acc_data, 6);
		
		status = (acc_data[6] >> 4) & 0x1;
		i++;
	}
	
	nus_add_to_buffer(samples, MC3635_PACKET_SIZE_BYTES);
}

bool mc3635_init_successful(void) {
	return mc3635_init_success;
}