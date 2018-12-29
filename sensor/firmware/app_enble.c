#include "app_enble.h"

#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_srv_common.h"

#include "led_button.h"
#include "bme280.h"

#include "app_timer.h"

#define NRF_LOG_MODULE_NAME "APP_ENBLE"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define APP_ADV_INTERVAL 3200          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 2.0 s). */
#define APP_ADV_TIMEOUT_IN_SECONDS 600 /**< The advertising timeout in units of seconds. */

// YOUR_JOB: Use UUIDs for service(s) used in your application.
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}}; /**< Universally unique service identifiers. */

APP_TIMER_DEF(m_meaurement_timer_id);

static uint16_t m_measurement_period;
static uint16_t m_device_id;
static ble_enble_t *p_enble_instance;

static bool m_is_first_measure;

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;
    switch (ble_adv_evt)
    {
    case BLE_ADV_EVT_FAST:
        NRF_LOG_INFO("Fast advertising\r\n");
        break;

    case BLE_ADV_EVT_SLOW:
        NRF_LOG_INFO("Slow advertising\r\n");
        break;

    case BLE_ADV_EVT_IDLE:
        err_code = ble_advertising_start(BLE_ADV_MODE_SLOW);
        APP_ERROR_CHECK(err_code);
        break;

    default:
        break;
    }
}

static uint32_t advertising_update_data(const BME280MeasurementData *measurement_data)
{
    uint32_t err_code;
    ble_advdata_t advdata;
    ble_adv_modes_config_t options;

    uint8_t serialized_measurement_data[8];
    serialized_measurement_data[0] = (uint8_t)measurement_data->temperature;
    serialized_measurement_data[1] = (uint8_t)(measurement_data->temperature >> 8);
    serialized_measurement_data[2] = (uint8_t)measurement_data->humidity;
    serialized_measurement_data[3] = (uint8_t)(measurement_data->humidity >> 8);
    serialized_measurement_data[4] = (uint8_t)measurement_data->pressure;
    serialized_measurement_data[5] = (uint8_t)(measurement_data->pressure >> 8);
    serialized_measurement_data[6] = 0;
    serialized_measurement_data[7] = 0;

    ble_advdata_manuf_data_t adv_manufacture_data;
    adv_manufacture_data.company_identifier = m_device_id;
    adv_manufacture_data.data.size = 8;
    adv_manufacture_data.data.p_data = serialized_measurement_data;

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata.uuids_complete.p_uuids = m_adv_uuids;
    advdata.p_manuf_specific_data = &adv_manufacture_data;

    memset(&options, 0, sizeof(options));
    options.ble_adv_slow_enabled = true;
    options.ble_adv_slow_interval = APP_ADV_INTERVAL;
    options.ble_adv_slow_timeout = APP_ADV_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    return err_code;
}

static void button_event_handler()
{
    led_blink(100);
}

static void bme280_data_handler(const BME280MeasurementData *measurement_data)
{
    uint32_t err_code;

    led_blink(5);

    NRF_LOG_DEBUG("%u %u %u\n", measurement_data->temperature, measurement_data->pressure, measurement_data->humidity);

    err_code = advertising_update_data(measurement_data);
    APP_ERROR_CHECK(err_code);

    if (m_is_first_measure)
    {
        err_code = ble_advertising_start(BLE_ADV_MODE_SLOW);
        APP_ERROR_CHECK(err_code);

        m_is_first_measure = false;
    }

    err_code = ble_enble_update_temperature(p_enble_instance, measurement_data->temperature);
    APP_ERROR_CHECK(err_code);

    err_code = ble_enble_update_humidity(p_enble_instance, measurement_data->humidity);
    APP_ERROR_CHECK(err_code);

    err_code = ble_enble_update_pressure(p_enble_instance, measurement_data->pressure);
    APP_ERROR_CHECK(err_code);

    err_code = ble_enble_update_battery(p_enble_instance, 0);
    APP_ERROR_CHECK(err_code);
}

static void measurement_timer_handler()
{
    uint32_t err_code = bme280_start_measuring();
    APP_ERROR_CHECK(err_code);
}

static uint32_t peripheral_init()
{
    uint32_t err_code;

    err_code = led_init();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = button_init(button_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = bme280_init(bme280_data_handler);
    return err_code;
}

void app_enble_on_device_id_update_evt(uint16_t new_value)
{
    uint32_t err_code;

    m_device_id = new_value;

    err_code = ble_enble_update_device_id(p_enble_instance, new_value);
    APP_ERROR_CHECK(err_code);
}

void app_enble_on_period_update_evt(uint16_t new_value)
{
    uint32_t err_code;

    m_measurement_period = new_value;

    err_code = ble_enble_update_period(p_enble_instance, new_value);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_stop(m_meaurement_timer_id);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_meaurement_timer_id, APP_TIMER_TICKS(m_measurement_period * 1000, 0), NULL);
    APP_ERROR_CHECK(err_code);
}

uint32_t app_enble_init(ble_enble_t *m_enble)
{
    uint32_t err_code;

    p_enble_instance = m_enble;

    m_is_first_measure = true;

    // TODO: Load bellow data from non volatile memory
    m_device_id = 1;
    m_measurement_period = 5;

    err_code = ble_enble_update_device_id(p_enble_instance, m_device_id);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = ble_enble_update_period(p_enble_instance, m_measurement_period);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = peripheral_init();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = app_timer_create(&m_meaurement_timer_id, APP_TIMER_MODE_REPEATED, measurement_timer_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = app_timer_start(m_meaurement_timer_id, APP_TIMER_TICKS(m_measurement_period * 1000, 0), NULL);

    led_blink(500);

    return err_code;
}