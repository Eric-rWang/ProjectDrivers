#include "i2c_driver.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/* Indicates if operation on TWI has ended. */
extern volatile bool m_xfer_done;

/* Set by the TWI handler when a transfer ends in a NACK or an unexpected
 * event, so the blocking read helpers can report failure instead of trusting
 * whatever happened to be in the buffer. */
static volatile bool m_xfer_error = false;

/* Upper bound on how long a single TWI transfer may take before we give up.
 * A 2-byte transfer at 100 kHz is well under 1 ms; 100 ms is generous and
 * guarantees a missing DONE event (e.g. sensor unplugged) can never hang the
 * logger in an infinite busy-wait. */
#define TWI_XFER_TIMEOUT_US     100000

/**
 * @brief Wait for the TWI handler to signal completion, bounded by a timeout.
 * @return true if the transfer completed, false if it timed out.
 */
static bool twi_wait_done(void)
{
    uint32_t remaining_us = TWI_XFER_TIMEOUT_US;
    while (!m_xfer_done && remaining_us > 0)
    {
        nrf_delay_us(10);
        remaining_us -= 10;
    }
    return m_xfer_done;
}

/* TWI instance. The nRF52811 has a single TWI instance (0), so the AS6214
 * temperature sensor uses TWI0. */
#undef  TWI_INSTANCE_ID
#define TWI_INSTANCE_ID 0
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

static uint8_t temp_data[2];

/**
 * @brief Function for handling data from temperature sensor.
 *
 * @param[in] temp          Temperature in Celsius degrees read from sensor.
 */
__STATIC_INLINE void data_handler(uint16_t data)
{  
    NRF_LOG_INFO("I2C data 0x%X", data);
    NRF_LOG_FLUSH();
}


/**
 * @brief Function to write to register over I2C
 */
void twi_write(uint8_t slave_address, uint8_t reg_and_data[], uint8_t size, bool no_stop) {
	ret_code_t err_code;
	uint8_t num_fails = 0;
	
	m_xfer_done = false;
	
	err_code = nrf_drv_twi_tx(&m_twi, slave_address, reg_and_data, size, no_stop);

	if (NRF_SUCCESS != err_code) {
		num_fails ++;
		NRF_LOG_INFO("TWI write failed (%d times).", num_fails);
	} 

	NRF_LOG_FLUSH();

	while (m_xfer_done == false) {}												
}

/**
 * @brief Function to read a register over I2C.
 *
 * Fault-tolerant: never faults the CPU and never spins forever. A failed
 * transfer start, a bus NACK, or a timeout all return false, leaving the
 * caller to skip or retry the sample rather than halting the logger.
 *
 * @return true if the requested bytes were read successfully, false otherwise.
 */
bool twi_read(uint8_t slave_address, uint8_t register_address, uint8_t output_array[], uint8_t bytes_to_read) {
    ret_code_t err_code;
	uint8_t reg[1] = {register_address};

	/* 1. Send the register pointer (write mode). Repeated start follows, so
	 *    do not send a stop condition. */
    m_xfer_done  = false;
    m_xfer_error = false;
    err_code = nrf_drv_twi_tx(&m_twi, slave_address, reg, sizeof(reg), true);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_INFO("TWI read: tx start failed (0x%X).", err_code);
        NRF_LOG_FLUSH();
        return false;
    }
    if (!twi_wait_done()) {
        NRF_LOG_INFO("TWI read: tx timed out.");
        NRF_LOG_FLUSH();
        return false;
    }
    if (m_xfer_error) {
        return false;   /* NACK already logged in the handler. */
    }

    /* 2. Repeated start. Send device address and read mode. Read and stop. */
    m_xfer_done  = false;
    m_xfer_error = false;
    err_code = nrf_drv_twi_rx(&m_twi, slave_address, output_array, bytes_to_read);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_INFO("TWI read: rx start failed (0x%X).", err_code);
        NRF_LOG_FLUSH();
        return false;
    }
    if (!twi_wait_done()) {
        NRF_LOG_INFO("TWI read: rx timed out.");
        NRF_LOG_FLUSH();
        return false;
    }
    return !m_xfer_error;
}




/**
 * @brief TWI events handler.
 */
void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:           
            if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX) {
                //NRF_LOG_INFO("RX");
				uint16_t m_sample = temp_data[0] | (temp_data[1] << 8);
				//data_handler(m_sample);
            } else if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_TX) {
                //NRF_LOG_INFO("Successul TX");
            } else if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_TXRX) {
                //NRF_LOG_INFO("TXRX");
            } else if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_TXTX) {
                //
				NRF_LOG_INFO("TXTX");
            } else {
            
            }           
            
            break;
		case NRF_DRV_TWI_EVT_ADDRESS_NACK:
			NRF_LOG_INFO("Address NACK");
			NRF_LOG_FLUSH();
			m_xfer_error = true;
			break;
		case NRF_DRV_TWI_EVT_DATA_NACK :
			NRF_LOG_INFO("Data NACK");
			NRF_LOG_FLUSH();
			m_xfer_error = true;
			break;
        default:
            NRF_LOG_INFO("Data not handled");
			NRF_LOG_FLUSH();
			m_xfer_error = true;
            break;
    }
	m_xfer_done = true;
}

/**
 * @brief TWI initialization.
 */
void twi_init (void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t twi_config = {
       .scl                = SCL_PIN,
       .sda                = SDA_PIN,
       .frequency          = NRF_DRV_TWI_FREQ_100K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = true
    };

    err_code = nrf_drv_twi_init(&m_twi, &twi_config, twi_handler, NULL);
    if(err_code != NRF_SUCCESS) {
        NRF_LOG_INFO("TWI initialization was not successful.");
        NRF_LOG_FLUSH();
    } else {
        NRF_LOG_INFO("TWI initialization was successful.");
        NRF_LOG_FLUSH();
        nrf_drv_twi_enable(&m_twi);

		//twi_scan();
    }
}

//Not used. 
void twi_scan (void)
{
    ret_code_t err_code;
    uint8_t address;
    uint8_t sample_data;
    bool detected_device = false;

	NRF_LOG_INFO("Starting scan...");


    for (address = 1; address <= TWI_ADDRESSES; address++)
    {
        err_code = nrf_drv_twi_rx(&m_twi, address, &sample_data, sizeof(sample_data));
        
        if (err_code == NRF_SUCCESS)
        {
            detected_device = true;
            NRF_LOG_INFO("TWI device detected at address 0x%x.", address);
            NRF_LOG_INFO("SAMPLE DATA: %d", sample_data); 
        }
        
        NRF_LOG_FLUSH();
        
    }

    if (!detected_device)
    {
        NRF_LOG_INFO("No device was found.");
        NRF_LOG_FLUSH();
    }

	NRF_LOG_INFO("Scan complete.");
	NRF_LOG_FLUSH();
}
