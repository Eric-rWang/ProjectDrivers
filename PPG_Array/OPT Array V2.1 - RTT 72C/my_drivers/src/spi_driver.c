#include "spi_driver.h"
#include "nrf_drv_gpiote.h"

static bool CS_trig = false;

#define SPI_MOSI_PIN 19
#define SPI_MISO_PIN 20
#define SPI_SCK_PIN 18


static void spi_event_handler(nrf_drv_spi_evt_t const* p_event, void* p_context)
{
	spi_xfer_done = true;
}


void spi_init(void) {
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.miso_pin		= SPI_MISO_PIN;
    spi_config.mosi_pin		= SPI_MOSI_PIN;
    spi_config.sck_pin		= SPI_SCK_PIN;
    spi_config.frequency	= NRF_DRV_SPI_FREQ_4M;	//Maximum for MAX86141 is 4MHz
    spi_config.mode			= NRF_DRV_SPI_MODE_0;	//Clock is idle low, and data is read in the MAX chip on the rising edge, and written on the falling edge. From my understanding it should be mode 1 but it seems like only mode 0 works.
    //spi_config.bit_order	= NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;
    spi_config.irq_priority = APP_IRQ_PRIORITY_HIGH;

    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));
	NRF_LOG_INFO("SPI initialized successfully");
	NRF_LOG_FLUSH();
}

/*
 * @brief Function to read one byte from a certain SPI slave at the specified register address.
 * @param cs_pin Which chip select pin to pull low
 * @param address 8-bit register address
 * @param rx_buf Address of where to save the data
 */
uint8_t spi_read_reg(uint8_t cs_pin, uint8_t address) {
	spi_xfer_done = false;
	
	//Bring cs_pin low
	nrf_gpio_pin_clear(cs_pin);

	//Send the register address and set the read bit
	uint8_t tx_buf[2] = {address, 0xFF};
	uint8_t m_rx_len = 3;
	uint8_t m_rx_buf[m_rx_len];

	//Start SPI transfer
	APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, tx_buf, 2, m_rx_buf, m_rx_len));

	//Wait until transfer is complete
	while (!spi_xfer_done) { __WFE(); }

	//Bring cs_pin high
	nrf_gpio_pin_set(cs_pin);

	return m_rx_buf[2];

}

uint8_t* spi_read_multi_reg(uint8_t* cs_pins, uint8_t address, uint8_t num_pins) {
        static uint8_t result_array[6]; // Static array to hold results

        uint8_t m_rx_len = 3; // Length of the receive buffer
        uint8_t m_rx_buf[m_rx_len]; // Receive buffer

        for(int i = 0; i < num_pins; i++) {
                // Store the result in the result_array
                result_array[i] = spi_read_reg(cs_pins[i], address);
        }

        return result_array;
}

/*
 * @brief Function to write one byte to a specific register address
 * @param cs_pin Which chip select pin to pull low
 * @param address 8-bit register address
 * @param data Byte to write at address
 */
void spi_write_reg(uint8_t cs_pin, uint8_t address, uint8_t data) {
	spi_xfer_done = false;

	//Bring cs_pin low
	nrf_gpio_pin_clear(cs_pin);

	//Set the register address, read bit is zero, and the data  byte
	uint8_t tx_buf[3] = {address, 0, data};

	//Start the SPI transfer
	APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, tx_buf, 3, NULL, 0));

	//Wait until transfer is complete
	while (!spi_xfer_done) { __WFE(); }

	//Bring cs_pin high
	nrf_gpio_pin_set(cs_pin);

}

void spi_write_multi_reg(uint8_t* cs_pins, uint8_t address, uint8_t data, uint8_t num_pins) {
        for (int i = 0; i < num_pins; i++) {
                spi_write_reg(cs_pins[i], address, data);
        }
}

/*
 * @brief Function to burst read several bytes from a certain SPI slave at the specified register address.
 * @param cs_pin Which chip select pin to pull low
 * @param address 8-bit register address
 * @param rx_buf Address of where to save the data
 * @param num_bytes How many byte to burst read
 */
void spi_burst_read(uint8_t cs_pin, uint8_t address, uint8_t* rx_buf, uint8_t num_bytes) {
	spi_xfer_done = false;
	
	//Bring cs_pin low
	nrf_gpio_pin_clear(cs_pin);

	//Send the register address and set the read bit
	uint8_t tx_buf[2] = {address, 0x80};
	uint8_t m_rx_buf[num_bytes + 2];

	//Start SPI transfer
	APP_ERROR_CHECK(nrf_drv_spi_transfer(&spi, tx_buf, 2, m_rx_buf, num_bytes + 2));

	//Wait until transfer is complete
	while (!spi_xfer_done) { __WFE(); }

	//Bring cs_pin high
	nrf_gpio_pin_set(cs_pin);

	//Copy the recieved data (minus the first 2 bytes) to the output buffer (rx_buf)
	memcpy(rx_buf, &m_rx_buf[2], num_bytes);

}
