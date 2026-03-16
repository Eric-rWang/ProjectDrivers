#include "nrf_drv_gpiote.h"
#include "ble_driver.h"

/////////////Hardware Pins//////////////////
#define PIN_CS_PPG						3
#define PIN_INT_PPG						15

////////////Pre-processor defines////////////
#define AS7343_SLAVE_ADDRESS                                    0x39 //0x29


void as7343_init(void);
void as7343_write_reg(uint8_t register_address, uint8_t data);
void as7343_write_two_reg(uint8_t register_address, uint8_t lsb, uint8_t msb);
uint8_t as7343_read_reg(uint8_t register_address);
void as7343_read_spectral_data(void);
void as7343_start_measurement(uint8_t num_measurements);
void as7343_dump_fifo(void);
bool as7343_check_gpio_trigger(void);
void as7343_sample_on_gpio_trigger(void);