#include "as6214_driver.h"
#include "SEGGER_RTT.h"

static bool as6214_init_success = false;

/* RTT terminal used for the live CSV stream. Capture this channel with the
 * J-Link RTT Logger / RTT Viewer "log to file" feature to obtain a .csv. */
#define AS6214_RTT_CSV_CHANNEL  0

void as6214_init(void)
{
    /* The AS6214 has no device-ID register, so verify communication by reading
     * the configuration register. A device that ACKs and returns its fixed
     * reserved bit (bit 14 = 1) is present and sane. A floating bus reads back
     * 0xFFFF. Config bytes are transmitted MSB first. */
    uint8_t cfg[2];
    twi_read(AS6214_SLAVE_ADDRESS, AS6214_REG_CONFIG, cfg, 2);

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

int16_t as6214_read_raw(void)
{
    uint8_t samples[2];
    twi_read(AS6214_SLAVE_ADDRESS, AS6214_REG_TVAL, samples, 2);

    /* MSB byte is read first, then the LSB byte (datasheet section 6.2.10).
     * The result is a signed 16-bit two's-complement value. */
    return (int16_t)(((uint16_t)samples[0] << 8) | samples[1]);
}

float as6214_raw_to_celsius(int16_t raw)
{
    /* 1 LSB = 0.0078125 degC = 1/128 degC. */
    return (float)raw * AS6214_LSB_CELSIUS;
}

float as6214_read_celsius(void)
{
    return as6214_raw_to_celsius(as6214_read_raw());
}

void as6214_csv_header(void)
{
    SEGGER_RTT_printf(AS6214_RTT_CSV_CHANNEL, "time_s,temperature_C\r\n");
}

void as6214_sample_to_csv(uint32_t timestamp_s)
{
    int16_t raw = as6214_read_raw();

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
                      (unsigned)timestamp_s, sign, (int)whole, (unsigned)frac);
}
