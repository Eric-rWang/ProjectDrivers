#include <stdint.h>

#define Si5338_ADDRESS 0x70

uint8_t Si5338_read (uint8_t address);
void Si5338_write (uint8_t address, uint8_t data);
uint8_t Si5338_mask_read (uint8_t address, uint8_t mask);
void Si5338_mask_write (uint8_t address, uint8_t data, uint8_t mask);
void Si5338_reg_init (int mode);
void Si5338_init (int mode);