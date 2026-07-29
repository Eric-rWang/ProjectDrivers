#include "nrf_drv_clock.h"
#include "app_timer.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_drv_gpiote.h"

#define ULTRASOUND_PIN_SR_CS          7
#define ULTRASOUND_PIN_PULSE          6
#define ULTRASOUND_PIN_FLOAT          5
#define ULTRASOUND_PIN_RXEN           8
#define ULTRASOUND_PIN_BOOST_EN       31


#define ULTRASOUND_PIN_PULSE_MASK     1UL << ULTRASOUND_PIN_PULSE
#define ULTRASOUND_PIN_FLOAT_MASK     1UL << ULTRASOUND_PIN_FLOAT
#define ULTRASOUND_PIN_RXEN_MASK      1UL << ULTRASOUND_PIN_RXEN
#define ULTRASOUND_PIN_BOOST_EN_MASK  1UL << ULTRASOUND_PIN_BOOST_EN

#define ULTRASOUND_PRF_HZ             50
#define ULTRASOUND_PRP_MS             1000 / ULTRASOUND_PRF_HZ
#define ULTRASOUND_TBLANK_US          2
#define ULTRASOUND_MAX_DEPTH_MM       50
#define ULTRASOUND_MIN_DEPTH_MM       5
#define ULTRASOUND_END_LISTEN_PERIOD_US   2 * 1000 * ULTRASOUND_MAX_DEPTH_MM / 1540
#define ULTRASOUND_START_LISTEN_PERIOD_US   2 * 1000 * ULTRASOUND_MIN_DEPTH_MM / 1540

#define ULTRASOUND_SECTOR_SIZE        4

//Digital potentiometer assignment
#define ULTRASOUND_RANGE_GATE_POT     1   //0 is A, 1 is B

#if ULTRASOUND_RANGE_GATE_POT ==      0
  #define ULTRASOUND_TX_WIDTH_POT     1
  #define ULTRASOUND_TX_WIDTH_REG     TPL0102_WRB_REG
  #define ULTRASOUND_RANGE_GATE_REG   TPL0102_WRA_REG
#else
  #define ULTRASOUND_TX_WIDTH_POT     1
  #define ULTRASOUND_TX_WIDTH_REG     TPL0102_WRA_REG
  #define ULTRASOUND_RANGE_GATE_REG   TPL0102_WRB_REG
#endif



//Testboard GPIOs for buttons.
#define BUTTON_1    13
#define BUTTON_2    14
#define BUTTON_3    15
#define BUTTON_4    16
void buttons_init(void);
static void highside_increment(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void highside_decrement(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void send_same_position(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

//#define ULTRASOUND_CALCULATE_MUX_POS(left, right) (((left & 0x07) << 1) | ((right & 0x07) << 4))
#define ULTRASOUND_CALCULATE_MUX_POS(pos) (1 << left) | (1 << (right + 8))


void ultrasound_init(void);
void ultrasound_start_measurement(void);
void ultrasound_pulse_and_receive(void);
static void lfclk_config(void);
static void create_timers();
void ultrasound_change_position(uint8_t position, uint8_t sector_size);
