/*
 * Copyright (C) 2013,2016-2017 The Linux Foundation. All rights reserved
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

#define LOG_TAG "BluetoothVendorJni"

#include "com_android_bluetooth.h"
#include <hardware/vendor.h>
#include "utils/Log.h"
#include "android_runtime/AndroidRuntime.h"


namespace android {

static jmethodID method_onBredrCleanup;
static jmethodID method_iotDeviceBroadcast;

static btvendor_interface_t *sBluetoothVendorInterface = NULL;
static jobject mCallbacksObj = NULL;

static void bredr_cleanup_callback(bool status){

    ALOGI("%s", __FUNCTION__);
    CallbackEnv sCallbackEnv(__func__);

    if (!sCallbackEnv.valid()) return;

    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onBredrCleanup, (jboolean)status);
}

static void iot_device_broadcast_callback(RawAddress* bd_addr, uint16_t error,
        uint16_t error_info, uint32_t event_mask, uint8_t lmp_ver, uint16_t lmp_subver,
        uint16_t manufacturer_id, uint8_t power_level, uint8_t rssi, uint8_t link_quality){
    ALOGI("%s", __FUNCTION__);
    CallbackEnv sCallbackEnv(__func__);
    if (!sCallbackEnv.valid()) return;
    ScopedLocalRef<jbyteArray> addr(
    sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
    if (!addr.get()) {
        ALOGE("Error while allocation byte array in %s", __func__);
        return;
    }
    sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                               (jbyte*)bd_addr);
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_iotDeviceBroadcast, addr.get(), (jint)error,
                    (jint)error_info, (jint)event_mask, (jint)lmp_ver, (jint)lmp_subver,
                    (jint)manufacturer_id, (jint)power_level, (jint)rssi, (jint)link_quality);
}
static btvendor_callbacks_t sBluetoothVendorCallbacks = {
    sizeof(sBluetoothVendorCallbacks),
    bredr_cleanup_callback,
    iot_device_broadcast_callback
};

static void classInitNative(JNIEnv* env, jclass clazz) {

    method_onBredrCleanup = env->GetMethodID(clazz, "onBredrCleanup", "(Z)V");
    method_iotDeviceBroadcast = env->GetMethodID(clazz, "iotDeviceBroadcast", "([BIIIIIIIII)V");
    ALOGI("%s: succeeds", __FUNCTION__);
}

static void initNative(JNIEnv *env, jobject object) {
    const bt_interface_t* btInf;
    bt_status_t status;

    if ( (btInf = getBluetoothInterface()) == NULL) {
        ALOGE("Bluetooth module is not loaded");
        return;
    }

    if (mCallbacksObj != NULL) {
        ALOGW("Cleaning up Bluetooth Vendor callback object");
        env->DeleteGlobalRef(mCallbacksObj);
        mCallbacksObj = NULL;
    }

    if ( (sBluetoothVendorInterface = (btvendor_interface_t *)
          btInf->get_profile_interface(BT_PROFILE_VENDOR_ID)) == NULL) {
        ALOGE("Failed to get Bluetooth Vendor Interface");
        return;
    }

    if ( (status = sBluetoothVendorInterface->init(&sBluetoothVendorCallbacks))
                 != BT_STATUS_SUCCESS) {
        ALOGE("Failed to initialize Bluetooth Vendor, status: %d", status);
        sBluetoothVendorInterface = NULL;
        return;
    }
    mCallbacksObj = env->NewGlobalRef(object);
}

static void cleanupNative(JNIEnv *env, jobject object) {
    const bt_interface_t* btInf;

    if ( (btInf = getBluetoothInterface()) == NULL) {
        ALOGE("Bluetooth module is not loaded");
        return;
    }

    if (sBluetoothVendorInterface !=NULL) {
        ALOGW("Cleaning up Bluetooth Vendor Interface...");
        sBluetoothVendorInterface->cleanup();
        sBluetoothVendorInterface = NULL;
    }

    if (mCallbacksObj != NULL) {
        ALOGW("Cleaning up Bluetooth Vendor callback object");
        env->DeleteGlobalRef(mCallbacksObj);
        mCallbacksObj = NULL;
    }

}


static bool bredrcleanupNative(JNIEnv *env, jobject obj) {

    ALOGI("%s", __FUNCTION__);

    jboolean result = JNI_FALSE;
    if (!sBluetoothVendorInterface) return result;

    sBluetoothVendorInterface->bredrcleanup();
    return JNI_TRUE;
}


static bool setWifiStateNative(JNIEnv *env, jobject obj, jboolean status) {

    ALOGI("%s", __FUNCTION__);

    jboolean result = JNI_FALSE;
    if (!sBluetoothVendorInterface) return result;

    sBluetoothVendorInterface->set_wifi_state(status);
    return JNI_TRUE;
}

static bool getProfileInfoNative(JNIEnv *env, jobject obj, jint profile_id , jint profile_info) {

    ALOGI("%s", __FUNCTION__);

    jboolean result = JNI_FALSE;

    if (!sBluetoothVendorInterface) return result;

    result = sBluetoothVendorInterface->get_profile_info((profile_t)profile_id, (profile_info_t)profile_info);

    return result;
}

static bool getQtiStackStatusNative() {
    return (sBluetoothVendorInterface != NULL);
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void *) classInitNative},
    {"initNative", "()V", (void *) initNative},
    {"cleanupNative", "()V", (void *) cleanupNative},
    {"bredrcleanupNative", "()V", (void*) bredrcleanupNative},
    {"setWifiStateNative", "(Z)V", (void*) setWifiStateNative},
    {"getProfileInfoNative", "(II)Z", (void*) getProfileInfoNative},
    {"getQtiStackStatusNative", "()Z", (void*) getQtiStackStatusNative},

};

int register_com_android_bluetooth_btservice_vendor(JNIEnv* env)
{
    ALOGE("%s:",__FUNCTION__);
    return jniRegisterNativeMethods(env, "com/android/bluetooth/btservice/Vendor",
                                    sMethods, NELEM(sMethods));
}

} /* namespace android */
