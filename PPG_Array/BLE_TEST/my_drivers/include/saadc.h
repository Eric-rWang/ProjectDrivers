#include "nrf_drv_saadc.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_timer.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define SAADC_SAMPLE_RATE_HZ    1000              //Sampling frequency (Fs). Timer/PPI trigger the SAMPLE task at this rate.
#define SAADC_SAMPLES_IN_BUFFER 100               //Samples per DMA buffer. At 1 kHz this batches 100 ms of data per callback.
#define SAADC_BURST_MODE 0                        //Set to 1 to enable BURST mode, otherwise set to 0.

void timer_handler(nrf_timer_event_t event_type, void * p_context);
void saadc_sampling_event_init(void);
void saadc_sampling_event_enable(void);
void saadc_callback(nrf_drv_saadc_evt_t const * p_event);
void saadc_init(void);