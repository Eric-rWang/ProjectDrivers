typedef enum {
	SYSTEM_STATE_ALL_ON,
	SYSTEM_STATE_PERIPHERALS_OFF,
	SYSTEM_STATE_ALL_OFF
} system_state_t;

void system_shutoff_total(void);
void system_shutoff_peripherals(void);
void system_turnon_peripherals(void);
void set_system_state(system_state_t p_state);
system_state_t get_system_state(void);