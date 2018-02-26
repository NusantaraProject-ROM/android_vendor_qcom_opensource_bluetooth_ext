/*
 * Copyright (C) 2016 The Linux Foundation. All rights reserved
 * Not a Contribution.
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/************************************************************************************
 *
 *  Filename:      btif_vendor.cc
 *
 *  Description:   Vendor Bluetooth Interface
 *
 *
 ***********************************************************************************/

#include <hardware/vendor.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "bt_btif_vendor"

#include <cutils/properties.h>
#include "bt_utils.h"
#include "btif_common.h"
#include "btif_util.h"
#include "btif_profile_queue.h"
#include "stack_manager.h"
#include "l2cdefs.h"
#include "l2c_api.h"
#include "stack_config.h"
#include "btm_api.h"
#include "profile_config.h"

#if TEST_APP_INTERFACE == TRUE
#include <bt_testapp.h>
#endif

#define BTA_SERVICE_ID_TO_SERVICE_MASK(id)  (1 << (id))
#define CALLBACK_TIMER_PERIOD_MS      (60000)
#define BTIF_VENDOR_BREDR_CLEANUP 1

typedef struct {
  RawAddress bd_addr; /* BD address peer device. */
  uint16_t error;
  uint16_t error_info;
  uint32_t event_mask;
  uint8_t power_level;
  uint8_t rssi;
  uint8_t link_quality;
  uint8_t lmp_ver;
  uint16_t lmp_subver;
  uint16_t manufacturer_id;
  bool is_valid;
} BTIF_VND_IOT_INFO_CB_DATA;

extern bt_status_t btif_in_execute_service_request(tBTA_SERVICE_ID service_id,
                                               bool b_enable);
extern bt_status_t btif_storage_get_remote_device_property(RawAddress *remote_bd_addr,
                                               bt_property_t *property);
extern tBTM_STATUS BTM_ReadRemoteVersion(const RawAddress& addr, uint8_t* lmp_version,
                                  uint16_t* manufacturer,
                                  uint16_t* lmp_sub_version);
extern bool interface_ready(void);
void btif_vendor_cleanup_iot_broadcast_timer(void);

btvendor_callbacks_t *bt_vendor_callbacks = NULL;
static alarm_t *broadcast_cb_timer = NULL;
static void btif_broadcast_timer_cb(UNUSED_ATTR void *data);
BTIF_VND_IOT_INFO_CB_DATA broadcast_cb_data;

#if TEST_APP_INTERFACE == TRUE
extern const btl2cap_interface_t *btif_l2cap_get_interface(void);
extern const btrfcomm_interface_t *btif_rfcomm_get_interface(void);
extern const btmcap_interface_t *btif_mcap_get_interface(void);
#endif
/*******************************************************************************
** VENDOR INTERFACE FUNCTIONS
*******************************************************************************/

/*******************************************************************************
**
** Function         btif_vendor_init
**
** Description     initializes the vendor interface
**
** Returns         bt_status_t
**
*******************************************************************************/
static bt_status_t init( btvendor_callbacks_t* callbacks)
{
    bt_vendor_callbacks = callbacks;
    broadcast_cb_timer = alarm_new("btif_vnd.cb_timer");
    return BT_STATUS_SUCCESS;
}

static void btif_vendor_bredr_cleanup_event(uint16_t event, char *p_param)
{
    tBTA_SERVICE_MASK service_mask;
    uint32_t i;
    service_mask = btif_get_enabled_services_mask();
    for (i = 0; i <= BTA_MAX_SERVICE_ID; i++)
    {
        if (i != BTA_BLE_SERVICE_ID && (service_mask &
              (tBTA_SERVICE_MASK)(BTA_SERVICE_ID_TO_SERVICE_MASK(i))))
        {
            btif_in_execute_service_request(i, FALSE);
        }
    }
    btif_queue_release();
    HAL_CBACK(bt_vendor_callbacks, bredr_cleanup_cb, true);
}

static void btif_vendor_send_iot_info_cb(uint16_t event, char *p_param)
{
    broadcast_cb_data.is_valid = false;
    HAL_CBACK(bt_vendor_callbacks, iot_device_broadcast_cb,
            &broadcast_cb_data.bd_addr, broadcast_cb_data.error,
            broadcast_cb_data.error_info, broadcast_cb_data.event_mask,
            broadcast_cb_data.lmp_ver, broadcast_cb_data.lmp_subver,
            broadcast_cb_data.manufacturer_id, broadcast_cb_data.power_level,
            broadcast_cb_data.rssi, broadcast_cb_data.link_quality);
}

void btif_broadcast_timer_cb(UNUSED_ATTR void *data) {
    btif_transfer_context(btif_vendor_send_iot_info_cb, 1, NULL, 0, NULL);
}

void btif_vendor_cleanup_iot_broadcast_timer()
{
    if(broadcast_cb_timer) {
        alarm_free(broadcast_cb_timer);
        broadcast_cb_timer = NULL;
    }
}

void btif_vendor_iot_device_broadcast_event(RawAddress* bd_addr,
                uint16_t error, uint16_t error_info, uint32_t event_mask,
                uint8_t power_level, uint8_t rssi, uint8_t link_quality)
{
    uint8_t lmp_ver;
    uint16_t lmp_subver, manufacturer_id;

    BTM_ReadRemoteVersion(*bd_addr, &lmp_ver, &manufacturer_id, &lmp_subver);
    if(error == BT_SOC_A2DP_GLITCH || error == BT_HOST_A2DP_GLITCH)
    {
        if(broadcast_cb_data.is_valid)
        {
            broadcast_cb_data.error_info = broadcast_cb_data.error_info|error_info;
            if(error == BT_SOC_A2DP_GLITCH && broadcast_cb_data.error_info == BT_HOST_A2DP_GLITCH)
            {
                broadcast_cb_data.event_mask = broadcast_cb_data.event_mask|event_mask;
                broadcast_cb_data.power_level = power_level;
                broadcast_cb_data.rssi = rssi;
                broadcast_cb_data.link_quality = link_quality;
            }
            return;
        }
        else if(broadcast_cb_timer)
        {
            broadcast_cb_data.bd_addr = *bd_addr;
            broadcast_cb_data.error = error;
            broadcast_cb_data.error_info = error_info;
            broadcast_cb_data.event_mask = event_mask;
            broadcast_cb_data.power_level = power_level;
            broadcast_cb_data.rssi = rssi;
            broadcast_cb_data.link_quality = link_quality;
            broadcast_cb_data.lmp_ver = lmp_ver;
            broadcast_cb_data.lmp_subver = lmp_subver;
            broadcast_cb_data.manufacturer_id = manufacturer_id;
            broadcast_cb_data.is_valid = true;

            alarm_set(broadcast_cb_timer, CALLBACK_TIMER_PERIOD_MS, btif_broadcast_timer_cb, NULL);
            return;
        }
    }

    HAL_CBACK(bt_vendor_callbacks, iot_device_broadcast_cb, bd_addr,
            error, error_info, event_mask, lmp_ver, lmp_subver,
            manufacturer_id, power_level, rssi, link_quality);
}
static void bredrcleanup(void)
{
    LOG_INFO(LOG_TAG,"bredrcleanup");
    btif_transfer_context(btif_vendor_bredr_cleanup_event,BTIF_VENDOR_BREDR_CLEANUP,
                          NULL, 0, NULL);
}

static void set_wifi_state(bool status)
{
    LOG_INFO(LOG_TAG,"setWifiState :%d", status);
    BTA_DmSetWifiState(status);
}

static bool get_profile_info(profile_t profile, profile_info_t feature_name)
{
    LOG_INFO(LOG_TAG,"get_profile_info :%d", profile);
    return profile_feature_fetch(profile,feature_name);
}
static void cleanup(void)
{
    LOG_INFO(LOG_TAG,"cleanup");
    if (bt_vendor_callbacks)
        bt_vendor_callbacks = NULL;
}


/*******************************************************************************
**
** Function         get_testapp_interface
**
** Description      Get the Test interface
**
** Returns          btvendor_interface_t
**
*******************************************************************************/
#if TEST_APP_INTERFACE == TRUE
static const void* get_testapp_interface(int test_app_profile)
{
    if (interface_ready() == FALSE) {
        return NULL;
    }
    ALOGI("get_testapp_interface %d", test_app_profile);
    switch(test_app_profile) {
        case TEST_APP_L2CAP:
            return btif_l2cap_get_interface();
        case TEST_APP_RFCOMM:
            return btif_rfcomm_get_interface();
        case TEST_APP_MCAP:
           return btif_mcap_get_interface();
        default:
            return NULL;
    }
    return NULL;
}
#endif

static const btvendor_interface_t btvendorInterface = {
    sizeof(btvendorInterface),
    init,
#if TEST_APP_INTERFACE == TRUE
    get_testapp_interface,
#else
    NULL,
#endif
    bredrcleanup,
    set_wifi_state,
    get_profile_info,
    cleanup,
};

/*******************************************************************************
** LOCAL FUNCTIONS
*******************************************************************************/

/*******************************************************************************
**
** Function         btif_vendor_get_interface
**
** Description      Get the vendor callback interface
**
** Returns          btvendor_interface_t
**
*******************************************************************************/
const btvendor_interface_t *btif_vendor_get_interface()
{
    BTIF_TRACE_EVENT("%s", __FUNCTION__);
    return &btvendorInterface;
}
