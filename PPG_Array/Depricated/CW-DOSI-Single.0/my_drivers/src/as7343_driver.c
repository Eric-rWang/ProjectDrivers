#include "as7343_driver.h"
#include "i2c_driver.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"
#include "system_control.h"

uint8_t led_power = 1;
uint8_t atime = 1;
uint8_t counter = 0;
bool previous_gpio_state = false;
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
    
    //The AS7343 can remain in sleep during setup.

    //Ensure that initialization busy is not set. THIS NEEDS AN EXIT IN CASE NACK
    while (as7343_read_reg(0xBC) & 0x01) {}

    //***********************************************************************
    //In order to access registers 0x80 and above, REG_BANK needs to be set to 0.
    as7343_write_reg(0xBF, (0 << 4));  // Enable low power, set REG_BANK to 0

    //ENABLE Register (0X80)
    //Setting the PON bit puts device in idle.
    //Setting the SP_EN bit puts the device in active.
    as7343_write_reg(0x80, 1); // Enabling Device

    //CONTROL Register
    //**** SOFTWARE RESET HERE!!!! **** FIFO CLEARED HERE!!!! ****
    //Software reset and clear FIFO Buffer
    as7343_write_reg(0xFA, (1 << 3));
    as7343_write_reg(0xFA, (1 << 1));
    //NRF_LOG_INFO("0x%02x", as7343_read_reg(0xBF));

    //***********************************************************************
    //In order to access registers below 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1

    //Ensure that device IDs are as expected.
    uint8_t id = as7343_read_reg(0x5A);
    if(id != 0b10000001) {
        NRF_LOG_INFO("AS7343 ID is incorrect (0x%02x)", id);
        return;
    }

    //**** SETTING PIN TO TAKE IN GPIO INPUT ****
    //Set the GPIO to be input enable (0X6B)
    as7343_write_reg(0x6B, 0x04); 

    //***********************************************************************
    //In order to access registers 0x80 and above, REG_BANK needs to be set to 0.
    //Also enable low power idle
    as7343_write_reg(0xBF, (0 << 5) | (0 << 4));  // Enable low power, set REG_BANK to 0

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    //LED Register (0xCD)
    //Bit 7 is use external LED
    //Writing 6mA for now
    as7343_write_reg(0xCD, (1 << 7) | 1);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //ATIME Register
    //Set to an integration time of 100
    as7343_write_reg(0x81, atime);

    //ASTEP Register
    //tint = (ATIME + 1) x (ASTEP + 1) x 2.78us
    //as7343_write_two_reg(0xCA, 0x03, 0xE7);  // ASTEP = 999
    as7343_write_reg(0xD4, 0xE7);
    as7343_write_reg(0xD5, 0x03);
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //ADC Configuration
    //CFG1 sets the spectral sensitivity. 0.5x, 1x, 2x, etc.
    as7343_write_reg(0xC6, 12);
    //AZ_CONFIG sets how often the sepctral engine offsets are reset. Set to only before first measurement cycle.
    as7343_write_reg(0xDE, 255);
    
    //CFG20 sets the automatic channel readout
    //as7343_write_reg(0xD6, 0x00);  // Disable auto_SMUX
    as7343_write_reg(0xD6, 3<<5); // Enable auto_SMUX

    //FIFO Configuration
    //Set which channels are written to the FIFO
    //Channels 0 - 4, not including ASTATUS and FD
    as7343_write_reg(0xFC, 0x3E);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //CFG6 Register SMUX_CMD
    //as7343_write_reg(0xF5, 0x20);
    
    ////Enable interupts via INTENAB Register
    uint8_t intenab = as7343_read_reg(0xF9);
    intenab |= 0x01; // Set SIEN bit
    //as7343_write_reg(0xF9, intenab);

    //as7343_write_reg(0xCA, 0x10); // Set SIEN_GPIO bit in CFG9 register

    // Disable wait timer
    //as7343_write_reg(0x83, 0x00);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //ENABLE Register (0X80)
    //Setting SMUXEN to 1.
    //Setting the PON bit puts device in idle.
    //Setting the SP_EN bit puts the device in active.
    as7343_write_reg(0x80, 0x01);

    NRF_LOG_INFO("AS7343 initialized successfully.");

    //**** VERIFYING SOFTWARE CONFIG ****
    uint8_t smux_cmd = as7343_read_reg(0xF5);
    uint8_t device_status = as7343_read_reg(0x80);
    
    //***********************************************************************
    //In order to access registers below 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1
    uint8_t gpio_input = as7343_read_reg(0x6B);
    NRF_LOG_INFO("GPIO Register 0x6B: (0x%02x)", gpio_input);
    
    NRF_LOG_INFO("SMUX_CMD Register 0xF5: (0x%02x)", smux_cmd);
    NRF_LOG_INFO("Enable Register 0x80: (0x%02x)", device_status);
}

void as7343_write_reg(uint8_t register_address, uint8_t data) {
    uint8_t reg_data[2] = {register_address, data};
    twi_write(AS7343_SLAVE_ADDRESS, reg_data, sizeof(reg_data), false);
}

void as7343_write_two_reg(uint8_t register_address, uint8_t lsb, uint8_t msb) {
    uint8_t reg_data[3] = {register_address, lsb, msb};
    twi_write(AS7343_SLAVE_ADDRESS, reg_data, sizeof(reg_data), false);
}

uint8_t as7343_read_reg(uint8_t register_address) {
    uint8_t data[1];
    twi_read(AS7343_SLAVE_ADDRESS, register_address, data, 1);

    return data[0];
}

void as7343_start_measurement(uint8_t num_measurements) {
    set_system_state(SYSTEM_STATE_CALIBRATING);
    NRF_LOG_INFO("Starting measurments...");


    //LED Register
    //[7]   LED_ACT: set for external LED
    //[6:0] LED_DRIVE: LED current. 1111111 for 258 mA
    as7343_write_reg(0xCD, (1 << 7) | led_power);
	
    for(uint8_t i = 0; i < num_measurements; i ++) {
        //PON and enable spectral measurement
        as7343_write_reg(0x80, 3);

        as7343_read_spectral_data();
    }

    //Turn off LED
    as7343_write_reg(0xCD, 0);

    //Disable PON
    as7343_write_reg(0x80, 0);
    set_system_state(SYSTEM_STATE_ALL_ON);
}

void as7343_read_spectral_data() {
    NRF_LOG_INFO("Reading Data...");

    //***********************************************************************
    //In order to access registers above 0x80, REG_BANK needs to be set to 0.
    as7343_write_reg(0xBF, 0 << 4); // set REG_BANK to 0

    // Read ASTATUS to latch the data
    uint8_t astatus = as7343_read_reg(0x94);

    // Read 36 bytes of spectral data
    uint8_t spectral_data[18];

    uint8_t data_low_bit;
    uint8_t data_high_bit;

    for (int i = 0; i < 18; i++) {

        data_low_bit = as7343_read_reg(0x95 + 2 * i);
        data_high_bit = as7343_read_reg(0x95 + (2 * i + 1));

        spectral_data[i] = (data_high_bit << 8) | data_low_bit;

        NRF_LOG_INFO("Channel(%s): %d", channel_descriptions[i], spectral_data[i]);
        
    }

    NRF_LOG_FLUSH();

    //send_data_nus(data, 37);
    //send_data_cus(TMS_MSG_TX_AS7343_DATA, data, 37);
    //NRF_LOG_INFO("Data sent.");
}

void as7343_dump_fifo() {
    //Check level of FIFO (FIFO_LVL in 0xFD)
    uint8_t fifo_lvl = as7343_read_reg(0xFD);

    NRF_LOG_INFO("%i samples in the FIFO: ", fifo_lvl);
    NRF_LOG_FLUSH();

    //***********************************************************************
    //In order to access registers above 0x80, REG_BANK needs to be set to 0.
    as7343_write_reg(0xBF, 0 << 4); // set REG_BANK to 0

    //Check if there is overflow
    uint8_t status4 = as7343_read_reg(0xBC);
    NRF_LOG_INFO("Overflow Status 0xBC: 0x%02x", status4);
    NRF_LOG_FLUSH();

    //***********************************************************************
    //In order to access registers below 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1

    uint8_t gpio_status = as7343_read_reg(0x6B);
    //NRF_LOG_INFO("GPIO Status 0x6B: 0x%02x", gpio_status);
    //NRF_LOG_FLUSH();

    //Counter
    //NRF_LOG_INFO("Sample Counter: 0x%02x", counter);
    //NRF_LOG_FLUSH();
    //counter++;
    
}

bool as7343_check_gpio_trigger(void) {
    //In order to access registers below 0x80, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 1 << 4); // set REG_BANK to 1

    uint8_t gpio_status = as7343_read_reg(0x6B);

    return (gpio_status & 0x01); // Check GPIO_IN bit
}

void as7343_sample_on_gpio_trigger(void) {

    // Starting measurement
    NRF_LOG_INFO("Starting Measurement...");
    NRF_LOG_INFO("Sample Counter: 0x%02x", counter);

    //In order to access registers 0x80 & above, REG_BANK needs to be set to 1.
    as7343_write_reg(0xBF, 0 << 4); // set REG_BANK to 1

    // Trigger a single measurement
    as7343_write_reg(0x80, 0x03);  // Set PON and SP_EN
        
    // Wait for measurement to complete
    while (!(as7343_read_reg(0x90) & 0x40)) {}
        
    // Dump FIFO for debugging... Meant to read FIFO data
    as7343_read_spectral_data();
    //as7343_dump_fifo();
        
    // Clear FIFO
    as7343_write_reg(0xFA, (1 << 1));
    as7343_write_reg(0x80, 0x01);

    counter++;
}
