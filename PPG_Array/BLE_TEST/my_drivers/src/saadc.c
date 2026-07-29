#include "saadc.h"
#include "ble_driver.h"

volatile uint8_t state = 1;

static const nrf_drv_timer_t m_timer = NRF_DRV_TIMER_INSTANCE(1);
static nrf_saadc_value_t     m_buffer_pool[2][SAADC_SAMPLES_IN_BUFFER];
static nrf_ppi_channel_t     m_ppi_channel;
static uint8_t               m_adc_evt_counter;


void timer_handler(nrf_timer_event_t event_type, void * p_context)
{

}


void saadc_sampling_event_init(void)
{
    ret_code_t err_code;

    err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
    timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
    err_code = nrf_drv_timer_init(&m_timer, &timer_cfg, timer_handler);
    APP_ERROR_CHECK(err_code);

    /* Setup m_timer to generate a compare event at Fs = SAADC_SAMPLE_RATE_HZ (period in microseconds). */
    uint32_t ticks = nrf_drv_timer_us_to_ticks(&m_timer, 1000000UL / SAADC_SAMPLE_RATE_HZ);
    nrf_drv_timer_extended_compare(&m_timer,
                                   NRF_TIMER_CC_CHANNEL0,
                                   ticks,
                                   NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                   false);
    nrf_drv_timer_enable(&m_timer);

    uint32_t timer_compare_event_addr = nrf_drv_timer_compare_event_address_get(&m_timer,
                                                                                NRF_TIMER_CC_CHANNEL0);
    uint32_t saadc_sample_task_addr   = nrf_drv_saadc_sample_task_get();

    /* setup ppi channel so that timer compare event is triggering sample task in SAADC */
    err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_assign(m_ppi_channel,
                                          timer_compare_event_addr,
                                          saadc_sample_task_addr);
    APP_ERROR_CHECK(err_code);
}


void saadc_sampling_event_enable(void)
{
    ret_code_t err_code = nrf_drv_ppi_channel_enable(m_ppi_channel);

    APP_ERROR_CHECK(err_code);
}


void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        ret_code_t err_code;

        /* Re-queue this buffer so EasyDMA keeps double-buffering while we transmit the batch. */
        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, SAADC_SAMPLES_IN_BUFFER);
        APP_ERROR_CHECK(err_code);

        /* Ship the whole batch over BLE. Each sample is a little-endian int16 holding a
           10-bit value (0..1023). send_data_nus() splits this into MTU-sized notifications. */
        send_data_nus((uint8_t *)p_event->data.done.p_buffer,
                      SAADC_SAMPLES_IN_BUFFER * sizeof(nrf_saadc_value_t));

        NRF_LOG_INFO("ADC batch %d sent (%d samples)", (int)m_adc_evt_counter, SAADC_SAMPLES_IN_BUFFER);
        m_adc_evt_counter++;
    }
}

/*
    AIN0: P0.02
    AIN1: P0.03
    AIN2: P0.04
    AIN3: P0.05
    AIN4: P0.28
    AIN5: P0.29
    AIN6: P0.30
    AIN7: P0.031

    */
void saadc_init(void)
{
    ret_code_t err_code;
    nrf_drv_saadc_config_t saadc_config;
    nrf_saadc_channel_config_t channel_config;


    //Configure SAADC
    saadc_config.low_power_mode = true;                                                   //Enable low power mode: SAADC only draws current while sampling.
    saadc_config.resolution = NRF_SAADC_RESOLUTION_10BIT;                                 //10-bit resolution: output values 0 (0V) .. 1023 (full-scale input).
    saadc_config.oversample = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;                         //Oversampling bypassed: one SAMPLE task -> one output value, so the SAADC samples at exactly Fs.
    saadc_config.interrupt_priority = APP_IRQ_PRIORITY_LOW;                               //Set SAADC interrupt to low priority.

    //Initialize SAADC
    err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);                         //Initialize the SAADC with configuration and callback function.
    APP_ERROR_CHECK(err_code);

    //Configure SAADC channel 0 on AIN1 (physical pin P0.03)
    channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;                              //Internal 0.6 V reference.
    channel_config.gain = NRF_SAADC_GAIN1_2;                                              //Gain 1/2 -> full-scale input = 0.6V / (1/2) = 1.2 V. 1023 counts == 1.2 V.
    channel_config.acq_time = NRF_SAADC_ACQTIME_40US;                                     //40 us acquisition time (well within the 1 ms sample period).
    channel_config.mode = NRF_SAADC_MODE_SINGLE_ENDED;                                    //Single-ended: negative input shorted to ground internally.
    channel_config.burst = SAADC_BURST_MODE ? NRF_SAADC_BURST_ENABLED : NRF_SAADC_BURST_DISABLED; //Burst off (only useful with oversampling).
    channel_config.pin_p = NRF_SAADC_INPUT_AIN1;                                          //Positive input = AIN1 = physical pin P0.03.
    channel_config.pin_n = NRF_SAADC_INPUT_DISABLED;                                      //No negative input (single-ended).
    channel_config.resistor_p = NRF_SAADC_RESISTOR_DISABLED;                              //Disable pullup resistor on the input pin.
    channel_config.resistor_n = NRF_SAADC_RESISTOR_DISABLED;                              //Disable pulldown resistor on the input pin.

    //Initialize SAADC channel
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);                            //Initialize SAADC channel 0 with the channel configuration.
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], SAADC_SAMPLES_IN_BUFFER);    //Set SAADC buffer 1. The SAADC will start to write to this buffer.
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], SAADC_SAMPLES_IN_BUFFER);    //Set SAADC buffer 2. Filled when buffer 1 is full, giving the app time to process buffer 1.
    APP_ERROR_CHECK(err_code);
}
