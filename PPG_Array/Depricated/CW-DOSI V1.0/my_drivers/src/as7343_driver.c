#include "as7343_driver.h"
#include "i2c_driver.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"
#include "system_control.h"

uint8_t led_power = 1;
uint8_t atime = 99;

void as7343_init(void) {
    
    //The AS7343 can remain in sleep during setup.

    //Ensure that initialization busy is not set. THIS NEEDS AN EXIT IN CASE NACK
    while (as7343_read_reg(0xBC) & 0x01) {}

    //In order to access registers below 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1<<4);


    //Ensure that device IDs are as expected.
    uint8_t id = as7343_read_reg(0x5A);
    if(id != 0b10000001) {
        NRF_LOG_INFO("AS7343 ID is incorrect (0x%02x)", id);
        return;
    }

    //In order to access registers 0x80 and above, REG_BANK needs to be set to 0.
    //Also enable low power idle
    as7343_write_reg(0xBF, (1 << 5) | (0 << 4));  // Enable low power, set REG_BANK to 0

    //ENABLE Register (0X80)
    //Setting the PON bit puts device in idle.
    //Setting the SP_EN bit puts the device in active.
    as7343_write_reg(0x80, 1); // Enabling Device
    
    //CONTROL Register
    //Software reset and clear FIFO Buffer
    as7343_write_reg(0xFA, (1 << 3) | (1 << 1));
    //NRF_LOG_INFO("0x%02x", as7343_read_reg(0xBF));

    //GPIO Register (0X6B)
    //Set the GPIO to be input enable
    as7343_write_reg(0x6B, 0x01 << 2);
    
    //LED Register (0xCD)
    //Bit 7 is use external LED
    //Writing 6mA for now
    as7343_write_reg(0xCD, (1 << 7) | 1);

    //INTENAB Register (0xF9)
    //Do not need to set. Not using interrupts.    

    //ATIME Register
    //Set to an integration time of 100
    as7343_write_reg(0x81, atime);
    //ASTEP Register
    //as7343_write_two_reg(0xCA, 0x03, 0xE7);
    //tint = (ATIME + 1) x (ASTEP + 1) x 2.78us
    //as7343_write_two_reg(0xCA, 0x03, 0xE7);  // ASTEP = 999
    as7343_write_reg(0xD4, 0xE7);
    as7343_write_reg(0xD5, 0x03);
    //NRF_LOG_INFO("0x%02x%02x", as7343_read_reg(0xCA), as7343_read_reg(0xCB));

    //WTIME Register
    //Sets the sample rate.
    //Only needs setting if WEN in reg 0x80 is set.

    //FD_Time Register
    //Sets the ADC settings for flicker detection.
    //Only needs setting if FDEN in reg 0x80 is set.

    //ADC Configuration
    //CFG1 sets the spectral sensitivity. 0.5x, 1x, 2x, etc.
    as7343_write_reg(0xC6, 12);
    //CFG10 sets the flicker detect persistence. Skipping for now.
    //AZ_CONFIG sets how often the sepctral engine offsets are reset. Set to only before first measurement cycle.
    as7343_write_reg(0xDE, 255);
    //AGC_GAIN_MAX sets the flicker detection AGC gain max. Skipping for now.
    
    //CFG8 sets the fifo size for triggering interrupts. Setting to fifo size of 1.
    //as7343_write_reg(0xC9, 0);
    
    //CFG20 sets the automatic channel readout
    as7343_write_reg(0xD6, 0);  // Disable auto_SMUX
    //as7343_write_reg(0xD6, 3<<5);

    //FIFO Configuration
    //Set which channels are written to the FIFO
    //Channels 0 - 4, not including ASTATUS and FD
    as7343_write_reg(0xFC, 0x3E);

    
    //Spectral Interrupt. Set thresholds and if SP_IEN is set, an interrupt will be thrown if data is not between the thresholds.
    NRF_LOG_INFO("AS7343 initialized successfully.");

    //ENABLE Register (0X80)
    //Setting the PON bit puts device in idle.
    //Setting the SP_EN bit puts the device in active.
    as7343_write_reg(0x80, 3); // Enabling Device

    uint8_t reg = as7343_read_reg(0x81);
    
    NRF_LOG_INFO("ATIME 0x81: (0x%02x)", reg);

    uint8_t time_1 = as7343_read_reg(0xD4);
    uint8_t time_2 = as7343_read_reg(0xD5);
    
    NRF_LOG_INFO("ASTEP 0xD4: (0x%02x)", time_1);
    NRF_LOG_INFO("ASTEP 0xD5: (0x%02x)", time_2);
}

void as7343_write_reg(uint8_t register_address, uint8_t data) {
    uint8_t reg_data[2] = {register_address, data};
    twi_write(AS7343_SLAVE_ADDRESS, reg_data, sizeof(reg_data), false);
}

void as7343_write_two_reg(uint8_t register_address, uint8_t lsb, uint8_t msb) {
    uint8_t reg_data[3] = {register_address, lsb, msb};
    twi_write(AS7343_SLAVE_ADDRESS, reg_data, sizeof(reg_data), false);
}

uint8_t as7343_read_reg(uint8_t register_address) {
    uint8_t data[1];
    twi_read(AS7343_SLAVE_ADDRESS, register_address, data, 1);

    return data[0];
}

void as7343_start_measurement(uint8_t num_measurements) {
    set_system_state(SYSTEM_STATE_CALIBRATING);
    NRF_LOG_INFO("Starting measurments...");


    //LED Register
    //[7]   LED_ACT: set for external LED
    //[6:0] LED_DRIVE: LED current. 1111111 for 258 mA
    as7343_write_reg(0xCD, (1 << 7) | led_power);
	
    for(uint8_t i = 0; i < num_measurements; i ++) {
        //PON and enable spectral measurement
        as7343_write_reg(0x80, 3);

        as7343_read_spectral_data();
    }

    //Turn off LED
    as7343_write_reg(0xCD, 0);

    //Disable PON
    as7343_write_reg(0x80, 0);
    set_system_state(SYSTEM_STATE_ALL_ON);
}

void as7343_read_spectral_data() {
    //Check if spectral measurement has been completed (AVALID in STATUS 2)
    NRF_LOG_INFO("Waiting...");
    uint8_t status = as7343_read_reg(0x90);

    while (!(status & 0x40)) {
        //NRF_LOG_INFO("0x%02x", status);
        status = as7343_read_reg(0x90);
        nrf_delay_ms(1);
    }

    //Signal saturated. Redo and lower LED current.
    //if(status & 0x08) {
    //	NRF_LOG_INFO("Analog saturation.");
    //	if(led_power > 0) {
    //		led_power --;
    //		NRF_LOG_INFO("Decreasing LED power.");
    //	} else if (atime > 0) {
    //		atime --;
    //		as7343_write_reg(0x81, atime);
    //		NRF_LOG_INFO("Decreasing integration time.");
    //	}
            
    //	as7343_start_measurement();
    //} else { 
    //	uint8_t data[37];
    //	twi_read(AS7343_SLAVE_ADDRESS, 0x95, data, 37);

    //	send_data_nus(data, 37);
    //	NRF_LOG_INFO("Data sent.");
    //}

    //1 byte for ASTATUS
    //36 data bytes (18 channels of data) 
    uint8_t data[37];
    twi_read(AS7343_SLAVE_ADDRESS, 0x94, data, 37);

    //send_data_nus(data, 37);
    send_data_cus(TMS_MSG_TX_AS7343_DATA, data, 37);
    NRF_LOG_INFO("Data sent.");
}

void as7343_dump_fifo() {
    //Check level of FIFO (FIFO_LVL in 0xFD)
    uint8_t fifo_lvl = as7343_read_reg(0xFD);

    NRF_LOG_INFO("%i samples in the FIFO", fifo_lvl);
    NRF_LOG_FLUSH();

    //Check if there is overflow
    uint8_t status4 = as7343_read_reg(0xBC);
    NRF_LOG_INFO("0x%02x", status4);
    NRF_LOG_FLUSH();

    //Clear FIFO (DEBUG)
    as7343_write_reg(0xFA, 2);

    if(fifo_lvl == 0)
      return;
    uint8_t data[fifo_lvl * 2];
    //twi_read(AS7343_SLAVE_ADDRESS, 0xFE, data, sizeof(data));

    //Check level of FIFO (FIFO_LVL in 0xFD)
    //fifo_lvl = as7343_read_reg(0xFD);

    //NRF_LOG_INFO("%i samples in the FIFO", fifo_lvl);
    //NRF_LOG_FLUSH();

    /*
    for(uint8_t i = 0; i < fifo_lvl; i ++) {
        //Read addresses 0xFE - 0xFF

        //1 byte for ASTATUS
        //36 data bytes (18 channels of data) 
	

	twi_read(AS7343_SLAVE_ADDRESS, 0xFE, data, sizeof(data));

	//send_data_nus(data, sizeof(data));
	//send_data_cus(TMS_MSG_TX_AS7343_DATA, data, sizeof(data));
	//NRF_LOG_INFO("Data sent.");
    }
    */
    
}