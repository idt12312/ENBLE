#ifndef _SENSOR_H
#define _SENSOR_H

#include <stdint.h>

typedef struct
{
    uint16_t temperature; // ℃ x10
    uint16_t humidity;    // % x10
    uint16_t pressure;    // hPa
    uint16_t battery;
} SensorMeasurementData;

typedef void (*sensor_data_handler_t)(const SensorMeasurementData *measurement_data);

uint32_t sensor_init(sensor_data_handler_t sensor_data_handler);
uint32_t sensor_start_measuring();

#endif