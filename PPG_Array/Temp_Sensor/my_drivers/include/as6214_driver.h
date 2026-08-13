#ifndef AS6214_DRIVER_H
#define AS6214_DRIVER_H

/**
 * @file as6214_driver.h
 * @brief Driver for the ams AS6214 digital temperature sensor (I2C).
 *
 * Datasheet: AS621x DS000677 v6-00.
 *   - 16-bit two's-complement temperature, MSB byte transmitted first.
 *   - 1 LSB = 0.0078125 degC (1/128 degC).
 *   - Default configuration = continuous conversion at 4 conv/s, which is
 *     more than sufficient for the 10 s sampling used here.
 *
 * Hardware wiring on this board (see i2c_driver.h):
 *   SCL = P0.27, SDA = P0.26. ADD0 and ADD1 tied to VSS -> address 0x48.
 */

#include "i2c_driver.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include <stdint.h>
#include <stdbool.h>

/* 7-bit I2C slave address. ADD1 = VSS, ADD0 = VSS -> 0x48 (datasheet Table 22). */
#define AS6214_SLAVE_ADDRESS    0x48

/* Register pointer (index register) values (datasheet Table 10). */
#define AS6214_REG_TVAL         0x00    /* Temperature value  (read only) */
#define AS6214_REG_CONFIG       0x01    /* Configuration register         */
#define AS6214_REG_TLOW         0x02    /* Low temperature threshold      */
#define AS6214_REG_THIGH        0x03    /* High temperature threshold     */

/* 1 LSB of the temperature register in degrees Celsius (= 1/128 degC). */
#define AS6214_LSB_CELSIUS      0.0078125f

/* Reserved config bit 14 reads back as 1 on a healthy device (Table 11);
 * used as a lightweight presence/sanity check during init. */
#define AS6214_CONFIG_RSVD_BIT  0x4000

/**@brief Initialize the AS6214. Verifies the device responds on the bus.
 *        twi_init() must have been called first. */
void as6214_init(void);

/**@brief @return true if as6214_init() confirmed a working device. */
bool as6214_init_successful(void);

/**@brief Read the raw 16-bit two's-complement temperature register.
 * @return Signed raw sample (MSB byte read first, then LSB). */
int16_t as6214_read_raw(void);

/**@brief Convert a raw temperature register value to degrees Celsius. */
float as6214_raw_to_celsius(int16_t raw);

/**@brief Read the sensor and return the temperature in degrees Celsius. */
float as6214_read_celsius(void);

/**@brief Print the CSV header row ("time_s,temperature_C") to the RTT
 *        terminal. Call once before streaming samples. */
void as6214_csv_header(void);

/**@brief Read one sample and print it as a CSV row to the RTT terminal:
 *        "<timestamp_s>,<temperature_C>".
 * @param[in] timestamp_s  Seconds since sampling started (row time stamp). */
void as6214_sample_to_csv(uint32_t timestamp_s);

#endif /* AS6214_DRIVER_H */
