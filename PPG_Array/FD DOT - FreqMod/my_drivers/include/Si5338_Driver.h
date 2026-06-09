#include <stdint.h>

uint8_t Si5338_read (uint8_t address);
void Si5338_write (uint8_t address, uint8_t data);
void Si5338_reg_init (void);
void Si5338_init (void);