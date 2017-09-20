/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package com.android.bluetooth.opp;

import com.android.bluetooth.R;

import java.io.File;
import javax.obex.ResponseCodes;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothProfile;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;
import android.os.PowerManager.WakeLock;
import android.os.Handler;
import android.os.PowerManager;
import android.os.SystemProperties;


public class BTOppUtils {

    private static final String TAG="BtOppUtils";
    private static final boolean V = Constants.VERBOSE;
    private static final boolean D = Constants.DEBUG;

    protected static boolean mZeroLengthFile = false;

    protected static boolean isA2DPPlaying;

    public static boolean isA2DPConnected;

    private static WakeLock mWakeLock;

    protected static boolean isScreenOff = false;

    protected static final int UPDATE_PROVIDER = 5;

    /*
     *  Grant permission to access a specific Uri.
     */
    static void grantPermissionToUri(Context ctx ,ClipData clipData) {
        if (clipData == null) {
            Log.i(TAG, "ClipData is null ");
            return;
        }
        try {
            String packageName = ctx.getPackageName();
            for (int i = 0; i < clipData.getItemCount(); i++) {
                ClipData.Item item = clipData.getItemAt(i);
                Uri uri = item.getUri();
                if (uri != null) {
                    String scheme = uri.getScheme();
                    if (scheme != null && scheme.equals(ContentResolver.SCHEME_CONTENT)) {
                        ctx.grantUriPermission(packageName, uri,
                                Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    }
                }
            }
        } catch (Exception e) {
          Log.e(TAG,"GrantUriPermission :" + e.toString());
        }
    }

    protected static Uri originalUri(Uri uri) {
        String mUri = uri.toString();
        int atIndex = mUri.lastIndexOf("@");
        if (atIndex != -1) {
            mUri = mUri.substring(0, atIndex);
            uri = Uri.parse(mUri);
        }
        if (V) Log.v(TAG, "originalUri: " + uri);
        return uri;
    }

    protected static Uri generateUri(Uri uri, BluetoothOppSendFileInfo sendFileInfo) {
        String fileInfo = sendFileInfo.toString();
        int atIndex = fileInfo.lastIndexOf("@");
        fileInfo = fileInfo.substring(atIndex);
        uri = Uri.parse(uri + fileInfo);
        if (V) Log.v(TAG, "generateUri: " + uri);
        return uri;
    }

    protected static void retryFailedTrasfer(final Context ctx, final BluetoothOppTransferInfo
            transInfo) {
        new Thread() {
            @Override
            public void run() {
                super.run();
                // retry the failed transfer
                Uri uri = originalUri(Uri.parse(transInfo.mFileUri));
                BluetoothOppSendFileInfo sendFileInfo = BluetoothOppSendFileInfo.generateFileInfo(
                        ctx, uri, transInfo.mFileType, false);
                uri = generateUri(uri, sendFileInfo);
                BluetoothOppUtility.putSendFileInfo(uri, sendFileInfo);
                transInfo.mFileUri = uri.toString();
                BluetoothOppUtility.retryTransfer(ctx, transInfo);
            }
        }.start();
    }

    protected static void isZeroLengthFile(Context ctx, String toastMsg, String fileName) {
        if (mZeroLengthFile) {
            toastMsg = ctx.getString(R.string.notification_sent_fail, fileName);
            mZeroLengthFile = false;
            Log.d(TAG, " ZeroLengthFile :" + fileName);
        }
    }

    protected static boolean isZeroLengthFileRejected(long fileLength, int responseCode) {
        /* Set if the file length is zero and it's rejected by remote */
        mZeroLengthFile = (fileLength == 0
                && responseCode == ResponseCodes.OBEX_HTTP_LENGTH_REQUIRED) ? true : false;
        if (D) Log.d(TAG, " isZeroLengthFileRejected :" + mZeroLengthFile);
        return mZeroLengthFile;
    }

    protected static int setZeroLengthFileStatus (int status) {
        /* Mark the status as success when a zero length file is rejected
         * by the remote device. It allows us to continue the transfer if
         * we have a batch and the file(s) are yet be sent in the row.
         */
        status = (mZeroLengthFile) ? BluetoothShare.STATUS_SUCCESS : status;
        if (D) Log.d(TAG, " setZeroLengthFileStatus :" + mZeroLengthFile + " status :" + status);
        return status ;
    }

    protected static void cleanFile(String fileName) {
        if(fileName == null)
            return;
        if(V) Log.v(TAG, "File to be deleted: " + fileName);
        File fileToDelete = new File(fileName);
        if (fileToDelete != null)
            fileToDelete.delete();
    }

    protected static void cleanOnPowerOff(ContentResolver cr) {
        String WHERE_INTERRUPTED_ON_POWER_OFF = "( " + BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_INBOUND + " OR "+BluetoothShare.DIRECTION + "="
                + BluetoothShare.DIRECTION_OUTBOUND + " ) AND " + BluetoothShare.STATUS + "="
                + BluetoothShare.STATUS_RUNNING;

        Cursor cursorToFile = cr.query(BluetoothShare.CONTENT_URI,
                new String[] { BluetoothShare._DATA }, WHERE_INTERRUPTED_ON_POWER_OFF, null, null);

        /*
         * remove the share and the respective file which was interrupted by
         * battery removal in the local device
         */
        if (cursorToFile != null) {
            if (cursorToFile.moveToFirst()) {
                cleanFile(cursorToFile.getString(0));
                int delNum = cr.delete(BluetoothShare.CONTENT_URI,
                        WHERE_INTERRUPTED_ON_POWER_OFF, null);
                if (V) Log.v(TAG, "Delete aborted inbound share, number = " + delNum);
            }
        }
    }

    protected static void checkAction(Intent intent) {
        String action = intent.getAction();
        if (action.equals(Intent.ACTION_SCREEN_OFF)) {
            isScreenOff = true;
        } else if (action.equals(Intent.ACTION_SCREEN_ON)) {
            isScreenOff = false;
        } else if (action.equals(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED)) {
            int newState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE,
                    BluetoothProfile.STATE_DISCONNECTED);
            BluetoothDevice device = intent.getParcelableExtra(
                    BluetoothDevice.EXTRA_DEVICE);
            if (device == null) {
                Log.i(TAG, "Device is NULL");
                return;
            }
            isA2DPPlaying = false;
            isA2DPConnected = (newState == BluetoothProfile.STATE_CONNECTED) ?
                    true : false;
            if (D) Log.d(TAG, "device: " + device + " newState: " + newState +
                    " isA2DPConnected: " + isA2DPConnected);

        } else if (action.equals(BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED)) {
            int newState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE,
                    BluetoothA2dp.STATE_NOT_PLAYING);
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (D) Log.d(TAG, "device: " + device + " newState: " + newState);
            if (device == null) {
                Log.i(TAG, "Device is NULL");
                return;
            }
            isA2DPPlaying = (newState == BluetoothA2dp.STATE_PLAYING) ? true : false;
            if (D) Log.d(TAG, " isA2DPPlaying :" + isA2DPPlaying);
        }
    }

    /*
     * Automation team not able to find incoming file notification
     * so broad cast to CST APP when receive incoming file request
     * @ Condition set persistent property using adb "persist.sys.opp" opp
     */
    protected static void sendVendorDebugBroadcast(Context ctx) {
        String INCOMING_FILE_NOTIFICATION = "android.btopp.intent.action.INCOMING_FILE_NOTIFICATION";
        String property = SystemProperties.get("persist.sys.opp", "");
        if (property.equals("opp")) {
            Intent intent = new Intent(INCOMING_FILE_NOTIFICATION);
            intent.setComponent(new android.content.ComponentName("com.android.CST",
                "com.android.CST.ConnectivitySystemTest.OppIncomingReceiver"));
            ctx.sendBroadcast(intent);
            if(D) Log.d(TAG, "intent :" + intent);
        }
        if(D) Log.d(TAG, "property :" + property +":");
    }

    /**
     * Returns the throughput of the file transfer
     */
    protected static void throughputInKbps(long fileSize, long beginTime) {
        int min_file_len_for_tp_measure = 500000;
        if (fileSize > min_file_len_for_tp_measure) {
            double tp = (fileSize * 1024 * 8) / ((System.currentTimeMillis() - beginTime) * 1024);
            if (D) Log.d(TAG, " Approx. throughput is " + tp + " Kbps");
        } else {
            if (D) Log.d(TAG, "File size is too small to measure throughput");
        }
    }

    protected static void acquireFullWakeLock(PowerManager pm, String tag){
        mWakeLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP
                | PowerManager.ON_AFTER_RELEASE,
                tag);
    }

    protected static void isTurnOnScreen (Context context, boolean needConfirm) {
        if(D) Log.d(TAG, "Received incoming file request");
        if (needConfirm) {
            if (!mWakeLock.isHeld() && isScreenOff) {
                if (D) Log.d(TAG, "acquire full WakeLock");
                mWakeLock.acquire();
            }
            sendVendorDebugBroadcast(context);
        }
    }

    protected static void acquirePartialWakeLock(WakeLock partialWakeLock) {
        if (mWakeLock.isHeld()) {
            mWakeLock.release();
        }
        if (!partialWakeLock.isHeld()) {
            if (D) Log.d(TAG, "acquire partial WakeLock");
            partialWakeLock.acquire();
        }
    }

    protected static void releaseFullWakeLock(){
        if (mWakeLock.isHeld()) {
            if (D) Log.d(TAG, "releasing full wakelock");
            mWakeLock.release();
        }
    }

    protected static void checkScreenState(Context ctx, PowerManager powerManager,
            IntentFilter filter) {
        powerManager = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
        isScreenOff = !powerManager.isInteractive();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED);
    }


    protected static void updateProviderFromhandler(Handler handler) {
        if (!handler.hasMessages(UPDATE_PROVIDER)) {
            handler.sendMessage(handler.obtainMessage(UPDATE_PROVIDER));
        }
    }

    protected static void isScreenTurnedOff(boolean isInterrupted){
        try {
            if (isScreenOff && !isInterrupted) {
                Thread.sleep(1000);
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted", e);
        }

    }

}
