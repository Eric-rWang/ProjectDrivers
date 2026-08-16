/**
 * Copyright (c) 2014 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @brief AS6214 temperature sensor logger.
 *
 * Reads the ams AS6214 digital temperature sensor over I2C on the PCA10056
 * dev board (SCL = P0.27, SDA = P0.26), converts each raw sample to degrees
 * Celsius, and streams the result live as CSV rows over a dedicated SEGGER
 * RTT channel (channel 1), one sample every 10 seconds. NRF_LOG debug text
 * stays on channel 0; the CSV data channel is separate and never dropped.
 *
 * Capture the live CSV, stamped with the PC's wall-clock time per row, with
 * the host tool tools/rtt_temp_logger.py (see that file). To capture the raw
 * stream without wall-clock stamps instead, point the J-Link RTT Logger at
 * RTT channel 1 (a control-block search range is required - see HANDOFF.md):
 *   JLinkRTTLogger -Device NRF52840_XXAA -If SWD -Speed 4000 -RTTChannel 1 \
 *       -RTTSearchRanges "0x20000000 0x40000" temperature_raw.csv
 */

#include "nrf_delay.h"
#include "nrf_gpio.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "SEGGER_RTT.h"

#include "i2c_driver.h"
#include "as6214_driver.h"

/* Sample period, in milliseconds (requirement: one sample every 10 seconds). */
#define SAMPLE_PERIOD_MS    10000

/* Indicates if an operation on TWI has ended. Defined here because
 * i2c_driver.c references it as an extern (updated from the TWI handler). */
volatile bool m_xfer_done = false;

/**@brief Function for initializing the nrf log module. */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Application main function. */
int main(void)
{
    log_init();

    NRF_LOG_INFO("AS6214 temperature logger starting.");
    NRF_LOG_FLUSH();

    /* Bring up the I2C bus (SCL = P0.27, SDA = P0.26) and the sensor. */
    twi_init();
    as6214_init();

    if (!as6214_init_successful())
    {
        NRF_LOG_INFO("AS6214 not responding - halting. Check wiring/address.");
        NRF_LOG_FLUSH();
        for (;;)
        {
            /* Nothing to log without a working sensor. */
        }
    }

    /* Make sure the init log lines are flushed out of the RTT buffer before
     * the CSV stream begins, so the captured CSV file starts cleanly. */
    while (NRF_LOG_PROCESS())
    {
        /* Drain pending log messages. */
    }

    /* Configure the dedicated RTT data channel (channel 1), then emit the CSV
     * header and a baseline sample at t = 0 s. */
    as6214_csv_init();
    as6214_csv_header();

    uint32_t uptime_s = 0;
    as6214_sample_to_csv(uptime_s);

    /* Sample and stream one CSV row every 10 seconds. The I2C read is a
     * blocking (busy-wait) transfer, so it runs here in thread context.
     * uptime_s is nominal loop time; the host capture tool stamps each row
     * with the authoritative PC wall-clock time. */
    for (;;)
    {
        nrf_delay_ms(SAMPLE_PERIOD_MS);
        uptime_s += SAMPLE_PERIOD_MS / 1000;
        as6214_sample_to_csv(uptime_s);
    }
}
