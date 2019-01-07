#include "app_enble.h"

#include <string.h>

#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_srv_common.h"

#include "led_button.h"
#include "sensor.h"

#include "app_timer.h"
#include "fds.h"

#define NRF_LOG_MODULE_NAME "APP_ENBLE"
#define NRF_LOG_LEVEL 4
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define APP_ADV_SLOW_TIMEOUT_IN_SECONDS 0  /**< The advertising timeout in units of seconds. 0 means continuously advertising without timeout. */
#define APP_ADV_SLOW_INTERVAL 3200         /**< The advertising interval (in units of 0.625 ms. This value corresponds to 2.0 s). */
#define APP_ADV_FAST_TIMEOUT_IN_SECONDS 10 /**< The advertising timeout in units of seconds. */
#define APP_ADV_FAST_INTERVAL 160          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 0.1 s). */

#define DEFAULT_DEVICE_ID 0xffff
#define DEFAULT_MEASUREMNT_PERIOD 10

static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}}; /**< Universally unique service identifiers. */

APP_TIMER_DEF(m_meaurement_timer_id);

static uint16_t m_measurement_period;
static uint16_t m_device_id;
static SensorMeasurementData m_measurement_data;

static ble_enble_t *p_enble_instance;

static bool m_is_first_measure;
static bool m_is_measuring;

// FDS file id and record key for backup data
#define FDS_BACKUP_FILE_ID 0x1000
#define FDS_BACKUP_RECORD_KEY 0x2000

static fds_record_desc_t m_enble_fds_record_desc;
static uint32_t m_fds_backup_data;

static uint32_t save_nonvolatile_data()
{
    uint32_t err_code;

    m_fds_backup_data = ((uint32_t)m_measurement_period << 16) | (uint32_t)m_device_id;

    fds_record_chunk_t fds_record_chunk;
    memset(&fds_record_chunk, 0, sizeof(fds_record_chunk));
    fds_record_chunk.p_data = &m_fds_backup_data;
    fds_record_chunk.length_words = 1;

    fds_record_t fds_record;
    memset(&fds_record, 0, sizeof(fds_record));
    fds_record.file_id = FDS_BACKUP_FILE_ID;
    fds_record.key = FDS_BACKUP_RECORD_KEY;
    fds_record.data.p_chunks = &fds_record_chunk;
    fds_record.data.num_chunks = 1;

    fds_find_token_t fds_find_token;
    memset(&fds_find_token, 0, sizeof(fds_find_token));

    uint32_t find_result = fds_record_find(FDS_BACKUP_FILE_ID, FDS_BACKUP_RECORD_KEY, &m_enble_fds_record_desc, &fds_find_token);

    // If data have already existed, write record, otherwise update
    if (find_result == FDS_SUCCESS)
    {
        err_code = fds_record_update(&m_enble_fds_record_desc, &fds_record);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        err_code = fds_record_write(&m_enble_fds_record_desc, &fds_record);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    return NRF_SUCCESS;
}

static uint32_t load_nonvolatile_data()
{
    uint32_t err_code;

    fds_flash_record_t fds_flash_record;
    memset(&fds_flash_record, 0, sizeof(fds_flash_record));

    fds_find_token_t fds_find_token;
    memset(&fds_find_token, 0, sizeof(fds_find_token));

    uint32_t find_result = fds_record_find(FDS_BACKUP_FILE_ID, FDS_BACKUP_RECORD_KEY, &m_enble_fds_record_desc, &fds_find_token);

    if (find_result == FDS_SUCCESS)
    {
        err_code = fds_record_open(&m_enble_fds_record_desc, &fds_flash_record);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }

        memcpy(&m_fds_backup_data, fds_flash_record.p_data, sizeof(m_fds_backup_data));

        m_device_id = (uint16_t)m_fds_backup_data;
        m_measurement_period = (uint16_t)(m_fds_backup_data >> 16);

        NRF_LOG_INFO("nonvolatile data is available\n");
        NRF_LOG_INFO("loaded device_id %u, measurment period %u\n", m_device_id, m_measurement_period);

        err_code = fds_record_close(&m_enble_fds_record_desc);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        m_measurement_period = DEFAULT_MEASUREMNT_PERIOD;
        m_device_id = DEFAULT_DEVICE_ID;

        NRF_LOG_INFO("nonvolatile data is not available\n");
        NRF_LOG_INFO("default value is configured\n");
        NRF_LOG_INFO("device_id %u, measurment period %u\n", m_device_id, m_measurement_period);

        err_code = save_nonvolatile_data();
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    return NRF_SUCCESS;
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;
    switch (ble_adv_evt)
    {
    case BLE_ADV_EVT_FAST:
        NRF_LOG_INFO("start fast advertising\n");
        break;

    case BLE_ADV_EVT_SLOW:
        NRF_LOG_INFO("start slow advertising\n");
        break;

    case BLE_ADV_EVT_IDLE:
        NRF_LOG_INFO("advertising mode is idle\n");
        err_code = ble_advertising_start(BLE_ADV_MODE_SLOW);
        APP_ERROR_CHECK(err_code);
        break;

    default:
        break;
    }
}

static uint32_t advertising_update_data()
{
    uint32_t err_code;
    ble_advdata_t advdata;
    ble_adv_modes_config_t options;

    uint8_t serialized_measurement_data[8];
    serialized_measurement_data[0] = (uint8_t)m_measurement_data.battery;
    serialized_measurement_data[1] = (uint8_t)(m_measurement_data.battery >> 8);
    serialized_measurement_data[2] = (uint8_t)m_measurement_data.temperature;
    serialized_measurement_data[3] = (uint8_t)(m_measurement_data.temperature >> 8);
    serialized_measurement_data[4] = (uint8_t)m_measurement_data.humidity;
    serialized_measurement_data[5] = (uint8_t)(m_measurement_data.humidity >> 8);
    serialized_measurement_data[6] = (uint8_t)m_measurement_data.pressure;
    serialized_measurement_data[7] = (uint8_t)(m_measurement_data.pressure >> 8);

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
    options.ble_adv_slow_interval = APP_ADV_SLOW_INTERVAL;
    options.ble_adv_slow_timeout = APP_ADV_SLOW_TIMEOUT_IN_SECONDS;
    options.ble_adv_fast_enabled = true;
    options.ble_adv_fast_interval = APP_ADV_FAST_INTERVAL;
    options.ble_adv_fast_timeout = APP_ADV_FAST_TIMEOUT_IN_SECONDS;

    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    return err_code;
}

static void button_event_handler()
{
    uint32_t err_code;

    led_blink(100);

    err_code = app_timer_stop(m_meaurement_timer_id);
    APP_ERROR_CHECK(err_code);

    if (!m_is_measuring && p_enble_instance->conn_handle == BLE_CONN_HANDLE_INVALID && !m_is_first_measure)
    {
        err_code = sd_ble_gap_adv_stop();
        APP_ERROR_CHECK(err_code);

        err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
        APP_ERROR_CHECK(err_code);
    }

    err_code = app_timer_start(m_meaurement_timer_id, APP_TIMER_TICKS(m_measurement_period * 1000, 0), NULL);
    APP_ERROR_CHECK(err_code);
}

static void sensor_data_handler(const SensorMeasurementData *measurement_data)
{
    uint32_t err_code;

    led_blink(10);

    memcpy(&m_measurement_data, measurement_data, sizeof(m_measurement_data));
    NRF_LOG_INFO("measurement data is updated\n");
    NRF_LOG_DEBUG("%d %u %u %u\n", measurement_data->temperature, measurement_data->pressure, measurement_data->humidity, measurement_data->battery);

    err_code = advertising_update_data();
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

    err_code = ble_enble_update_battery(p_enble_instance, measurement_data->battery);
    APP_ERROR_CHECK(err_code);

    m_is_measuring = false;
}

static void measurement_timer_handler()
{
    m_is_measuring = true;

    uint32_t err_code = sensor_start_measuring();
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

    err_code = sensor_init(sensor_data_handler);
    return err_code;
}

void app_enble_on_device_id_update_evt(uint16_t new_value)
{
    uint32_t err_code;

    NRF_LOG_INFO("device id is updated %d\n", new_value);

    m_device_id = new_value;

    err_code = advertising_update_data();
    APP_ERROR_CHECK(err_code);

    save_nonvolatile_data();
}

void app_enble_on_period_update_evt(uint16_t new_value)
{
    uint32_t err_code;

    NRF_LOG_INFO("period is updated %d\n", new_value);

    m_measurement_period = new_value;

    err_code = app_timer_stop(m_meaurement_timer_id);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_meaurement_timer_id, APP_TIMER_TICKS(m_measurement_period * 1000, 0), NULL);
    APP_ERROR_CHECK(err_code);

    save_nonvolatile_data();
}

uint32_t app_enble_init(ble_enble_t *m_enble)
{
    uint32_t err_code;

    p_enble_instance = m_enble;

    m_is_first_measure = true;
    m_is_measuring = false;

    load_nonvolatile_data();

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