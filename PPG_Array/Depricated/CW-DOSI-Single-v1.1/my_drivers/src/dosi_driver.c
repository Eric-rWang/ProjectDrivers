#include "dosi_driver.h"
#include "as7343_driver.h"
#include "spi_driver.h"
#include "helper_functions.h"

//Supposedly using TIMER1 will cause a current consumption of 1mA
//https://devzone.nordicsemi.com/f/nordic-q-a/188/timer-configuration-with-softdevice
//Unfortunately RTC only has a resolution of 30.517 us (32.768 kHz)
APP_TIMER_DEF(m_repeated_timer_id);

void dosi_init() {
    NRF_LOG_INFO("Starting initialization...");

    //Configure GPIOS
    nrf_gpio_cfg_output(PIN_DOSI_TRIGGER);

    as7343_init();

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
    
    
    nrf_delay_ms(100);


    nrf_gpio_pin_clear(PIN_DOSI_TRIGGER);

    
}