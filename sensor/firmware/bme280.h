#ifndef _BME280_H
#define _BME280_H

#include <stdint.h>

typedef struct
{
    uint16_t temperature; // ℃ x10
    uint16_t humidity;    // % x10
    uint16_t pressure;    // hPa
} BME280MeasurementData;

typedef void (*bme280_data_handler_t)(const BME280MeasurementData *measurement_data);

uint32_t bme280_init(bme280_data_handler_t bme280_data_handler);
uint32_t bme280_start_measuring();

#endif