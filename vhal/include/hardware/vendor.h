/*
 * Copyright (C) 2016 The Linux Foundation. All rights reserved
 * Not a Contribution.
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_INCLUDE_BT_VENDOR_H
#define ANDROID_INCLUDE_BT_VENDOR_H

#include <hardware/bluetooth.h>

__BEGIN_DECLS

#define BT_PROFILE_VENDOR_ID "vendor"
#define BT_PROFILE_WIPOWER_VENDOR_ID "wipower"

// Profile related Enums & function
typedef enum {
  AVRCP_ID = 1,
  PBAP_ID,
  MAP_ID,
  END_OF_PROFILE_LIST
} profile_t;


typedef enum {
 VERSION = 1,
 AVRCP_COVERART_SUPPORT,
 AVRCP_0103_SUPPORT,
 USE_SIM_SUPPORT,
 MAP_EMAIL_SUPPORT,
 PBAP_0102_SUPPORT,
 END_OF_FEATURE_LIST
 } profile_info_t;

// Vendor Callbacks
/** Callback when bredr cleanup is done.
 */
typedef void (*  btvendor_bredr_cleanup_callback)(bool status);
typedef void (*  btvendor_iot_device_broadcast_callback)(RawAddress* remote_bd_addr, uint16_t error,
                        uint16_t error_info, uint32_t event_mask, uint8_t lmp_ver, uint16_t lmp_subver,
                        uint16_t manufacturer_id, uint8_t power_level, uint8_t rssi, uint8_t link_quality );

/** BT-Vendor callback structure. */
typedef struct {
    /** set to sizeof(BtVendorCallbacks) */
    size_t      size;
    btvendor_bredr_cleanup_callback  bredr_cleanup_cb;
    btvendor_iot_device_broadcast_callback iot_device_broadcast_cb;
} btvendor_callbacks_t;

/** Represents the standard BT-Vendor interface.
 */
typedef struct {

    /** set to sizeof(BtVendorInterface) */
    size_t  size;

    /**
     * Register the BtVendor callbacks
     */
    bt_status_t (*init)( btvendor_callbacks_t* callbacks );

    /** test interface. */
    const void* (*get_testapp_interface)(int);

    /** Does BREDR cleanup */
    void (*bredrcleanup)(void);

    /** set wifi state */
    void (*set_wifi_state)(bool);

    /** get profile info */
    bool (*get_profile_info)(profile_t, profile_info_t);

    /** Closes the interface. */
    void  (*cleanup)( void );

} btvendor_interface_t;

__END_DECLS

#endif /* ANDROID_INCLUDE_BT_VENDOR_H */

