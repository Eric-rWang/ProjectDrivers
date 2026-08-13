#include "boards.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "nrf_drv_twi.h"

#ifndef USE_TEMP_SENSOR
#define USE_TEMP_SENSOR 1
#endif


/* TWI instance ID. */
#if TWI0_ENABLED
#define TWI_INSTANCE_ID     0
#elif TWI1_ENABLED
#define TWI_INSTANCE_ID     1
#endif

 /* Number of possible TWI addresses for the scan function. */
 #define TWI_ADDRESSES      127

/* AS6214 temperature sensor I2C pins (PCA10056):
 *   SCL = P0.27, SDA = P0.26 */
#define SCL_PIN             27
#define SDA_PIN             26

void twi_init (void);
void twi_scan (void);
void twi_write(uint8_t slave_address, uint8_t reg_and_data[], uint8_t size, bool no_stop);
void twi_read(uint8_t slave_address, uint8_t register_address, uint8_t output_array[], uint8_t bytes_to_read);

