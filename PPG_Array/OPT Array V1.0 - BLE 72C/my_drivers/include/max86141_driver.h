#include "nrf_drv_gpiote.h"
#include "ble_driver.h"

/////////////Hardware Pins//////////////////
#define PIN_CS_PPG1						3
#define PIN_INT_PPG                                             17

#define NUM_MAX_IC                                              6              
#define SPI_CS_1                                                    11
#define SPI_CS_2                                                    12
#define SPI_CS_3                                                    13
#define SPI_CS_4                                                    14
#define SPI_CS_5                                                    15
#define SPI_CS_6                                                    16

// SAMPLING_PATTERN
// 0: Diagonals Only
// 1: Across Only
// 2: ALL
#define SAMPLING_PATTERN                                        2

////////////Constant Variables //////////////
extern uint8_t PPG_CS_PPG[NUM_MAX_IC];
#define FIFO_TIMEOUT_RETRIES 1000                 // Adjust this if SPI is exceptionally fast/slow

////////////Pre-processor defines////////////
#define MAX86141_FIFO_SAMPLES			12 //MAX86141_PACKET_SIZE_BYTES / 3		//3 bytes per sample       originally 12
#define MAX86141_PACKET_SIZE_BYTES              MAX86141_FIFO_SAMPLES * 3 
#define MAX86141_A_FULL				128 - MAX86141_FIFO_SAMPLES
#define MAX86141_DIE_TEMP_THRESH		44	//degrees celsius
#define NRF_DIE_TEMP_THRESH			48	//degrees celsius. The accuracy of the nRF temp sensor is much worse (+/- 5C) and can be expected to be a bit warmer

//#define MAX86141_A_FULL					0x10
//#define MAX86141_FIFO_SAMPLES			128 - MAX86141_A_FULL
//128 - 108 = 20 samples

////////////////Register Map/////////////////
#define MAX86141_INTERRUPT_STATUS_1		0x00
#define MAX86141_INTERRUPT_STATUS_2		0x01
#define MAX86141_INTERRUPT_ENABLE_1		0x02
#define MAX86141_INTERRUPT_ENABLE_2		0x03
//FIFO Registers
#define MAX86141_FIFO_WRITE_POINTER		0x04
#define MAX86141_FIFO_READ_POINTER		0x05
#define MAX86141_OVERFLOW_COUNTER		0x06
#define MAX86141_FIFO_DATA_COUNTER		0x07
#define MAX86141_FIFO_DATA_REGISTER		0x08
#define MAX86141_FIFO_CONFIGURATION_1           0x09
#define MAX86141_FIFO_CONFIGURATION_2           0x0A
//System Control
#define MAX86141_SYSTEM_CONTOL			0x0D
//PPG Configuration
#define MAX86141_PPG_SYNC_CONTROL		0x10
#define MAX86141_PPG_CONFIGURATION_1            0x11
#define MAX86141_PPG_CONFIGURATION_2            0x12
#define MAX86141_PPG_CONFIGURATION_3            0x13
#define MAX86141_PROX_INTERRUPT_THRESH          0x14
#define MAX86141_PHOTODIODE_BIAS		0x15
//PPG Picket Fence Detect and Replace
#define MAX86141_PISCKET_FENCE			0x16
//LED Sequence Control
#define MAX86141_LED_SEQUENCE_REG_1		0x20
#define MAX86141_LED_SEQUENCE_REG_2		0x21
#define MAX86141_LED_SEQUENCE_REG_3		0x22
//LED Pulse Amplitude
#define MAX86141_LED1_PA			0x23
#define MAX86141_LED2_PA			0x24
#define MAX86141_LED3_PA			0x25
#define MAX86141_LED4_PA			0x26
#define MAX86141_LED5_PA			0x27
#define MAX86141_LED6_PA			0x28
#define MAX86141_LED_PILOT_PA			0x29
#define MAX86141_LED_RANGE_1			0x2A
#define MAX86141_LED_RANGE_2			0x2B
//PPG1_HI_RES_DAC
#define MAX86141_S1_HI_RES_DAC			0x2C
#define MAX86141_S2_HI_RES_DAC			0x2D
#define MAX86141_S3_HI_RES_DAC			0x2E
#define MAX86141_S4_HI_RES_DAC			0x2F
#define MAX86141_S5_HI_RES_DAC			0x30
#define MAX86141_S6_HI_RES_DAC			0x31
//PPG2_HI_RES_DAC
#define MAX86141_S1_HI_RES_DAC2			0x32
#define MAX86141_S2_HI_RES_DAC2			0x33
#define MAX86141_S3_HI_RES_DAC2			0x34
#define MAX86141_S4_HI_RES_DAC2			0x35
#define MAX86141_S5_HI_RES_DAC2			0x36
#define MAX86141_S6_HI_RES_DAC2			0x37
//Die Temperature
#define MAX86141_DIE_TEMP_CONFIG		0x40
#define MAX86141_DIE_TEMP_INTEGER		0x41
#define MAX86141_DIE_TEMP_FRACTION		0x42
//SHA256
#define MAX86141_SHA_COMMAND			0xF0
#define MAX86141_SHA_CONFIG			0xF1
//Memory
#define MAX86141_MEMORY_CONTROL			0xF2
#define MAX86141_MEMORY_INDEX			0xF3
#define MAX86141_MEMORY_DATA			0xF4
//Part ID
#define MAX86141_PART_ID			0xFF

/*
 *@brief Function to get cs pins.
 */
const uint8_t* max86141_get_cs_pins(void);

/*
 *@brief Function to setup the internal registers in the MAX86141
 */
void max86141_init(void);

/*
 *@brief Function to set all CS Pins to high
 */
void max86141_cs_set(void);

/*
 *@brief Function to set all CS Pins to low
 */
void max86141_cs_clear(void);

/*
 *@brief Function to setup the GPIOs in the nRF to detect interrupts from the MAX86141
 */
void max86141_setup_interrupts(void);

/*
 *@brief Interrupt handler for MAX86141
 */
void max86141_interrupt_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

/*
 *@brief Function for reading data from the FIFO of the MAX86141
 */

void max86141_flush_all_fifos(void);

void max86141_process_data(void);

bool max86141_fifo_parser(uint8_t PIN_CS_PPG);

void max86141_fifo_multi_parser(uint8_t* PINS_CS_PPG,  uint8_t num_pins);

bool max86141_fetch_filtered_data(uint8_t cs_pin, uint8_t* out_buf);

void dump_fifo_data(uint8_t *data_buf, uint16_t num_bytes);

/*
 *@brief Function that starts a die temperature measurement.
 */
void max86141_start_die_temp_measurement(void);

/*
 *@brief Function that checks if a temperature reading is ready.
 */
void check_die_temperatures();

/*
 *@brief Function that configures cs pins to gpio output and set high.
 */
void max86141_config_pin(void);

void max86141_dump_all_ic_registers(void);

void max86141_cs_pin_test(void);