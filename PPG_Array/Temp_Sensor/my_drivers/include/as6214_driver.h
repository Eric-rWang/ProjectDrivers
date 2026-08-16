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

/**@brief Configure the dedicated RTT up-buffer used for the CSV stream.
 *
 * The CSV is streamed on its own RTT channel (channel 1), kept separate from
 * the NRF_LOG debug backend on channel 0, and runs in BLOCK_IF_FIFO_FULL mode
 * so that no sample row is ever silently dropped while a host logger is
 * attached. Call once, before as6214_csv_header(). */
void as6214_csv_init(void);

/**@brief Initialize the AS6214. Verifies the device responds on the bus.
 *        twi_init() must have been called first. */
void as6214_init(void);

/**@brief @return true if as6214_init() confirmed a working device. */
bool as6214_init_successful(void);

/**@brief Read the raw 16-bit two's-complement temperature register, retrying a
 *        few times on a transient bus error.
 * @param[out] raw  Signed raw sample (MSB byte read first, then LSB), valid
 *                  only when the function returns true.
 * @return true on a successful read, false if the bus never responded. */
bool as6214_read_raw(int16_t *raw);

/**@brief Convert a raw temperature register value to degrees Celsius. */
float as6214_raw_to_celsius(int16_t raw);

/**@brief Read the sensor and return the temperature in degrees Celsius.
 * @param[out] celsius  Temperature, valid only when the function returns true.
 * @return true on a successful read, false on a bus error. */
bool as6214_read_celsius(float *celsius);

/**@brief Print the CSV header row ("uptime_s,temperature_C") to the RTT
 *        data channel. Call once before streaming samples. The host capture
 *        tool prepends a "pc_time" wall-clock column of its own. */
void as6214_csv_header(void);

/**@brief Read one sample and print it as a CSV row to the RTT data channel:
 *        "<uptime_s>,<temperature_C>".
 * @param[in] uptime_s  Seconds of on-target uptime since sampling started
 *                      (nominal loop time; the authoritative wall-clock
 *                      timestamp is added by the host on capture). */
void as6214_sample_to_csv(uint32_t uptime_s);

#endif /* AS6214_DRIVER_H */
