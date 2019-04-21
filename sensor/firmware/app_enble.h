#ifndef _APP_ENBLE_H
#define _APP_ENBLE_H

#include <stdint.h>
#include "ble_enble.h"

uint32_t app_enble_init(ble_enble_t *m_enble);
void app_enble_on_period_update_evt(uint16_t new_value);
void app_enble_on_device_id_update_evt(uint16_t new_value);

#endif