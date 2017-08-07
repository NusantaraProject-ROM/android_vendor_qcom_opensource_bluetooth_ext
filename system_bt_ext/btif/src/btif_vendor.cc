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

#if TEST_APP_INTERFACE == TRUE
#include <bt_testapp.h>
#endif

#define BTA_SERVICE_ID_TO_SERVICE_MASK(id)  (1 << (id))
extern bt_status_t btif_in_execute_service_request(tBTA_SERVICE_ID service_id,
                                               bool b_enable);
extern bool interface_ready(void);
btvendor_callbacks_t *bt_vendor_callbacks = NULL;

#define BTIF_VENDOR_BREDR_CLEANUP 1

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
