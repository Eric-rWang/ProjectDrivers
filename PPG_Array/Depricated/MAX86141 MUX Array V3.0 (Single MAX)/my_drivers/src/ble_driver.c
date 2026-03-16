#include <stdint.h>
#include "ble_driver.h"
#include "system_control.h"
#include "max86141_driver.h"

#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "ble_cus.h"

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

BLE_CUS_DEF(m_cus);

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

uint8_t ble_buffer[NUS_PACKET_SIZE];
uint16_t ble_buffer_index = 0;
uint32_t poopie = 0;

ble_nus_rx_handler_t nus_rx_handler;
ble_cus_rx_handler_t cus_rx_handler;

void send_data_nus(uint8_t* data_array, uint16_t length) {
	uint32_t err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
}

void nus_add_to_buffer(uint8_t* data, uint16_t length) {

	for(int i = 0; i < length; i ++, ble_buffer_index ++) {
		if (ble_buffer_index >= NUS_PACKET_SIZE) {
                        NRF_LOG_INFO("BLE_BUFFER_INDEX (%d), Packet Size(%d)", ble_buffer_index, NUS_PACKET_SIZE);
			//NRF_LOG_INFO("NUS buffer has reached max length! Discarding sample.");
		} else {
			ble_buffer[ble_buffer_index] = data[i];
		}
	}

	if(ble_buffer_index == NUS_PACKET_SIZE) {
                //ble_buffer[ble_buffer_index] = poopie;
                //NRF_LOG_INFO("buffer sent, poopie counter (%d), ble_buffer (%d)", poopie, ble_buffer_index);
                  NRF_LOG_INFO("NRF Sent");
                  NRF_LOG_FLUSH();
                  nus_send_buffer();
                  
                  poopie++;
               
	}
}

void nus_send_buffer(void) {
	uint16_t length = NUS_PACKET_SIZE;
        
	uint32_t err_code = ble_nus_data_send(&m_nus, ble_buffer, &length, m_conn_handle);

	//Reset the buffer index.
	ble_buffer_index = 0;
}

void send_data_cus(tms_msg_tx_t message) {
	//uint32_t err_code = ble_cus_data_send(&m_cus, (uint8_t) message);
	uint8_t data = (uint8_t) message;
	uint16_t len = sizeof(data);
	uint32_t err_code = ble_cus_data_send(&m_cus, &data, &len, m_conn_handle);
}

void send_system_state(void) {
	system_state_t state = get_system_state();
	
	switch (state) {
		case SYSTEM_STATE_ALL_ON:
			send_data_cus(TMS_MSG_TX_NORMAL_STATE);
			break;
		case SYSTEM_STATE_PERIPHERALS_OFF:
			send_data_cus(TMS_MSG_TX_SHUTDOWN);
			break;
		default:
			break;
	}
}

///////////////////PREDEFINED FUNCTIONS////////////////////////////////////////

void ble_init(ble_nus_rx_handler_t ble_nus_rx_handler, ble_cus_rx_handler_t ble_cus_rx_handler) {    
	NRF_LOG_INFO("Starting BLE initialization...");
	NRF_LOG_FLUSH();

	nus_rx_handler = ble_nus_rx_handler;
	cus_rx_handler = ble_cus_rx_handler;

	ble_stack_init();
	NRF_LOG_INFO("\tBLE stack initialzied");
	NRF_LOG_FLUSH();
    gap_params_init();
	NRF_LOG_INFO("\tGAP parameters initialized");
	NRF_LOG_FLUSH();
    gatt_init();
	NRF_LOG_INFO("\tGatt initialized.");
	NRF_LOG_FLUSH();
    services_init();
	NRF_LOG_INFO("\tServices initialized.");
	NRF_LOG_FLUSH();
    advertising_init();
	NRF_LOG_INFO("\tAdvertising initialized");
	NRF_LOG_FLUSH();
    conn_params_init();
	NRF_LOG_INFO("\tConnection parameters intialized");
	NRF_LOG_FLUSH();
}


void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_RX_DATA) {
        nus_rx_handler(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
    }
}


static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
	ble_cus_init_t     cus_init;
    nrf_ble_qwr_init_t qwr_init = {0};
	

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));
    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);

	// Initialize CUS Service init structure to zero.
	memset(&cus_init, 0, sizeof(cus_init));
    cus_init.evt_handler = on_cus_evt;
    
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cus_init.custom_value_char_attr_md.cccd_write_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cus_init.custom_value_char_attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cus_init.custom_value_char_attr_md.write_perm);

	err_code = ble_cus_init(&m_cus, &cus_init);

	//Make sure to
	//1. Set the NRF_SDH_BLE_VS_UUID_COUNT in sdk_config.h correctly
	//2. Set the section pacement macros section in the linker correctly
	//Otherwise, you will experience a NRF_ERROR_NO_MEM here
	APP_ERROR_CHECK(err_code);
}


static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


static void sleep_mode_enter(void)
{
    uint32_t err_code;

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            //APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            //sleep_mode_enter();
			err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
            APP_ERROR_CHECK(err_code);

            break;
        default:
            break;
    }
}


static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);

			//Set the radio TX power to +4dBm
			err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_conn_handle, 4);
			APP_ERROR_CHECK(err_code);

            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}	


void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}

static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

static void on_cus_evt(ble_cus_evt_t * p_evt)
{
	uint32_t err_code;

    switch(p_evt->evt_type)
    {
        case BLE_CUS_EVT_NOTIFICATION_ENABLED:
			send_system_state();
            break;

        case BLE_CUS_EVT_NOTIFICATION_DISABLED:
            break;

        case BLE_CUS_EVT_CONNECTED :
            break;

        case BLE_CUS_EVT_DISCONNECTED:
            break;
		
		case BLE_CUS_EVT_RX_DATA:
			//cus_data_received(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);			
			//Currently, the max length is assumed to be 1 byte.
			cus_rx_handler(p_evt->params.rx_data.p_data[0]);			

			break;
        default:
              // No implementation needed.
              break;
    }
}