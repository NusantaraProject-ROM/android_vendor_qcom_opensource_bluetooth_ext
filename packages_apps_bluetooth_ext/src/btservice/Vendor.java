/*
 * Copyright (C) 2016-2017 The Linux Foundation. All rights reserved
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

package com.android.bluetooth.btservice;

import android.util.Log;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothClass;
import com.android.bluetooth.Utils;

import android.content.Intent;
import android.content.Context;
final class Vendor {
    private static final String TAG = "BluetoothVendorService";
    private AdapterService mService;
    private boolean isQtiStackEnabled;

    static {
        classInitNative();
    }

    public Vendor(AdapterService service) {
        mService = service;
    }

    public void init(){
        initNative();
        isQtiStackEnabled = getQtiStackStatusNative();
        Log.d(TAG,"Qti Stack enabled status: " + isQtiStackEnabled);
    }

    public void bredrCleanup() {
        bredrcleanupNative();
    }

    public void setWifiState(boolean status) {
        Log.d(TAG,"setWifiState to: " + status);
        setWifiStateNative(status);
    }

    public boolean getProfileInfo(int profile_id , int profile_info) {
        Log.d(TAG,"getProfileInfo profile_id: " + profile_id);
        return getProfileInfoNative(profile_id, profile_info);
    }

    public boolean getQtiStackStatus() {
        return isQtiStackEnabled;
    }

    public void cleanup() {
        cleanupNative();
    }

   private void onBredrCleanup(boolean status) {
        Log.d(TAG,"BREDR cleanup done");
        mService.startBluetoothDisable();
    }

    private void iotDeviceBroadcast(byte[] remoteAddr,
                int error, int error_info, int event_mask, int lmpVer, int lmpSubVer,
                int manufacturerId,int pwr_level, int rssi, int linkQuality) {
        String mRemoteAddr = Utils.getAddressStringFromByte(remoteAddr);
        BluetoothDevice mBluetoothDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(mRemoteAddr);
        String mRemoteName = mService.getRemoteName(mBluetoothDevice);
        int mRemoteCoD = mService.getRemoteClass(mBluetoothDevice);
        Log.d(TAG,"iotDeviceBroadcast " + mRemoteName + " address: " + mRemoteAddr + " error: " + error
                    + " error info: " + error_info + " event mask: " + event_mask + "Class of Device: " + mRemoteCoD
                    + " lmp version: " + lmpVer + " lmp subversion: " + lmpSubVer + " manufacturer: " + manufacturerId
                    + " power level: " + pwr_level + " rssi: " + rssi + " link quality: " + linkQuality);
        // Commented due to frame work dependent change
        //Intent intent = new Intent(BluetoothDevice.ACTION_REMOTE_ISSUE_OCCURRED);
        //intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mRemoteAddr);
        //intent.putExtra(BluetoothDevice.EXTRA_NAME, mRemoteName);
        //intent.putExtra(BluetoothDevice.EXTRA_CLASS, mRemoteCoD);
        //intent.putExtra(BluetoothDevice.EXTRA_ISSUE_TYPE, error);
        //intent.putExtra(BluetoothDevice.EXTRA_ERROR_CODE, error_info);
        //intent.putExtra(BluetoothDevice.EXTRA_ERROR_EVENT_MASK, event_mask);
        //intent.putExtra(BluetoothDevice.EXTRA_LMP_VERSION, lmpVer);
        //intent.putExtra(BluetoothDevice.EXTRA_LMP_SUBVER, lmpSubVer);
        //intent.putExtra(BluetoothDevice.EXTRA_MANUFACTURER, manufacturerId);
        //intent.putExtra(BluetoothDevice.EXTRA_POWER_LEVEL, pwr_level);
        //intent.putExtra(BluetoothDevice.EXTRA_RSSI, rssi);
        //intent.putExtra(BluetoothDevice.EXTRA_LINK_QUALITY, linkQuality);
        //mService.sendBroadcast(intent, AdapterService.BLUETOOTH_PERM);
    }
    private native void bredrcleanupNative();
    private native void initNative();
    private native static void classInitNative();
    private native void cleanupNative();
    private native void setWifiStateNative(boolean status);
    private native boolean getProfileInfoNative(int profile_id , int profile_info);
    private native boolean getQtiStackStatusNative();
}
