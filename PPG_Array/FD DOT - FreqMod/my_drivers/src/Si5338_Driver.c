#include "i2c_driver.h"
#include "Si5338_Reg.h"
#include "Si5338_Driver.h"
#include "nrf_delay.h"
#include "nrf_log.h"

uint8_t Si5338_read (uint8_t address) {
  uint8_t read_out[1];
  twi_read (Si5338_ADDRESS, address, read_out, 1);
  return read_out[0];
}

void Si5338_write (uint8_t address, uint8_t data) {
  uint8_t packet[2] = {address, data};
  twi_write (Si5338_ADDRESS, packet, 2, false);
}

uint8_t Si5338_mask_read (uint8_t address, uint8_t mask) {
  uint8_t temp_read;
  temp_read = Si5338_read (address);
  return temp_read & mask;
}

void Si5338_mask_write (uint8_t address, uint8_t data, uint8_t mask) {
  uint8_t old, data_write;
  old = Si5338_read (address);
  old &= ~mask;
  data &= mask;
  data_write = old|data;
  Si5338_write(address, data_write);
}

void Si5338_reg_init (void) {
  
  uint8_t address, value, mask;
  
  for (int i = 0; i < NUM_REGS_MAX; i++) {
    address = Reg_Store[i].Reg_Addr;
    value = Reg_Store[i].Reg_Val;
    mask = Reg_Store[i].Reg_Mask;

    if (mask == 0x00) {
      continue;
    }

    else if (mask == 0xFF) {
      Si5338_write (address, value);
    }

    else {
      Si5338_mask_write (address, value, mask);
    }
  }

    uint8_t test_read = Si5338_read(48); // Or whichever register you pick
    // NRF_LOG_INFO("Expected: 0x3A, Actual: 0x%02X", test_read);
}

void Si5338_init (void) { // Follows initiation flow chart on page 23 of Si5338 datasheet
  uint8_t status, temp;

  // Set address page to 0
  Si5338_write (255, 0x00);

  // Disable Outputs
  Si5338_mask_write (230, 0x10, 0x10);

  // Pause LOL
  Si5338_mask_write (241, 0x80, 0x80);

  // Initialize registers from ClockBuilderPro
  Si5338_reg_init ();

  // Validate input clock
  status = Si5338_mask_read (218, 0x04);
  while (status != 0x00) { // when bit 2 is low, there is no loss of input clock signal on IN1,2
    nrf_delay_ms (1);
    status = Si5338_mask_read (218, 0x04);
  }

  // Configure PLL for locking
  Si5338_mask_write (49, 0x00, 0x80); // Configure PLL for locking (Page 1)

  // Initiate locking of PLL
  Si5338_mask_write (246, 0x02, 0x02);

  // Wait 25 ms
  nrf_delay_ms (25);

  // Restart LOL
  Si5338_write (241, 0x65);

  // Confirm PLL lock status
  status = Si5338_mask_read (218, 0x15);
  while (status != 0x00) {
    nrf_delay_ms (1);
    status = Si5338_mask_read (218, 0x15);
  }

  // Copy FCAL values to active registers
  temp = Si5338_mask_read (237, 0x03);
  Si5338_mask_write (47, temp, 0x03);
  temp = Si5338_read (236);
  Si5338_write (46, temp);
  temp = Si5338_read (235);
  Si5338_write (45, temp);
  Si5338_mask_write (47, 0x14, 0xFC);

  // Set PLL to use FCAL values
  Si5338_mask_write (49, 0x80, 0x80);

  // Enable Outputs
  Si5338_mask_write (230, 0x00, 0x10);
}