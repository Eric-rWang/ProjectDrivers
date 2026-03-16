#ifndef BLE_CUS_H__
#define BLE_CUS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"

/* RX Messages from TMS */
typedef enum {
    TMS_MSG_RX_SHUTDOWN = 1,
	TMS_MSG_RX_TURNON = 2,
	TMS_MSG_RX_CALIBRATE = 4
} tms_msg_rx_t;

/* TX Messages from TMS */
typedef enum {
	TMS_MSG_TX_NORMAL_STATE = 1,
	TMS_MSG_TX_SHUTDOWN = 2,
	TMS_MSG_TX_OVERHEATED = 3,
	TMS_MSG_TX_CALIBRATION_DONE = 4,
	TMS_MSG_TX_AS7343_DATA = 5
} tms_msg_tx_t;


/**@brief   Macro for defining a ble_hrs instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CUS_DEF(_name)                                                                          \
static ble_cus_t _name;                                                                             \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                                 \
                     BLE_HRS_BLE_OBSERVER_PRIO,                                                     \
                     ble_cus_on_ble_evt, &_name)



// CUSTOM_SERVICE_UUID_BASE f364adc9-b000-4042-ba50-05ca45bf8ab1

#define CUSTOM_SERVICE_UUID_BASE         {0xB1, 0x8A, 0xBF, 0x45, 0xCA, 0x05, 0x50, 0xBA, \
                                          0x40, 0x42, 0xB0, 0x00, 0xC9, 0xAD, 0x64, 0xF3}

#define CUSTOM_SERVICE_UUID             0x1400
#define CUSTOM_TX_CHAR_UUID				0x1401
#define CUSTOM_RX_CHAR_UUID				0x1402
																					
/**@brief Custom Service event type. */
typedef enum
{
    BLE_CUS_EVT_NOTIFICATION_ENABLED,                             /**< Custom value notification enabled event. */
    BLE_CUS_EVT_NOTIFICATION_DISABLED,                             /**< Custom value notification disabled event. */
    BLE_CUS_EVT_DISCONNECTED,
    BLE_CUS_EVT_CONNECTED,
	BLE_CUS_EVT_RX_DATA
} ble_cus_evt_type_t;

// Forward declaration of the ble_cus_t type.
typedef struct ble_cus_s ble_cus_t;

/**@brief   Tattoo Messeging Service @ref BLE_CUS_EVT_RX_DATA event data.
 *
 * @details This structure is passed to an event when @ref BLE_CUS_EVT_RX_DATA occurs.
 */
typedef struct
{
    uint8_t const * p_data; /**< A pointer to the buffer with received data. */
    uint16_t        length; /**< Length of received data. */
} ble_cus_evt_rx_data_t;

/**@brief   Tattoo Messeging Service event structure.
 *
 * @details This structure is passed to an event coming from service.
 */
typedef struct
{
    ble_cus_evt_type_t         evt_type;        /**< Event type. */
    ble_cus_t                * p_cus;       /**< A pointer to the instance. */
    union
    {
        ble_cus_evt_rx_data_t rx_data; /**< @ref BLE_NUS_EVT_RX_DATA event data. */
    } params;
} ble_cus_evt_t;



/**@brief Custom Service event handler type. */
typedef void (*ble_cus_evt_handler_t) (ble_cus_evt_t * p_evt);

/**@brief Service init structure. This contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
    ble_cus_evt_handler_t         evt_handler;                    /**< Event handler to be called for handling events in the Custom Service. */
    uint8_t                       initial_custom_value;           /**< Initial custom value */
    ble_srv_cccd_security_mode_t  custom_value_char_attr_md;     /**< Initial security level for Custom characteristics attribute */
} ble_cus_init_t;

/**@brief Custom Service structure. This contains various status information for the service. */
struct ble_cus_s
{
    uint8_t                       uuid_type; 
	uint16_t                      service_handle;                 /**< Handle of Custom Service (as provided by the BLE stack). */
	ble_gatts_char_handles_t      tx_handles;				      /**< Handles related to the TX characteristic. */
	ble_gatts_char_handles_t	  rx_handles;					  /**< Handles related to the RX characteristic (as provided by the SoftDevice). */
	ble_cus_evt_handler_t         evt_handler;                    /**< Event handler to be called for handling events in the Custom Service. */
    //uint16_t                      conn_handle;                    /**< Handle of the current connection (as provided by the BLE stack, is BLE_CONN_HANDLE_INVALID if not in a connection). */
};

/**@brief Function for initializing the Custom Service.
 *
 * @param[out]  p_cus       Custom Service structure. This structure will have to be supplied by
 *                          the application. It will be initialized by this function, and will later
 *                          be used to identify this particular service instance.
 * @param[in]   p_cus_init  Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
uint32_t ble_cus_init(ble_cus_t * p_cus, const ble_cus_init_t * p_cus_init);

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @details Handles all events from the BLE stack of interest to the Battery Service.
 *
 * @note 
 *
 * @param[in]   p_cus      Custom Service structure.
 * @param[in]   p_ble_evt  Event received from the BLE stack.
 */
void ble_cus_on_ble_evt( ble_evt_t const * p_ble_evt, void * p_context);

/**@brief Function for updating the custom value.
 *
 * @details The application calls this function when the cutom value should be updated. If
 *          notification has been enabled, the custom value characteristic is sent to the client.
 *
 * @note 
 *       
 * @param[in]   p_bas          Custom Service structure.
 * @param[in]   Custom value 
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */

uint32_t ble_cus_data_send(ble_cus_t * p_cus,
                           uint8_t   * p_data,
                           uint16_t  * p_length,
                           uint16_t    conn_handle);

/**@brief Function for handling the Custom Service Service events.
 *
 * @details This function will be called for all Custom Service events which are passed to
 *          the application.
 *
 * @param[in]   p_evt          Event received from the Custom Service.
 *
 */
static void on_cus_evt(ble_cus_evt_t * p_evt);

#endif // BLE_CUS_H__