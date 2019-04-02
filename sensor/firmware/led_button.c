#include "led_button.h"
#include "app_timer.h"

#include "nrf_gpio.h"
#include "app_button.h"

#include <string.h>

#define LED_PIN 11
#define BUTTON_PIN 12

#define BUTTON_DETECT_DELAY 100

APP_TIMER_DEF(m_led_blink_timer_id);
static button_evt_handler_t m_button_evt_handler = NULL;

static bool m_is_led_blinking;

static void led_blink_timer_handler()
{
    led_off();
    m_is_led_blinking = false;
}

static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    if (pin_no == BUTTON_PIN && button_action == APP_BUTTON_PUSH)
    {
        if (m_button_evt_handler)
        {
            m_button_evt_handler();
        }
    }
}

uint32_t led_init()
{
    nrf_gpio_cfg_output(LED_PIN);
    
    m_is_led_blinking = false;

    return app_timer_create(&m_led_blink_timer_id, APP_TIMER_MODE_SINGLE_SHOT, led_blink_timer_handler);
}

void led_on()
{
    nrf_gpio_pin_set(LED_PIN);
}

void led_off()
{
    nrf_gpio_pin_clear(LED_PIN);
}

uint32_t led_blink(uint32_t duration)
{
    if (m_is_led_blinking) 
    {
        return NRF_SUCCESS;
    }

    m_is_led_blinking = true;

    led_on();
    return app_timer_start(m_led_blink_timer_id, APP_TIMER_TICKS(duration, 0), NULL);
}

uint32_t button_init(button_evt_handler_t _button_evt_handler)
{
    uint32_t err_code;

    static app_button_cfg_t button_config = {
        .pin_no = BUTTON_PIN,
        .active_state = APP_BUTTON_ACTIVE_LOW,
        .pull_cfg = GPIO_PIN_CNF_PULL_Pullup,
        .button_handler = button_event_handler};

    m_button_evt_handler = _button_evt_handler;

    err_code = app_button_init(&button_config, 1, BUTTON_DETECT_DELAY);

    return err_code;
}

bool button_is_pushed()
{
    return app_button_is_pushed(0);
}

uint32_t button_interrupt_enable()
{
    return app_button_enable();
}

uint32_t button_interrupt_disable()
{
    return app_button_disable();
}