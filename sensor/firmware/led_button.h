#ifndef _LED_BUTTON_H
#define _LED_BUTTON_H

#include <stdint.h>
#include <stdbool.h>

// Led
uint32_t led_init();
void led_on();
void led_off();
uint32_t led_blink(uint32_t duration);

// Button
typedef void (*button_evt_handler_t)();

uint32_t button_init(button_evt_handler_t _button_evt_handler);
bool button_is_pushed();
uint32_t button_interrupt_enable();
uint32_t button_interrupt_disable();

#endif