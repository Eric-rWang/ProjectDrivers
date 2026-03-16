#include "nrf_drv_spi.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include <stdint.h>

#define SPI_INSTANCE  0 /**< SPI instance index. */

#define PIN_MOSI						5
#define PIN_MISO						8
#define PIN_SCKL						12

static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static volatile bool spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */

void spi_init(void);
uint8_t spi_read_reg(uint8_t cs_pin, uint8_t address);
void spi_write_reg(uint8_t cs_pin, uint8_t address, uint8_t data);
void spi_burst_read(uint8_t cs_pin, uint8_t address, uint8_t* rx_buf, uint8_t num_bytes);