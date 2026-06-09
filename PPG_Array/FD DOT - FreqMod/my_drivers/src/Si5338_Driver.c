#include "i2c_driver.h"
#include "Si5338_Reg.h"
#include "Si5338_Driver.h"
#include "nrf_delay.h"

uint8_t Si5338_read (uint8_t address) {
  uint8_t read_out[1];
  twi_read (Si5338_ADDRESS, address, read_out, 1);
  return read_out[0];
}

void Si5338_write (uint8_t address, uint8_t data) {
  uint8_t packet[2] = {address, data};
  twi_write (Si5338_ADDRESS, packet, 2, false);
}

void Si5338_reg_init (void) {
  
  uint8_t address, value, mask, old, data;
  
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
      old = Si5338_read (address);
      old &= ~mask;
      value &= mask;
      data = old|value;
      Si5338_write(address, data);
    }
  }
}

void Si5338_init (void) {
