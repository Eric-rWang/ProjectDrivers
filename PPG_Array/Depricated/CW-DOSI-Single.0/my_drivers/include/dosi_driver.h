#include "nrf_drv_clock.h"
#include "app_timer.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/////////////Hardware Pins//////////////////
#define PIN_DOSI_A0						30
#define PIN_DOSI_A1						29
#define PIN_DOSI_A2                                             28  
#define PIN_DOSI_TRIGGER                                        27
#define PIN_DOSI_INT                                            9

////////////Pre-processor defines////////////
#define DOSI_SAMPLE_RATE_HZ                                     0.5
#define DOSI_SAMPLE_PERIOD_MS                                   1000 / DOSI_SAMPLE_RATE_HZ

////////////Functions///////////////////////
void dosi_init(void);
void dosi_start_measurement(void);
static void lfclk_config(void);
static void create_timers();