#include "max86141_driver.h"
#include "spi_driver.h"
#include "nrf_temp.h"

#include "nrf_delay.h"
#include "ble_driver.h"

#include "helper_functions.h"
#include "system_control.h"

//CS Array Init
uint8_t PIN_CS_PPG[NUM_MAX_IC] = {
    SPI_CS_1, SPI_CS_2, SPI_CS_3,
    SPI_CS_4, SPI_CS_5, SPI_CS_6
};

static bool max86141_init_success = false;
volatile bool ppg_data_ready = false;
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

        nrf_gpio_cfg_output(SPI_CS_5);
        nrf_gpio_pin_set(SPI_CS_5);
        nrf_gpio_pin_clear(SPI_CS_5);

        nrf_gpio_cfg_output(SPI_CS_6);
        nrf_gpio_pin_set(SPI_CS_6);
        nrf_gpio_pin_clear(SPI_CS_6);

        max86141_cs_set();
}

void max86141_init(void) {
        //Configure pins
        max86141_config_pin();

        //Validate hardware connectivity
        max86141_burst_integrity();

        //Set up a GPIO for receiving interrupts
	max86141_setup_interrupts();
        uint8_t* part_id = spi_read_multi_reg(PIN_CS_PPG, 0xFF, NUM_MAX_IC);

	//Soft Reset (write 0x1 to RESET[0])
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_SYSTEM_CONTOL, 0x01, NUM_MAX_IC);

	//Delay 1ms
	nrf_delay_ms(1);
        
        //-------------------------------------------------------------------//
        ////////////////////System Configuration///////////////////////////////
        //-------------------------------------------------------------------//

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
        //spi_write_multi_reg(PIN_CS_PPG, MAX86141_PPG_CONFIGURATION_1, (0 << 7) | (0x03 << 4) | (0x03 << 2) | (0x03), NUM_MAX_IC);
  
	
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
	//The Photo Diode Bias (0x15) for SFH 2704 (Capacitance < 65pF)
        // Set PDBIAS1 and PDBIAS2 to 0x01
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_PHOTODIODE_BIAS, (0x01 << 4) | (0x01), NUM_MAX_IC);

	//Set the LED Driver 1 Range to 124mA (write 0x3 to LED1_RGE[1:0])
	//Set the LED Driver 2 Range to 124mA (write 0x3 to LED2_RGE[1:0])
	//Set the LED Driver 3 Range to 124mA (write 0x3 to LED2_RGE[1:0])
	//LED Range 1 (0x2A)
	//[5:4]		LED3_RGE
	//[3:2]		LED2_RGE
	//[1:0]		LED1_RGE

	//Set the LED Driver 4 Range to 124mA (write 0x3 to LED4_RGE[1:0])
	//Set the LED Driver 5 Range to 124mA (write 0x3 to LED5_RGE[1:0])
	//Set the LED Driver 6 Range to 124mA (write 0x3 to LED6_RGE[1:0])
	//LED Range 2 (0x2B)
	//[5:4]		LED4_RGE
	//[3:2]		LED5_RGE
	//[1:0]		LED6_RGE
	
        //LED Driver Range to 31mA (0x00)
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_RANGE_1, (0x00 << 4) | (0x00 << 2) | (0x00), NUM_MAX_IC);
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_RANGE_2, (0x00 << 4) | (0x00 << 2) | (0x00), NUM_MAX_IC);

	//Set the LED 1 Drive Current to 124mA (write 0xFF to LED1_DRV[7:0]) (1. HUMAN: 660 -> PHANTOM: 540)
        //Set the LED 5 Drive Current to 124mA (write 0xFF to LED5_DRV[7:0]) (1. HUMAN: 660 -> PHANTOM: 540)
        //0x80
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED1_PA, 0x84, NUM_MAX_IC);
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED5_PA, 0x84, NUM_MAX_IC);

        //Set the LED 3 Drive Current to 124mA (write 0xFF to LED3_DRV[7:0]) (2. HUMAN: 940 -> PHANTOM: 660)
	//Set the LED 4 Drive Current to 124mA (write 0xFF to LED4_DRV[7:0]) (2. HUMAN: 940 -> PHANTOM: 660)
        //0x42
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED3_PA, 0x42, NUM_MAX_IC);
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED4_PA, 0x42, NUM_MAX_IC);

        //Set the LED 6 Drive Current to 124mA (write 0xFF to LED6_DRV[7:0]) (3. HUMAN: 850 -> PHANTOM: 630)
        //Set the LED 2 Drive Current to 124mA (write 0xFF to LED2_DRV[7:0]) (3. HUMAN: 850 -> PHANTOM: 630)
        //0x42
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED2_PA, 0x42, NUM_MAX_IC);
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED6_PA, 0x42, NUM_MAX_IC);
        
        //-------------------------------------------------------------------//
	////////////////////FIFO Configuration/////////////////////////////////
        //-------------------------------------------------------------------//

	//Set the FIFO INT trigger condition (write MAX86141_A_FULL to FIFO_A_FULL[6:0])
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_FIFO_CONFIGURATION_1, MAX86141_A_FULL, NUM_MAX_IC);

	//Enable FIFO Roll over (write 0x1 to FIFO_RO)
	//Note: Right now interrupt is only cleared on reading interrupt status register
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_FIFO_CONFIGURATION_2, 0x02, NUM_MAX_IC);
        
        //-------------------------------------------------------------------//
        //////////////.//////INT Configuration/////////////////////////////////
        //-------------------------------------------------------------------//

        // CHANGE FOR INTERUPT
	//Enable the FIFO_A_FULL interrupt and DIE_TEMP_RDY (write 0x1 to A_FULL_EN)
	spi_write_reg(SPI_CS_6, MAX86141_INTERRUPT_ENABLE_1, 0x80);

        //spi_write_multi_reg(PIN_CS_PPG, MAX86141_INTERRUPT_ENABLE_1, 0x80, NUM_MAX_IC);
        
        //-------------------------------------------------------------------//
	//////////////////LED Sequence Control/////////////////////////////////
        //-------------------------------------------------------------------//

	//Put LED1 exposure to time slot 1 (write 0x1 to LEDC1[3:0])
	//Put LED2 exposure to time slot 2 (write 0x2 to LEDC2[3:0])
	//spi_write_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_1, (0x02 << 4) | (0x01));
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_1, (0x02 << 4) | (0x01), NUM_MAX_IC);
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_2, (0x0A << 4) | (0x03), NUM_MAX_IC);
        spi_write_multi_reg(PIN_CS_PPG, MAX86141_LED_SEQUENCE_REG_3, (0x0C << 4) | (0x0B), NUM_MAX_IC);

	//Start sampling (write 0x0 to SHDN[0])
	//Enable Low Power Mode (Write 0x1 to LP_Mode[0])
	//Dual PD
        nrf_delay_ms(1);
	spi_write_multi_reg(PIN_CS_PPG, MAX86141_SYSTEM_CONTOL, 0x04, NUM_MAX_IC);

	max86141_init_success = true;

        //Deactivate CS
        max86141_cs_set();

	NRF_LOG_INFO("MAX86141(s) initialization successful");
	NRF_LOG_FLUSH();
}


void max86141_cs_set(void) {
        for (int i = 0; i < NUM_MAX_IC; i++) {
            nrf_gpio_pin_set(PIN_CS_PPG[i]);
        }
}


void max86141_cs_clear(void) {
        for (int i = 0; i < NUM_MAX_IC; i++) {
            nrf_gpio_pin_clear(PIN_CS_PPG[i]);
        }
}

void max86141_burst_integrity(void) {
        bool init;

        for (int i = 0; i < NUM_MAX_IC; i++) {
            init = max86141_verify_integrity(PIN_CS_PPG[i]);
            if (!init) {
                NRF_LOG_INFO("Pin: 0x%02x Failed Verification", PIN_CS_PPG[i]);
            } else if (init) {
                NRF_LOG_INFO("Pin: 0x%02x Passed Verification", PIN_CS_PPG[i]);
            }
            nrf_delay_ms(10);
        }

        NRF_LOG_INFO("Burst Verification Complete");
        NRF_LOG_FLUSH();
}

bool max86141_verify_integrity(uint16_t cs_pin) {
        max86141_cs_set();

        uint8_t part_id = spi_read_reg(cs_pin, 0xFF);
        bool init;

        if (part_id != 0x25) {
        	init =  false;
        } else {
            init =  true;
        }
        
        max86141_cs_clear();
        nrf_delay_ms(1);

        return init;
}

uint8_t test[3] = {0, 0, 0};

uint32_t poop = 0;

bool MAX86141_NOT_FULL = false;

void max86141_setup_interrupts(void) {
	//Interrupt will bring INT_PPG low. We also need to enable the internal pullup resistor.
	ret_code_t err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);

        nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
        in_config.pull = NRF_GPIO_PIN_PULLUP;

        err_code = nrf_drv_gpiote_in_init(PIN_INT_PPG, &in_config, max86141_interrupt_handler);
        APP_ERROR_CHECK(err_code);

        nrf_drv_gpiote_in_event_enable(PIN_INT_PPG, true);

        NRF_LOG_INFO("INT Setup successful");
	NRF_LOG_FLUSH();
}

void max86141_interrupt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
	//Read status (Read 0x00 and put in intStatus)	
	ppg_data_ready = true;
}

void max86141_process_data(void) {
    if (!ppg_data_ready) return;
    ppg_data_ready = false;
 
    // Safe to do SPI here — we're in thread context,
    // so the SPI completion IRQ can preempt us normally.
    uint8_t intStatus = spi_read_reg(SPI_CS_6, MAX86141_INTERRUPT_STATUS_1);
 
    if (intStatus & 0x80) {
        max86141_fifo_multi_parser(PIN_CS_PPG, NUM_MAX_IC);
    }
}

void max86141_fifo_multi_parser(uint8_t* PINS_CS_PPG,  uint8_t num_pins) {
        for(int i = 0; i < num_pins; i++) {
            max86141_fifo_parser(PINS_CS_PPG[i]);
        }
}

void max86141_fifo_parser(uint8_t PIN_CS_PPG) {
	uint8_t sampleCnt = spi_read_reg(PIN_CS_PPG,MAX86141_FIFO_DATA_COUNTER); // sampleCnt should be the same value as FIFO_SAMPLES;

        //NRF_LOG_INFO("Data in FIFO (%d), expected (%d)", sampleCnt, MAX86141_FIFO_SAMPLES);

	if (sampleCnt < MAX86141_FIFO_SAMPLES) {
		NRF_LOG_INFO("CS: (%d) ~ Data in FIFO (%d) is NOT as expected (%d)", PIN_CS_PPG, sampleCnt, MAX86141_FIFO_SAMPLES);
		NRF_LOG_FLUSH();
                MAX86141_NOT_FULL = true;
	} else {
                //NRF_LOG_INFO("CS: (%d) ~ Data in FIFO (%d) IS as expected (%d)", PIN_CS_PPG, sampleCnt, MAX86141_FIFO_SAMPLES);
		//NRF_LOG_FLUSH();
        }

        // MAX86141_FIFO_SAMPLES = 12
	uint8_t data_buf[MAX86141_FIFO_SAMPLES * 3]; //(128 - FIFO_A_FULL[6:0]) samples, 3 byte/channel        

	//Read FIFO
        //12 * 3 = 36 bytes
	uint8_t bytes_to_read = MAX86141_FIFO_SAMPLES * 3;

	spi_burst_read(PIN_CS_PPG, MAX86141_FIFO_DATA_REGISTER, data_buf, bytes_to_read);

	//nus_add_to_buffer(data_buf, bytes_to_read);

        // Dump extra data not needed...
        // Diagonal
        //static const uint8_t drop_mask[12] = { 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0 };
        // Across
        static const uint8_t drop_mask[12] = { 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1 };

        // This buffer will hold the 6 kept samples (6 * 3 = 18 bytes)
        uint8_t filtered_buf[6 * 3];
        uint8_t write_index = 0;

        for (uint8_t i = 0; i < MAX86141_FIFO_SAMPLES; i++) {
            if (drop_mask[i] == 1) {
                // Copy the 3 bytes of sample #i from data_buf into filtered_buf
                memcpy(&filtered_buf[write_index * 3],
                       &data_buf[i * 3],
                       3);
                write_index++;
            }
        }

        // If you want to send only the filtered 18 bytes onward:
        nus_add_to_buffer(filtered_buf, 18);
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


// TESTING //
// It reads back every critical register from each IC individually
// and flags any IC whose value differs from the majority.
void max86141_dump_all_ic_registers(void)
{
    // Registers that control ADC range, LED current, and sampling —
    // any mismatch here explains saturation on one IC.
    typedef struct {
        uint8_t addr;
        const char *name;
    } reg_entry_t;
 
    static const reg_entry_t regs[] = {
        { 0x11, "PPG_CFG1  ADC_RGE/TINT" },
        { 0x12, "PPG_CFG2  SR/SMP_AVE"   },
        { 0x13, "PPG_CFG3  LED_SETLNG"   },
        { 0x15, "PD_BIAS"                },
        { 0x20, "LED_SEQ1"               },
        { 0x21, "LED_SEQ2"               },
        { 0x22, "LED_SEQ3"               },
        { 0x23, "LED1_PA"                },
        { 0x24, "LED2_PA"                },
        { 0x25, "LED3_PA"                },
        { 0x26, "LED4_PA"                },
        { 0x27, "LED5_PA"                },
        { 0x28, "LED6_PA"                },
        { 0x2A, "LED_RNG1"              },
        { 0x2B, "LED_RNG2"              },
        { 0x09, "FIFO_CFG1"             },
        { 0x0A, "FIFO_CFG2"             },
        { 0x0D, "SYS_CTRL"              },
        { 0xFF, "PART_ID"               },
    };
 
    uint8_t num_regs = sizeof(regs) / sizeof(regs[0]);
    uint8_t vals[NUM_MAX_IC];
    uint8_t mismatch_count = 0;
 
    NRF_LOG_INFO("========== MAX86141 REGISTER DUMP ==========");
    NRF_LOG_INFO("Register                    IC1   IC2   IC3   IC4   IC5   IC6   Match?");
    NRF_LOG_INFO("------------------------------------------------------------------------");
    NRF_LOG_FLUSH();
 
    for (uint8_t r = 0; r < num_regs; r++) {
        // Read this register from each IC using its own CS pin
        for (uint8_t ic = 0; ic < NUM_MAX_IC; ic++) {
            vals[ic] = spi_read_reg(PIN_CS_PPG[ic], regs[r].addr);
        }
 
        // Check if all ICs agree
        uint8_t all_match = 1;
        for (uint8_t ic = 1; ic < NUM_MAX_IC; ic++) {
            if (vals[ic] != vals[0]) {
                all_match = 0;
                break;
            }
        }
 
        if (!all_match) mismatch_count++;
        
        NRF_LOG_INFO("%-28s 0x%02X  0x%02X  0x%02X",
            regs[r].name, vals[0], vals[1], vals[2]);
        NRF_LOG_INFO("                             0x%02X  0x%02X  0x%02X  %s",
            vals[3], vals[4], vals[5],
            all_match ? "OK" : "** MISMATCH **");
        NRF_LOG_FLUSH();
    }
 
    NRF_LOG_INFO("------------------------------------------------------------------------");
    if (mismatch_count == 0) {
        NRF_LOG_INFO("All %d ICs match on every register.", NUM_MAX_IC);
    } else {
        NRF_LOG_INFO("WARNING: %d register(s) have mismatches!", mismatch_count);
    }
    NRF_LOG_INFO("============================================");
    NRF_LOG_FLUSH();
}
 
 
// ---------- CS Pin Connectivity Test ----------
// Writes a unique pattern to each IC, reads all back.
// This verifies each CS pin actually addresses a different IC.
// Uses PPG_SYNC_CONTROL (0x10) as scratch — you already use it,
// so we restore it to 0x02 when done.
 
void max86141_cs_pin_test(void)
{
    uint8_t test_patterns[NUM_MAX_IC] = { 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6 };
    uint8_t readback[NUM_MAX_IC];
    uint8_t errors = 0;
 
    NRF_LOG_INFO("--- CS Pin Connectivity Test ---");
    NRF_LOG_FLUSH();
 
    // Step 1: Write a UNIQUE pattern to each IC
    for (uint8_t ic = 0; ic < NUM_MAX_IC; ic++) {
        spi_write_reg(PIN_CS_PPG[ic], MAX86141_PPG_SYNC_CONTROL, test_patterns[ic]);
    }
 
    nrf_delay_ms(1);
 
    // Step 2: Read back from each IC
    for (uint8_t ic = 0; ic < NUM_MAX_IC; ic++) {
        readback[ic] = spi_read_reg(PIN_CS_PPG[ic], MAX86141_PPG_SYNC_CONTROL);
    }
 
    // Step 3: Check results
    for (uint8_t ic = 0; ic < NUM_MAX_IC; ic++) {
        uint8_t ok = (readback[ic] == test_patterns[ic]);
        if (!ok) errors++;
 
        NRF_LOG_INFO("  IC%d: wrote 0x%02X, read 0x%02X  %s",
            ic + 1, test_patterns[ic], readback[ic],
            ok ? "OK" : "** FAIL **");
    }
 
    // Step 4: Check for crossed wires — did any two ICs return the same value?
    for (uint8_t i = 0; i < NUM_MAX_IC; i++) {
        for (uint8_t j = i + 1; j < NUM_MAX_IC; j++) {
            if (readback[i] == readback[j] && readback[i] != 0x00) {
                NRF_LOG_INFO("  WARNING: IC%d and IC%d returned same value 0x%02X — possible shared CS!",
                    i + 1, j + 1, readback[i]);
                errors++;
            }
        }
    }
 
    NRF_LOG_FLUSH();
 
    if (errors == 0) {
        NRF_LOG_INFO("  All CS pins are independently wired. OK.");
    } else {
        NRF_LOG_INFO("  ** %d error(s) found — check wiring! **", errors);
    }
    NRF_LOG_INFO("--------------------------------");
    NRF_LOG_FLUSH();
 
    // Restore the register to your init value (0x02 = MUX control)
    spi_write_multi_reg(PIN_CS_PPG, MAX86141_PPG_SYNC_CONTROL, 0x02, NUM_MAX_IC);
}