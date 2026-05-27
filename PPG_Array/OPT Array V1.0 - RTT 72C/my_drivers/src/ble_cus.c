//https://github.com/bjornspockeli/custom_ble_service_example

#include "sdk_common.h"
#include "ble_cus.h"
#include <string.h>
#include "ble_srv_common.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "nrf_log.h"

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_cus       Custom Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_cus_t * p_cus, ble_evt_t const * p_ble_evt)
{
    //p_cus->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

    ble_cus_evt_t evt;
	evt.p_cus		= p_cus;
    evt.evt_type	= BLE_CUS_EVT_CONNECTED;

    p_cus->evt_handler(&evt);
}

/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_cus       Custom Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_cus_t * p_cus, ble_evt_t const * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    //p_cus->conn_handle = BLE_CONN_HANDLE_INVALID;
    
    ble_cus_evt_t evt;
	evt.p_cus		= p_cus;
    evt.evt_type	= BLE_CUS_EVT_DISCONNECTED;

    p_cus->evt_handler(&evt);
}

/**@brief Function for handling the Write event.
 *
 * @param[in]   p_cus       Custom Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_cus_t * p_cus, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write	= &p_ble_evt->evt.gatts_evt.params.write;
    ble_cus_evt_t evt;
	memset(&evt, 0, sizeof(ble_cus_evt_t));
	evt.p_cus									= p_cus;
		
	if ((p_evt_write->handle == p_cus->tx_handles.cccd_handle)
        && (p_evt_write->len == 2))
    {
        // CCCD written, call application event handler
       
		if (ble_srv_is_notification_enabled(p_evt_write->data))
		{
			evt.evt_type = BLE_CUS_EVT_NOTIFICATION_ENABLED;
		}
		else
		{
			evt.evt_type = BLE_CUS_EVT_NOTIFICATION_DISABLED;
		}

		if (p_cus->evt_handler != NULL)
        {
            // Call the application event handler.
            p_cus->evt_handler(&evt);
        }
    } else if (p_evt_write->handle == p_cus->rx_handles.value_handle &&
				(p_cus->evt_handler != NULL)) {
		//Forward event data to event handler
		
		evt.evt_type = BLE_CUS_EVT_RX_DATA;
		evt.params.rx_data.p_data = p_evt_write->data;
		evt.params.rx_data.length = p_evt_write->len;
		
		p_cus->evt_handler(&evt);
	} else {
		//Do nothing
	}

}

void ble_cus_on_ble_evt( ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_cus_t * p_cus = (ble_cus_t *) p_context;
    
    //NRF_LOG_INFO("BLE event received. Event type = %d\r\n", p_ble_evt->header.evt_id); 
    if (p_cus == NULL || p_ble_evt == NULL)
    {
        return;
    }
    
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_cus, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_cus, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_cus, p_ble_evt);
            break;
/* Handling this event is not necessary
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            NRF_LOG_INFO("EXCHANGE_MTU_REQUEST event received.\r\n");
            break;
*/
        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_cus_init(ble_cus_t * p_cus, const ble_cus_init_t * p_cus_init)
{
    if (p_cus == NULL || p_cus_init == NULL)
    {
        return NRF_ERROR_NULL;
    }

    uint32_t   err_code;
    ble_uuid_t ble_uuid;
	ble_uuid128_t base_uuid = CUSTOM_SERVICE_UUID_BASE;
	ble_add_char_params_t add_char_params;

    // Initialize service structure
    p_cus->evt_handler               = p_cus_init->evt_handler;
    //p_cus->conn_handle               = BLE_CONN_HANDLE_INVALID;

    // Add Custom Service UUID
    
    err_code =  sd_ble_uuid_vs_add(&base_uuid, &p_cus->uuid_type);
    VERIFY_SUCCESS(err_code);
    
    ble_uuid.type = p_cus->uuid_type;
    ble_uuid.uuid = CUSTOM_SERVICE_UUID;

    // Add the Custom Service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_cus->service_handle);
    VERIFY_SUCCESS(err_code);

	// Add the RX Characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                     = CUSTOM_RX_CHAR_UUID;
    add_char_params.uuid_type                = p_cus->uuid_type;
    add_char_params.max_len                  = sizeof(uint8_t);
    add_char_params.init_len                 = sizeof(uint8_t);
    add_char_params.is_var_len               = false;
    add_char_params.char_props.write         = 1;
    add_char_params.char_props.write_wo_resp = 1;

    add_char_params.read_access  = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;

    err_code = characteristic_add(p_cus->service_handle, &add_char_params, &p_cus->rx_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add the TX Characteristic.
    /**@snippet [Adding proprietary characteristic to the SoftDevice] */
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = CUSTOM_TX_CHAR_UUID;
    add_char_params.uuid_type         = p_cus->uuid_type;
    add_char_params.max_len           = sizeof(uint8_t);
    add_char_params.init_len          = sizeof(uint8_t);
    add_char_params.is_var_len        = false;
    add_char_params.char_props.notify = 1;

    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access      = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    return characteristic_add(p_cus->service_handle, &add_char_params, &p_cus->tx_handles);
}

uint32_t ble_cus_data_send(ble_cus_t * p_cus,
                           uint8_t   * p_data,
                           uint16_t  * p_length,
                           uint16_t    conn_handle) {
    uint32_t err_code = NRF_SUCCESS;
	ble_gatts_hvx_params_t hvx_params;
	//ble_gatts_value_t gatts_value;

	if (p_cus == NULL) {
        return NRF_ERROR_NULL;
    }

    
    

    // Initialize value struct.
    //memset(&gatts_value, 0, sizeof(gatts_value));

    //gatts_value.len     = sizeof(uint8_t);
    //gatts_value.offset  = 0;
    //gatts_value.p_value = &custom_value;

    // Update database.
	//err_code = sd_ble_gatts_value_set(conn_handle,
 //                                     p_cus->tx_handles.value_handle,
 //                                     &gatts_value);
									  
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }

    // Send value if connected and notifying.
    if ((conn_handle != BLE_CONN_HANDLE_INVALID)) {
        memset(&hvx_params, 0, sizeof(hvx_params));

        hvx_params.handle = p_cus->tx_handles.value_handle;
		hvx_params.p_data = p_data;
		hvx_params.p_len  = p_length;
		hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        
        err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);

        NRF_LOG_INFO("sd_ble_gatts_hvx result: %x. \r\n", err_code); 
    }
   
    return err_code;
}