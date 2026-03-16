#include "max86141_driver.h"
#include "spi_driver.h"
#include "nrf_temp.h"

#include "nrf_delay.h"
#include "ble_driver.h"

//#include "mc3635_driver.h"
//#include "tmp117_driver.h"
#include "helper_functions.h"
#include "system_control.h"


static bool max86141_init_success = false;
uint8_t skipped_nrf_temp_meas = 0;

void max86141_config_pin(void) {
      	//Set a GPIO as the CS control

        nrf_gpio_cfg_output(SPI_CS_1);
        nrf_gpio_pin_set(SPI_CS_1);
        nrf_gpio_pin_clear(SPI_CS_1);

        nrf_gpio_cfg_output(SPI_CS_2);
        nrf_gpio_pin_set(SPI_CS_2);
        nrf_gpio_pin_clear(SPI_CS_2);

        nrf_gpio_cfg_output(SPI_CS_3);
        nrf_gpio_pin_set(SPI_CS_3);
        nrf_gpio_pin_clear(SPI_CS_3);

        nrf_gpio_cfg_output(SPI_CS_4);
        nrf_gpio_pin_set(SPI_CS_4);
        nrf_gpio_pin_clear(SPI_CS_4);

        max86141_cs_set();
}

void max86141_init(void) {

	//Set up a GPIO for receiving interrupts
	max86141_setup_interrupts();
        
        // UPDATE FOR MAX ICs
        uint8_t PIN_CS_PPG[4] = {SPI_CS_1, SPI_CS_2, SPI_CS_3, SPI_CS_4};

        uint8_t* part_id = spi_read_multi_reg(PIN_CS_PPG, 0xFF, NUM_MAX_IC);

	//if (part_id != 0x25) {
	//	NRF_LOG_INFO("Unexpected part ID for MAX86141: 0x%02x", part_id);
	//	NRF_LOG_FLUSH();
	//	return;
	//} else {
 //            NRF_LOG_INFO("MAX86141 ID: 0x%02x", part_id);
 //            NRF_LOG_FLUSH();
 //       }

	//Soft Reset (write 0x1 to RESET[0])
	spi_write_reg(PIN_CS_PPG, MAX86141_SYSTEM_CONTOL, 0x01);

	//Delay 1ms
	nrf_delay_ms(1);

	//Read interrupt status 1 to clear interrupt
	spi_read_multi_reg(PIN_CS_PPG, MAX86141_INTERRUPT_STATUS_1, NUM_MAX_IC);

	//Read interrupt status 2 to clear interrupt
	spi_read_multi_reg(PIN_CS_PPG, MAX86141_INTERRUPT_STATUS_2, NUM_MAX_IC);

	//Put the device into power-save mode / shutdown (write 0x1 to SHDN[0])
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_SYSTEM_CONTOL, 0x02, NUM_MAX_IC);

	//Set the GPIO2 function to MUX control
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_PPG_SYNC_CONTROL, 0x02, NUM_MAX_IC);

	//Set the pulse width to 117.3us (write 0x3 to PPG_TINT[1:0])
	//Set the ADC Range to 32uA (write 0x3 to PPG1_ADC_RGET[1:0])
	//Set the ADC Range to 32uA (write 0x3 to PPG2_ADC_RGET[1:0])
	//PPG Configuration 1 (0x11)
	//[7]:		ALC_DISABLE
	//[6]:		ADD_OFFSET
	//[5:4]:	PPG2_ADC_RGE
	//[3:2]:	PPG1_ADC_RGE
	//[1:0]:	PPG_TINT: 14.8us (0x0), 29.4us (0x1), 58.7 (0x2), 117.3 (0x3)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_PPG_CONFIGURATION_1, (1 << 7) | (0x03 << 4) | (0x03 << 2) | (0x03), NUM_MAX_IC);
	
	//Set the sample averaging to 1 (write 0x0 to SMP_AVE[2:0])
	//Set the sample rate to 100sps (write 0x03 to PPG_SR[4:0])
	//PPG Configuration 2 (0x12)
	//[7:3]		PPG_SR [0x04: 200sps, 0x03: 100sps, 0x01: 50sps, 0x00: 25sps]
	//[2:0]		SMP_AVE
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_PPG_CONFIGURATION_2, (0x03 << 3) | (0x0), NUM_MAX_IC);


	//Set the LED settling time
	//Set the digital filter to CDM
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_PPG_CONFIGURATION_3, (0x00 << 6) | (0x00 << 5), NUM_MAX_IC);

	//Set the PD 1 Biasing for Cpd = 0~65pF (write 0x01 to PDBIAS1[2:0])
	//Set the PD 2 Biasing for Cpd = 0~65pF (write 0x01 to PDBIAS2[2:0])
	//The TEMD7000 has a diode capacitance of 4pF
	//Photo Diode Bias (0x15)
	//[6:4]		PDBIAS2
	//[2:1]		PDBIAS1
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_PHOTODIODE_BIAS, (0b110 << 4) | (0b110), NUM_MAX_IC);

	//Set the LED Driver 1 Range to 124mA (write 0x3 to LED1_RGE[1:0])
	//Set the LED Driver 2 Range to 124mA (write 0x3 to LED2_RGE[1:0])
	//Set the LED Driver 3 Range to 124mA (write 0x3 to LED2_RGE[1:0])
	//LED Range 1 (0x2A)
	//[5:4]		LED3_RGE
	//[3:2]		LED2_RGE
	//[1:0]		LED1_RGE
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_RANGE_1, (0x03 << 4) | (0x03 << 2) | (0x03), NUM_MAX_IC);

	//Set the LED Driver 4 Range to 124mA (write 0x3 to LED4_RGE[1:0])
	//Set the LED Driver 5 Range to 124mA (write 0x3 to LED5_RGE[1:0])
	//Set the LED Driver 6 Range to 124mA (write 0x3 to LED6_RGE[1:0])
	//LED Range 2 (0x2B)
	//[5:4]		LED4_RGE
	//[3:2]		LED5_RGE
	//[1:0]		LED6_RGE
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_RANGE_2, (0x03 << 4) | (0x03 << 2) | (0x03), NUM_MAX_IC);

	//Set the LED 1 Drive Current to 124mA (write 0xFF to LED1_DRV[7:0]) (Red)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED1_PA, 0xFF, NUM_MAX_IC);
	
	//Set the LED 2 Drive Current to 124mA (write 0xFF to LED2_DRV[7:0]) (IR)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED2_PA, 0xFF, NUM_MAX_IC);

        //Set the LED 3 Drive Current to 124mA (write 0xFF to LED3_DRV[7:0]) (Red)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED3_PA, 0xFF, NUM_MAX_IC);

	//Set the LED 4 Drive Current to 124mA (write 0xFF to LED4_DRV[7:0]) (IF)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED4_PA, 0xFF, NUM_MAX_IC);

	//Set the LED 5 Drive Current to 124mA (write 0xFF to LED5_DRV[7:0]) (Red)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED5_PA, 0xFF, NUM_MAX_IC);

        //Set the LED 6 Drive Current to 124mA (write 0xFF to LED6_DRV[7:0]) (IR)
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED6_PA, 0xFF, NUM_MAX_IC);

	////////////////////FIFO Configuration/////////////////////////////////
	//Set the FIFO INT trigger condition (write MAX86141_A_FULL to FIFO_A_FULL[6:0])
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_FIFO_CONFIGURATION_1, MAX86141_A_FULL, NUM_MAX_IC);

	//Enable FIFO Roll over (write 0x1 to FIFO_RO)
	//Note: Right now interrupt is only cleared on reading interrupt status register
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_FIFO_CONFIGURATION_2, 0x02, NUM_MAX_IC);

	//Enable the FIFO_A_FULL interrupt and DIE_TEMP_RDY (write 0x1 to A_FULL_EN)
	spi_write_reg(SPI_CS_4, MAX86141_INTERRUPT_ENABLE_1, 0x80);
        //spi_write_multi_reg(PIN_CS_PPG, MAX86141_INTERRUPT_ENABLE_1, 0x80, NUM_MAX_IC);

	//////////////////LED Sequence Control/////////////////////////////////
	//Put LED1 exposure to time slot 1 (write 0x1 to LEDC1[3:0])
	//Put LED2 exposure to time slot 2 (write 0x2 to LEDC2[3:0])
	//spi_write_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_1, (0x02 << 4) | (0x01));
	
        //spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_1, (0x02 << 4) | (0x03), NUM_MAX_IC);
        //spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_2, (0x0C << 4) | (0x01), NUM_MAX_IC);
        //spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_3, (0x0A << 4) | (0x0B), NUM_MAX_IC);

        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_1, (0x02 << 4) | (0x01), NUM_MAX_IC);
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_2, (0x0A << 4) | (0x03), NUM_MAX_IC);
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_3, (0x0C << 4) | (0x0B), NUM_MAX_IC);

	//Put LED4 exposure to time slot 3 (write 1010b to LEDC3[3:0])
	//Put LED5 exposure to time slot 4 (write 1011B to LEDC4[3:0])
	//spi_write_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_2, (0b1011 << 4) | (0b1010));

	//Put blank config in the rest of the time slots (write 0x0 to LEDC4[3:0] to LEDC6[3:0])
	//Shouldn't be necessary, since on reset they default to 0x0

	//Start sampling (write 0x0 to SHDN[0])
	//Enable Low Power Mode (Write 0x1 to LP_Mode[0])
	//Dual PD
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_SYSTEM_CONTOL, 0x04, NUM_MAX_IC);

	max86141_init_success = true;

        //Deactivate CS
        //nrf_gpio_pin_set(cs_pin);
        max86141_cs_set();

	NRF_LOG_INFO("MAX86141(s) initialized successfully");
	NRF_LOG_FLUSH();
	
}


void max86141_cs_set(void) {
        uint16_t cs_pins[NUM_MAX_IC] = {SPI_CS_1, SPI_CS_2, SPI_CS_3, SPI_CS_4};
        
        for (int i = 0; i < NUM_MAX_IC; i++) {
            nrf_gpio_pin_set(cs_pins[i]);
        }
}


void max86141_cs_clear(void) {
        uint16_t cs_pins[NUM_MAX_IC] = {SPI_CS_1, SPI_CS_2, SPI_CS_3, SPI_CS_4};

        for (int i = 0; i < NUM_MAX_IC; i++) {
            nrf_gpio_pin_clear(cs_pins[i]);
        }
}

void max86141_verify_integrity(uint16_t cs_pin) {
       max86141_cs_set();

        uint8_t part_id = spi_read_reg(cs_pin, 0xFF);

        if (part_id != 0x25) {
        	NRF_LOG_INFO("Unexpected part ID for MAX86141: 0x%02x", part_id);
        	NRF_LOG_FLUSH();
        	return;
        } else {
                NRF_LOG_INFO("MAX86141 ID: 0x%02x", part_id);
                NRF_LOG_FLUSH();
        }

        max86141_cs_clear();
}


void max86141_setup_interrupts(void) {
	//Interrupt will bring INT_PPG low. We also need to enable the internal pullup resistor.
	ret_code_t err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);

        nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
        in_config.pull = NRF_GPIO_PIN_PULLUP;

        err_code = nrf_drv_gpiote_in_init(PIN_INT_PPG, &in_config, max86141_interrupt_handler);
        APP_ERROR_CHECK(err_code);

        nrf_drv_gpiote_in_event_enable(PIN_INT_PPG, true);
}

void max86141_interrupt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
	//Read status (Read 0x00 and put in intStatus)	
	uint8_t intStatus = spi_read_reg(SPI_CS_4, MAX86141_INTERRUPT_STATUS_1);

	if (intStatus & 0x80) { //A FULL RDY
                //NRF_LOG_INFO("MAX86141(s) full");
                //NRF_LOG_FLUSH();
		//Increment LEDs
		//nrf_gpio_pin_toggle(PIN_INCR_LED);
		//nrf_delay_us(500);

		//Read PPG FIFO
                uint8_t PIN_CS_PPG[4] = {SPI_CS_1, SPI_CS_2, SPI_CS_3, SPI_CS_4};
                max86141_fifo_multi_parser(PIN_CS_PPG, 4);

		//Read accelerometer FIFO
		//if(mc3635_init_successful())
			//mc3635_read_data();

		//Read temperature sensor
		//if(tmp117_init_successful())
			//tmp117_read_temp();
		
		//Prompt to send the nus buffer over BLE
		//nus_send_buffer();

		//Check the die temperatures.
		//check_die_temperatures();
	}
}


const uint8_t max_test[MAX86141_PACKET_SIZE_BYTES] = {
                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12
};
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12,
//                                                        0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6,
//                                                        0, 0, 7, 0, 0, 8, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 12};


uint8_t test[3] = {0, 0, 0};

uint32_t poop = 0;

bool MAX86141_NOT_FULL = false;

void max86141_fifo_parser(uint8_t PIN_CS_PPG) {
	uint8_t sampleCnt = spi_read_reg(PIN_CS_PPG,MAX86141_FIFO_DATA_COUNTER); // sampleCnt should be the same value as FIFO_SAMPLES;

        //NRF_LOG_INFO("Data in FIFO (%d), expected (%d)", sampleCnt, MAX86141_FIFO_SAMPLES);

	if (sampleCnt != MAX86141_FIFO_SAMPLES) {
		NRF_LOG_INFO("CS: (%d) ~ Data in FIFO (%d) is NOT as expected (%d)", PIN_CS_PPG, sampleCnt, MAX86141_FIFO_SAMPLES);
		NRF_LOG_FLUSH();
                MAX86141_NOT_FULL = true;
	} else {
                NRF_LOG_INFO("CS: (%d) ~ Data in FIFO (%d) IS as expected (%d)", PIN_CS_PPG, sampleCnt, MAX86141_FIFO_SAMPLES);
		NRF_LOG_FLUSH();
        }

	uint8_t data_buf[MAX86141_FIFO_SAMPLES * 3]; //(128 - FIFO_A_FULL[6:0]) samples, 3 byte/channel
        

	//Read FIFO
        //36 * 3 = 108 bytes
        //18 * 3 = 54 bytes
	uint8_t bytes_to_read = MAX86141_FIFO_SAMPLES * 3;

	spi_burst_read(PIN_CS_PPG, MAX86141_FIFO_DATA_REGISTER, data_buf, bytes_to_read);

        //dump_fifo_data(data_buf, bytes_to_read);

	nus_add_to_buffer(data_buf, bytes_to_read);

        //nus_add_to_buffer(max_test, bytes_to_read);



}

#define MAX_BYTES_PER_LINE 16

void dump_fifo_data(uint8_t *data_buf, uint16_t num_bytes) {
    NRF_LOG_INFO("FIFO Data:");
    for (uint16_t i = 0; i < num_bytes; i++) {
        if (i % MAX_BYTES_PER_LINE == 0 && i > 0) {
            NRF_LOG_FLUSH();
            NRF_LOG_INFO("");
        }
        NRF_LOG_RAW_INFO("%02X ", data_buf[i]); // Print each byte in hexadecimal format
    }
    NRF_LOG_FLUSH();
}

void max86141_fifo_multi_parser(uint8_t* PINS_CS_PPG,  uint8_t num_pins) {
	
        for(int i = 0; i < num_pins; i++) {
            max86141_fifo_parser(PINS_CS_PPG[i]);
        }

}

void max86141_start_die_temp_measurement(void) {
	//To start a die temperature measurement, write a 1 to the TEMP_EN bit in Die Temperature Configureation (0x40)
	spi_write_reg(SPI_CS_1, MAX86141_DIE_TEMP_CONFIG, 0x01);

	//Start a new measurement
	NRF_TEMP->TASKS_START = 1; /** Start the temperature measurement. */

}

void check_die_temperatures() {
	uint8_t max_temp_int = 0;
	int32_t nrf_temp = 0;
	
	if (spi_read_reg(SPI_CS_1, MAX86141_DIE_TEMP_CONFIG) == 0) {	//MAX86141 die temperature ready
		//Read the temperature. We don't need super accurate readings, so just the integer part should be fine.
		max_temp_int = spi_read_reg(SPI_CS_1, MAX86141_DIE_TEMP_INTEGER);
		NRF_LOG_INFO("MAX86141 die temperature: %i degrees C", max_temp_int);
		//Start a new measurement.
		spi_write_reg(SPI_CS_1, MAX86141_DIE_TEMP_CONFIG, 0x01);

		//Check if the temperature is within threshold
		if (max_temp_int >= MAX86141_DIE_TEMP_THRESH) {
			NRF_LOG_INFO("MAX86141 over-temperature detected! Putting tattoo in shutdown.");
			system_shutoff_total();
		}
	}

	//Check if the nRF temperature measurement is finished.
	if (NRF_TEMP->EVENTS_DATARDY != 0 || skipped_nrf_temp_meas > 3) {
		NRF_TEMP->EVENTS_DATARDY = 0;
		nrf_temp = (nrf_temp_read() / 4);
		NRF_TEMP->TASKS_STOP = 1; /** Stop the temperature measurement. */
		NRF_TEMP->TASKS_START = 1; /** Start the temperature measurement. */
		NRF_LOG_INFO("nRF52811 die temperature: %i degrees C", nrf_temp);
		
		if (nrf_temp >= NRF_DIE_TEMP_THRESH) {
			NRF_LOG_INFO("nRF52811 over-temperature detected! Putting tattoo in shutdown.");
			system_shutoff_total();
		}
		skipped_nrf_temp_meas = 0;
	} else {
		skipped_nrf_temp_meas ++;
	}
}