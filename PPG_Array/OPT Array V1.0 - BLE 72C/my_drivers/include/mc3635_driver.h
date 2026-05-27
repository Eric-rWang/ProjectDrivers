#include <stdint.h>
#include <stdbool.h>

#include "ble_driver.h"

#define MC3635_SLAVE_ADDRESS		0x4C

//Register Addresses
#define MC3635_INIT_REG_1			0x0F
#define MC3635_DMX_REG				0x20
#define MC3635_DMY_REG				0x21
#define MC3635_DMZ_REG				0x22
#define MC3635_INIT_REG_2			0x28
#define MC3635_INIT_REG_3			0x1A
#define MC3635_ACC_DATA				0x02
#define MC3635_STAT_REG_1			0x08
#define MC3635_STAT_REG_2			0x09
#define MC3635_FEAT_REG_1			0x0D
#define MC3635_FEAT_REG_2			0x0E
#define MC3635_MODE_REG				0x10
#define MC3635_RATE_REG_1			0x11
#define MC3635_RR_CTR_REG			0x15
#define MC3635_FIFO_CTRL			0x16
#define MC3635_INT_CTRL				0x17
#define MC3635_PWR_MODE				0x1C
#define MC3635_RESET_REG			0x24

#define MC3635_FIFO_SAMPLES			MC3635_PACKET_SIZE_BYTES / 6

typedef struct {
    int16_t X;
    int16_t Y;
    int16_t Z;
} mc3635_data_t;

void mc3635_init(void);
void mc3635_twi_write(uint8_t address, uint8_t data);
void mc3635_read_data(void);
bool mc3635_init_successful(void);