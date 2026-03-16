#include "dosi_driver.h"
#include "as7343_driver.h"
#include "spi_driver.h"
#include "helper_functions.h"

//Supposedly using TIMER1 will cause a current consumption of 1mA
//https://devzone.nordicsemi.com/f/nordic-q-a/188/timer-configuration-with-softdevice
//Unfortunately RTC only has a resolution of 30.517 us (32.768 kHz)
APP_TIMER_DEF(m_repeated_timer_id);

const uint8_t mux_pos[8] = {0, 1, 2, 3, 7, 6, 5, 4};

uint8_t position = 0;
void dosi_init() {
    NRF_LOG_INFO("Starting initialization...");

    //Configure GPIOS
    nrf_gpio_cfg_output(PIN_DOSI_A0);
    nrf_gpio_cfg_output(PIN_DOSI_A1);
    nrf_gpio_cfg_output(PIN_DOSI_A2);
    nrf_gpio_cfg_output(PIN_DOSI_TRIGGER);
    
    //Configure the AS7343s
    //For now, set them all to have the same settings (ext trigger, LED driver enabled)
    //for (int i = 0; i < 8; i ++) {
    //    NRF_LOG_INFO("Initializing AS7343 %i", i);

    //    nrf_gpio_pin_write(PIN_DOSI_A0, (i & 0x01));
    //    nrf_gpio_pin_write(PIN_DOSI_A1, (i & 0x02) >> 1);
    //    nrf_gpio_pin_write(PIN_DOSI_A2, (i & 0x04) >> 2);

    //    as7343_init();
    //}

    as7343_init();

    //Reset the MUXes to position 0
    //nrf_gpio_pin_write(PIN_DOSI_A0, 0);
    //nrf_gpio_pin_write(PIN_DOSI_A1, 0);
    //nrf_gpio_pin_write(PIN_DOSI_A2, 0);

    //Configure a timer to set a repeating task
    //lfclk_config(); //Enable if SoftDevice not enabled.
    create_timers();
    ret_code_t err_code = app_timer_start(m_repeated_timer_id, APP_TIMER_TICKS(DOSI_SAMPLE_PERIOD_MS), NULL);
    //Stop with app_timer_stop(m_repeated_timer_id);
    APP_ERROR_CHECK(err_code);
    
    NRF_LOG_INFO("DOSI initialization complete.");
    NRF_LOG_FLUSH();
}

static void lfclk_config(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_clock_lfclk_request(NULL);
    app_timer_init();
}

static void repeated_timer_handler(void * p_context)
{
    dosi_start_measurement();
    //NRF_LOG_INFO("Measurement made.");
    //NRF_LOG_FLUSH();
}

static void create_timers()
{
    ret_code_t err_code;

    // Create timers
    err_code = app_timer_create(&m_repeated_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                repeated_timer_handler);
    APP_ERROR_CHECK(err_code);
}

void dosi_start_measurement()
{
    nrf_gpio_pin_set(PIN_DOSI_TRIGGER);
    
    //NRF_LOG_INFO("position %i", position % 8);
    //as7343_dump_fifo();
    //Change the position of the LED
    //position ++;
    position = 0;
    
    as7343_sample_on_gpio_trigger();
    nrf_delay_ms(10);
    
    //Update the MUXes to represent the new position
    //nrf_gpio_pin_write(PIN_DOSI_A0, (mux_pos[position % 8] & 0x01));
    //nrf_gpio_pin_write(PIN_DOSI_A1, (mux_pos[position % 8] & 0x02) >> 1);
    //nrf_gpio_pin_write(PIN_DOSI_A2, (mux_pos[position % 8] & 0x04) >> 2);

    //NRF_LOG_INFO("%i, %i, %i", (position & 0x01), (position & 0x02) >> 1, (position & 0x04) >> 2);

    //Trigger measurement
    nrf_gpio_pin_clear(PIN_DOSI_TRIGGER);

    //nrf_delay_ms(50);
    
}