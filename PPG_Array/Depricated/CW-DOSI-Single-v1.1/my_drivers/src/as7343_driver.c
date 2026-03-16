#include "as7343_driver.h"
#include "i2c_driver.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"
#include "system_control.h"

const char* channel_descriptions[18] = {
    "F1 (~405 nm)",           // DATA_0
    "F2 (~425 nm)",           // DATA_1
    "FZ (~450 nm)",           // DATA_2
    "F3 (~475 nm)",           // DATA_3
    "F4 (~515 nm)",           // DATA_4
    "FY (~555 nm)",           // DATA_5
    "F5 (~550 nm)",           // DATA_6
    "FXL (~600 nm)",          // DATA_7
    "F6 (~640 nm)",           // DATA_8
    "F7 (~690 nm)",           // DATA_9
    "F8 (~745 nm)",           // DATA_10
    "NIR (~855 nm)",          // DATA_11
    "VIS (380~760 nm)",       // DATA_12
    "Clear (Broadband)",      // DATA_13
    "Auxiliary/Flicker 1",    // DATA_14
    "Auxiliary/Flicker 2",    // DATA_15
    "Auxiliary/Flicker 3",    // DATA_16
    "Auxiliary/Flicker 4"     // DATA_17
};


void as7343_init(void) {
    // ************************************************************************************//

    //In order to access registers < 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1

    //Ensure that device IDs are as expected.
    uint8_t id = as7343_read_reg(0x5A);
    if(id != 0b10000001) {
        NRF_LOG_INFO("AS7343 ID is incorrect (0x%02x)", id);
        return;
    } else {
      NRF_LOG_INFO("AS7343 ID is correct");
    }

    // ************************************************************************************//
    
    //In order to access registers >= 0x80, REG_BANK needs to be set to 0.
    as7343_write_reg(0xBF, 0 << 4); // set REG_BANK to 0

    // Power on the device
    as7343_write_reg(0x80, 0x01);

    nrf_delay_ms(1); // Delay to ensure device is on
    
    // Configure device for spectral measurement
    as7343_write_reg(0x81, 29);   // Set ATIME (integration time)
    as7343_write_reg(0xD4, 0x57); // Set ASTEP_LSB
    as7343_write_reg(0xD5, 0x02); // Set ASTEP_MSB
    as7343_write_reg(0xC6, 0x07); // Set AGAIN (gain)

    // Enable low power mode when waiting for GPIO trigger
    as7343_write_reg(0xBF, 0x20);

    // ************************************************************************************//

    //In order to access registers < 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1
    
    // Configure GPIO as input for synchronization
    as7343_write_reg(0x6B, 0x04); // Enable GPIO as input

    // ************************************************************************************//
    
    //In order to access registers >= 0x80, REG_BANK needs to be set to 0.
    as7343_write_reg(0xBF, 0 << 4); // set REG_BANK to 0

    // Enable device but don't start measurements yet
    as7343_write_reg(0x80, 0x01);

    NRF_LOG_INFO("AS7343 initialized and ready for GPIO trigger");
}

void as7343_write_reg(uint8_t register_address, uint8_t data) {
    uint8_t reg_data[2] = {register_address, data};
    twi_write(AS7343_SLAVE_ADDRESS, reg_data, sizeof(reg_data), false);
}

uint8_t as7343_read_reg(uint8_t register_address) {
    uint8_t data[1];
    twi_read(AS7343_SLAVE_ADDRESS, register_address, data, 1);

    return data[0];
}

bool as7343_check_gpio_status(void) {
    //In order to access registers < 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1
    
    bool gpio_status = as7343_read_reg(0x6B) & 0x01;

    NRF_LOG_INFO("GPIO status (0x%02x)", gpio_status);

    return gpio_status; // Read GPIO state
}

// Function to read spectral data
void read_spectral_data(void) {
    uint16_t channel_data[18];
    
    for (int i = 0; i < 18; i++) {
        uint8_t low_byte = as7343_read_reg(0x95 + i*2);
        uint8_t high_byte = as7343_read_reg(0x96 + i*2);
        channel_data[i] = (high_byte << 8) | low_byte;
    }

}