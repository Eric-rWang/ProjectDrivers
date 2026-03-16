#include "nrf_drv_gpiote.h"
#include "ble_driver.h"

/////////////Hardware Pins//////////////////
#define PIN_CS_PPG						3
#define PIN_INT_PPG						15

////////////Pre-processor defines////////////
#define AS7343_SLAVE_ADDRESS                                    0x39 //0x29


void as7343_init(void);
void as7343_write_reg(uint8_t register_address, uint8_t data);
uint8_t as7343_read_reg(uint8_t register_address);
void as7343_gpio_interrupt_handler(void);
void read_spectral_data(void);
bool as7343_check_gpio_status(void);
void as7343_single_shot_sample(void);