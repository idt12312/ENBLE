#ifndef BLE_ENBLE_SERVICE_H__
#define BLE_ENBLE_SERVICE_H__

#include "ble.h"
#include "ble_srv_common.h"
#include <stdint.h>
#include <stdbool.h>

#define BLE_UUID_ENBLE_SERVICE 0x0001

/* Forward declaration of the ble_enble_t type. */
typedef struct ble_enble_s ble_enble_t;

/**@brief ENBLE Service event handler type. */
typedef void (*ble_enble_device_id_update_handler_t)(ble_enble_t *p_enble, uint16_t new_value);
typedef void (*ble_enble_period_update_handler_t)(ble_enble_t *p_enble, uint16_t new_value);

/**@brief ENBLE Service initialization structure.
 *
 * @details This structure contains the initialization information for the service. The application
 * must fill this structure and pass it to the service using the @ref ble_enble_init function.
 */
typedef struct
{
    ble_enble_device_id_update_handler_t device_id_update_handler; /**< Event handler to be called for handling received new device id. */
    ble_enble_period_update_handler_t period_update_handler;       /**< Event handler to be called for handling received new period. */
} ble_enble_init_t;

/**@brief ENBLE Service structure.
 *
 * @details This structure contains status information related to the service.
 */
struct ble_enble_s
{
    uint8_t uuid_type;                                             /**< UUID type for ENBLE Service Base UUID. */
    uint16_t service_handle;                                       /**< Handle of ENBLE Service (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t device_id_handles;                    /**< Handles related to the DeviceID characteristic (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t period_handles;                       /**< Handles related to the Period characteristic (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t temperature_handles;                  /**< Handles related to the Temperature characteristic (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t humidity_handles;                     /**< Handles related to the Humidity characteristic (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t pressure_handles;                     /**< Handles related to the Pressure characteristic (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t battery_handles;                      /**< Handles related to the Battrery characteristic (as provided by the S110 SoftDevice). */
    uint16_t conn_handle;                                          /**< Handle of the current connection (as provided by the S110 SoftDevice). BLE_CONN_HANDLE_INVALID if not in a connection. */
    ble_enble_device_id_update_handler_t device_id_update_handler; /**< Event handler to be called for handling received new device id. */
    ble_enble_period_update_handler_t period_update_handler;       /**< Event handler to be called for handling received new period. */
};

/**@brief Function for initializing the ENBLE Service.
 *
 * @param[out] p_enble      ENBLE Service structure. This structure must be supplied by the application. 
 *                          It is initialized by this function and will later be used to identify this particular service instance.
 * @param[in] p_enble_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was successfully initialized. Otherwise, an error code is returned.
 * @retval NRF_ERROR_NULL If either of the pointers p_enble or p_enble_init is NULL.
 */
uint32_t ble_enble_init(ble_enble_t *p_enble, const ble_enble_init_t *p_enble_init);

/**@brief Function for handling the ENBLE Service's BLE events.
 *
 * @details The ENBLE Service expects the application to call this function each time an
 * event is received from the S110 SoftDevice. This function processes the event if it
 * is relevant and calls the ENBLE Service event handler of the
 * application if necessary.
 *
 * @param[in] p_enble       ENBLE Service structure.
 * @param[in] p_ble_evt   Event received from the S110 SoftDevice.
 */
void ble_enble_on_ble_evt(ble_enble_t *p_enble, ble_evt_t *p_ble_evt);

/**@brief Functions for updating characteristics values.
 *
 * @details These functions update each characteristic value and expose the new value to the connected peer.
 *
 * @param[in] p_enble       Pointer to the ENBLE Service structure.
 * @param[in] new_value     new value of the characteristics.
 *
 * @retval NRF_SUCCESS If the string was sent successfully. Otherwise, an error code is returned.
 */
uint32_t ble_enble_update_device_id(ble_enble_t *p_enble, uint16_t new_value);
uint32_t ble_enble_update_period(ble_enble_t *p_enble, uint16_t new_value);
uint32_t ble_enble_update_battery(ble_enble_t *p_enble, uint16_t new_value);
uint32_t ble_enble_update_temperature(ble_enble_t *p_enble, int16_t new_value);
uint32_t ble_enble_update_humidity(ble_enble_t *p_enble, uint16_t new_value);
uint32_t ble_enble_update_pressure(ble_enble_t *p_enble, uint16_t new_value);

#endif // BLE_ENBLE_SERVICE_H__

/** @} */
