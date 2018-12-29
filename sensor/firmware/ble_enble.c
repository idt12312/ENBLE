#include "ble_enble.h"
#include <string.h>
#include "nordic_common.h"
#include "ble_srv_common.h"

#define UUID_DEVICE_ID 0x0011
#define UUID_PERIOD 0x0012
#define UUID_BATTERY 0x0021
#define UUID_TEMPERATURE 0x0022
#define UUID_HUMIDITY 0x0023
#define UUID_PRESSURE 0x0024

#define CHAR_VALUE_LEN_DEVICE_ID 2
#define CHAR_VALUE_LEN_PERIOD 2
#define CHAR_VALUE_LEN_BATTERY 2
#define CHAR_VALUE_LEN_TEMPERATURE 2
#define CHAR_VALUE_LEN_HUMIDITY 2
#define CHAR_VALUE_LEN_PRESSURE 2

// bff2xxxx-378e-4955-89d6-25948b941062
#define ENBLE_BASE_UUID                                                                                    \
    {                                                                                                      \
        {                                                                                                  \
            0x62, 0x10, 0x94, 0x8b, 0x94, 0x25, 0xd6, 0x89, 0x55, 0x49, 0x8e, 0x37, 0x00, 0x00, 0xf2, 0xbf \
        }                                                                                                  \
    }

typedef struct
{
    ble_gatts_char_handles_t *p_handles;
    uint16_t uuid;
    ble_gatt_char_props_t props;
    uint16_t len;
} char_config_t;

/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_enble     ENBLE Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_enble_t *p_enble, ble_evt_t *p_ble_evt)
{
    p_enble->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}

/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_enble     ENBLE Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_enble_t *p_enble, ble_evt_t *p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_enble->conn_handle = BLE_CONN_HANDLE_INVALID;
}

/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_enble     ENBLE Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_enble_t *p_enble, ble_evt_t *p_ble_evt)
{
    ble_gatts_evt_write_t *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (
        p_evt_write->handle == p_enble->device_id_handles.value_handle &&
        p_evt_write->len == CHAR_VALUE_LEN_DEVICE_ID &&
        p_enble->device_id_update_handler != NULL)
    {
        uint16_t *new_value = (uint16_t *)p_evt_write->data;
        p_enble->device_id_update_handler(p_enble, *new_value);
    }
    else if (
        p_evt_write->handle == p_enble->period_handles.value_handle &&
        p_evt_write->len == CHAR_VALUE_LEN_PERIOD &&
        p_enble->period_update_handler != NULL)
    {
        uint16_t *new_value = (uint16_t *)p_evt_write->data;
        p_enble->period_update_handler(p_enble, *new_value);
    }
    else
    {
        // Do Nothing. This event is not relevant for this service.
    }
}

/**@brief Function for adding a characteristic.
 *
 * @param[in] p_enble       ENBLE Service structure.
 * @param[in] p_enble_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t add_char(ble_enble_t *p_enble, const char_config_t *char_config, const char *char_name)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t attr_char_value;
    ble_uuid_t ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props = char_config->props;
    char_md.p_char_user_desc = (uint8_t*)char_name;
    char_md.char_user_desc_max_size = strlen(char_name);
    char_md.char_user_desc_size = strlen(char_name);

    ble_uuid.type = p_enble->uuid_type;
    ble_uuid.uuid = char_config->uuid;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = char_config->len;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = char_config->len;

    return sd_ble_gatts_characteristic_add(p_enble->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           char_config->p_handles);
}

/**@brief Function for updating a specified characteristic.
 *
 * @param[in] p_enble       ENBLE Service structure.
 * @param[in] char_handles  characteristics handles needed to be updated.
 * @param[in] data          data to be set.
 * @param[in] len           length of the above data.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t update_char_value(ble_enble_t *p_enble, ble_gatts_char_handles_t *char_handles, const uint8_t *data, uint16_t len)
{
    ble_gatts_value_t gatts_value;
    gatts_value.len = len;
    gatts_value.offset = 0;
    gatts_value.p_value = (uint8_t *)data;

    return sd_ble_gatts_value_set(p_enble->conn_handle, char_handles->value_handle, &gatts_value);
}

void ble_enble_on_ble_evt(ble_enble_t *p_enble, ble_evt_t *p_ble_evt)
{
    if ((p_enble == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
        on_connect(p_enble, p_ble_evt);
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        on_disconnect(p_enble, p_ble_evt);
        break;

    case BLE_GATTS_EVT_WRITE:
        on_write(p_enble, p_ble_evt);
        break;

    default:
        // No implementation needed.
        break;
    }
}

uint32_t ble_enble_init(ble_enble_t *p_enble, const ble_enble_init_t *p_enble_init)
{
    uint32_t err_code;
    ble_uuid_t ble_uuid;
    ble_uuid128_t enble_base_uuid = ENBLE_BASE_UUID;

    if ((p_enble == NULL) || (p_enble_init == NULL))
    {
        return NRF_ERROR_NULL;
    }

    // Initialize the service structure.
    p_enble->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_enble->device_id_update_handler = p_enble_init->device_id_update_handler;
    p_enble->period_update_handler = p_enble_init->period_update_handler;

    /**@snippet [Adding proprietary Service to S110 SoftDevice] */
    // Add a custom base UUID.
    err_code = sd_ble_uuid_vs_add(&enble_base_uuid, &p_enble->uuid_type);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    ble_uuid.type = p_enble->uuid_type;
    ble_uuid.uuid = BLE_UUID_ENBLE_SERVICE;

    // Add the service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_enble->service_handle);
    /**@snippet [Adding proprietary Service to S110 SoftDevice] */
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    char_config_t char_config;
    ble_gatt_char_props_t char_props;

    memset(&char_props, 0, sizeof(ble_gatt_char_props_t));
    char_props.read = 1;
    char_props.write = 1;
    char_props.write_wo_resp = 1;

    char_config.p_handles = &p_enble->device_id_handles;
    char_config.uuid = UUID_DEVICE_ID;
    char_config.len = CHAR_VALUE_LEN_DEVICE_ID;
    char_config.props = char_props;
    err_code = add_char(p_enble, &char_config, "DevideID");
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    char_config.p_handles = &p_enble->period_handles;
    char_config.uuid = UUID_PERIOD;
    char_config.len = CHAR_VALUE_LEN_PERIOD;
    err_code = add_char(p_enble, &char_config, "Period");
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    memset(&char_props, 0, sizeof(ble_gatt_char_props_t));
    char_props.read = 1;

    char_config.p_handles = &p_enble->battery_handles;
    char_config.uuid = UUID_BATTERY;
    char_config.len = CHAR_VALUE_LEN_BATTERY;
    char_config.props = char_props;
    err_code = add_char(p_enble, &char_config, "Battery");
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    char_config.p_handles = &p_enble->temperature_handles;
    char_config.uuid = UUID_TEMPERATURE;
    char_config.len = CHAR_VALUE_LEN_TEMPERATURE;
    char_config.props = char_props;
    err_code = add_char(p_enble, &char_config, "Temperature");
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    char_config.p_handles = &p_enble->humidity_handles;
    char_config.uuid = UUID_HUMIDITY;
    char_config.len = CHAR_VALUE_LEN_HUMIDITY;
    char_config.props = char_props;
    err_code = add_char(p_enble, &char_config, "Humidity");
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    char_config.p_handles = &p_enble->pressure_handles;
    char_config.uuid = UUID_PRESSURE;
    char_config.len = CHAR_VALUE_LEN_PRESSURE;
    char_config.props = char_props;
    err_code = add_char(p_enble, &char_config, "Pressure");
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}

uint32_t ble_enble_update_device_id(ble_enble_t *p_enble, uint16_t new_value)
{
    return update_char_value(p_enble, &p_enble->device_id_handles, (const uint8_t *)&new_value, 2);
}

uint32_t ble_enble_update_period(ble_enble_t *p_enble, uint16_t new_value)
{
    return update_char_value(p_enble, &p_enble->period_handles, (const uint8_t *)&new_value, 2);
}

uint32_t ble_enble_update_battery(ble_enble_t *p_enble, uint16_t new_value)
{
    return update_char_value(p_enble, &p_enble->battery_handles, (const uint8_t *)&new_value, 2);
}

uint32_t ble_enble_update_temperature(ble_enble_t *p_enble, int16_t new_value)
{
    return update_char_value(p_enble, &p_enble->temperature_handles, (const uint8_t *)&new_value, 2);
}

uint32_t ble_enble_update_humidity(ble_enble_t *p_enble, uint16_t new_value)
{
    return update_char_value(p_enble, &p_enble->humidity_handles, (const uint8_t *)&new_value, 2);
}

uint32_t ble_enble_update_pressure(ble_enble_t *p_enble, uint16_t new_value)
{
    return update_char_value(p_enble, &p_enble->pressure_handles, (const uint8_t *)&new_value, 2);
}
