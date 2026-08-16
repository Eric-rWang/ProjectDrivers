#include "as6214_driver.h"
#include "SEGGER_RTT.h"

static bool as6214_init_success = false;

/* Dedicated RTT up-buffer used for the live CSV stream. Channel 0 is left to
 * the NRF_LOG RTT backend (debug text); the CSV lives on its own channel so
 * the two never share a buffer or interleave. Capture THIS channel (1) with
 * the host tool / J-Link RTT Logger to obtain a clean .csv. */
#define AS6214_RTT_CSV_CHANNEL      1

/* Backing storage for the CSV up-buffer. Sized for generous pre-attach
 * headroom: a data row is ~25 bytes, so ~40 rows can queue before the host
 * logger has to be attached. Once attached and draining, it never fills. */
#define AS6214_RTT_CSV_BUFFER_SIZE  1024
static char as6214_rtt_csv_buffer[AS6214_RTT_CSV_BUFFER_SIZE];

void as6214_csv_init(void)
{
    /* BLOCK_IF_FIFO_FULL guarantees CSV integrity: a sample row is never
     * partially written or silently skipped. If no host is attached, the
     * buffer fills and sampling stalls until a logger drains it - the correct
     * trade-off for a data logger, where losing rows is worse than pausing. */
    SEGGER_RTT_ConfigUpBuffer(AS6214_RTT_CSV_CHANNEL, "TEMP_CSV",
                              as6214_rtt_csv_buffer,
                              sizeof(as6214_rtt_csv_buffer),
                              SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
}

void as6214_init(void)
{
    /* The AS6214 has no device-ID register, so verify communication by reading
     * the configuration register. A device that ACKs and returns its fixed
     * reserved bit (bit 14 = 1) is present and sane. A floating bus reads back
     * 0xFFFF. Config bytes are transmitted MSB first. */
    uint8_t cfg[2];
    if (!twi_read(AS6214_SLAVE_ADDRESS, AS6214_REG_CONFIG, cfg, 2))
    {
        NRF_LOG_INFO("AS6214 initialization FAILED (no I2C response).");
        NRF_LOG_FLUSH();
        return;
    }

    uint16_t config = ((uint16_t)cfg[0] << 8) | cfg[1];

    if (config == 0xFFFF || (config & AS6214_CONFIG_RSVD_BIT) == 0)
    {
        NRF_LOG_INFO("AS6214 initialization FAILED (config = 0x%04X).", config);
        NRF_LOG_FLUSH();
        return;
    }

    /* Default configuration already runs continuous conversion at 4 conv/s,
     * which comfortably covers the 10 s sample period. No reconfiguration
     * required. */
    as6214_init_success = true;
    NRF_LOG_INFO("AS6214 initialized successfully (config = 0x%04X).", config);
    NRF_LOG_FLUSH();
}

bool as6214_init_successful(void)
{
    return as6214_init_success;
}

/* Number of times a single temperature read is retried before it is reported
 * as failed, to ride out transient bus glitches. */
#define AS6214_READ_RETRIES     3

bool as6214_read_raw(int16_t *raw)
{
    uint8_t samples[2];

    for (uint8_t attempt = 0; attempt < AS6214_READ_RETRIES; attempt++)
    {
        if (twi_read(AS6214_SLAVE_ADDRESS, AS6214_REG_TVAL, samples, 2))
        {
            /* MSB byte is read first, then the LSB byte (datasheet section
             * 6.2.10). The result is a signed 16-bit two's-complement value. */
            *raw = (int16_t)(((uint16_t)samples[0] << 8) | samples[1]);
            return true;
        }
        nrf_delay_ms(2);   /* Brief settle before retrying. */
    }

    return false;
}

float as6214_raw_to_celsius(int16_t raw)
{
    /* 1 LSB = 0.0078125 degC = 1/128 degC. */
    return (float)raw * AS6214_LSB_CELSIUS;
}

bool as6214_read_celsius(float *celsius)
{
    int16_t raw;
    if (!as6214_read_raw(&raw))
    {
        return false;
    }
    *celsius = as6214_raw_to_celsius(raw);
    return true;
}

void as6214_csv_header(void)
{
    SEGGER_RTT_printf(AS6214_RTT_CSV_CHANNEL, "uptime_s,temperature_C\r\n");
}

void as6214_sample_to_csv(uint32_t uptime_s)
{
    int16_t raw;
    if (!as6214_read_raw(&raw))
    {
        /* Sensor glitched this cycle. Skip the row rather than emitting a
         * bogus temperature; the logger keeps running and the next good
         * sample resumes the stream (the host wall-clock shows the gap). */
        NRF_LOG_INFO("AS6214 read failed at t=%u s - row skipped.",
                     (unsigned)uptime_s);
        NRF_LOG_FLUSH();
        return;
    }

    /* Emit the temperature with 4 decimal places (the sensor's resolution is
     * 0.0078125 degC) using integer math, since SEGGER_RTT_printf() does not
     * support %f. Value is expressed in ten-thousandths of a degree:
     *   raw / 128 [degC]  ->  raw * 10000 / 128 [1e-4 degC].
     * raw fits in int16, so raw * 10000 fits comfortably in int32. */
    int32_t ten_thousandths = ((int32_t)raw * 10000) / 128;

    int32_t whole = ten_thousandths / 10000;
    int32_t frac  = ten_thousandths % 10000;
    if (frac < 0)
    {
        frac = -frac;
    }

    /* Preserve the minus sign for values between 0 and -1 degC (whole == 0). */
    const char *sign = (ten_thousandths < 0 && whole == 0) ? "-" : "";

    SEGGER_RTT_printf(AS6214_RTT_CSV_CHANNEL, "%u,%s%d.%04u\r\n",
                      (unsigned)uptime_s, sign, (int)whole, (unsigned)frac);
}
