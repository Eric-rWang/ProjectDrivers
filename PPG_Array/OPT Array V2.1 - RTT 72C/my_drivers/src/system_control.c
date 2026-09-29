#include "system_control.h"

system_state_t m_system_state = SYSTEM_STATE_ALL_ON;

void system_shutoff_total(void) {
	//Put MAX86141 chip in shutdown.
	//Set bit 1. Bit 2 is the LP_MODE, so keep it set as well.
	

	//Put the nRF in shutdown
	//sd_power_system_off();
}

void system_shutoff_peripherals(void) {
	set_system_state(SYSTEM_STATE_PERIPHERALS_OFF);
}

void system_turnon_peripherals(void) {
	set_system_state(SYSTEM_STATE_ALL_ON);
}

void set_system_state(system_state_t p_state) {
	m_system_state = p_state;
}

system_state_t get_system_state() {
	return m_system_state;
}