#ifndef BOARD_FUNCTIONS_H
#define BOARD_FUNCTIONS_H

void usb_lld_connect_bus(void*);
void usb_lld_disconnect_bus(void*);
void boardInit(void);

void led_toggle(unsigned int led);
void led_set(unsigned int led);
void led_clear(unsigned int led);
void can_set_silent_mode(int silent);
int user_button_pressed(void);
void can_bus_power_enable(int enabled);

#endif
