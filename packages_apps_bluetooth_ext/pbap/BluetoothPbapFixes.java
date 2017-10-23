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

package com.android.bluetooth.pbap;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothPbap;
import android.content.Context;
import android.database.Cursor;
import android.database.CursorWindowAllocationException;
import android.database.MatrixCursor;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.SystemProperties;
import android.os.UserManager;
import android.provider.ContactsContract.CommonDataKinds;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.Data;
import android.util.Log;

import com.android.bluetooth.ObexServerSockets;
import com.android.bluetooth.pbap.BluetoothPbapVcardManager.VCardFilter;
import com.android.bluetooth.R;
import com.android.bluetooth.opp.BTOppUtils;
import com.android.bluetooth.sdp.SdpManager;

import java.util.ArrayList;
import java.util.Collections;

import javax.obex.ServerSession;

public class BluetoothPbapFixes {

    private static final String TAG = "BluetoothPbapFixes";

    public static final boolean DEBUG = BluetoothPbapService.DEBUG;

    public static final boolean VERBOSE = BluetoothPbapService.VERBOSE;

    static final String[] PHONES_PROJECTION = new String[] {
        Data._ID, // 0
        CommonDataKinds.Phone.TYPE, // 1
        CommonDataKinds.Phone.LABEL, // 2
        CommonDataKinds.Phone.NUMBER, // 3
        Contacts.DISPLAY_NAME, // 4
    };

    protected static final int SDP_PBAP_LEGACY_SERVER_VERSION = 0x0101;

    protected static final int SDP_PBAP_LEGACY_SUPPORTED_REPOSITORIES = 0x0001;

    protected static final int SDP_PBAP_LEGACY_SUPPORTED_FEATURES = 0x0003;

    protected static boolean isSimSupported = true;

    protected static boolean isSupportedPbap12 = true;

    /* To get feature support from config file */
    protected static void getFeatureSupport(Context context) {
        isSimSupported = context.getResources().getBoolean(R.bool.pbap_use_sim_support);
        isSupportedPbap12 = context.getResources().getBoolean(R.bool.pbap_12_support);
        if (DEBUG)
            Log.d(TAG, "isSimSupported :" + isSimSupported + " Pbap 1.2 support: "
                    + isSupportedPbap12);
    }

    /* To sort name list obtained when search attribute is number*/
    public static void sortNameList(int mOrderBy, String searchValue,
            ArrayList<String> names) {

        // Check if the order required is alphabetic
        if (mOrderBy == BluetoothPbapObexServer.ORDER_BY_ALPHABETICAL) {
            if (VERBOSE)
                Log.v(TAG, "Name list created for Search Value ="+ searchValue
                    +". Order By: " + mOrderBy);
            Collections.sort(names);
        }
    }

    /* Used to fetch vCard entry from contact cursor for required handle value*/
    public static MatrixCursor getVcardEntry(Cursor contactCursor,
            MatrixCursor contactIdsCursor, int contactIdColumn, int startPoint) {
        while (contactCursor.moveToNext()) {
            long currentContactId = contactCursor.getLong(contactIdColumn);
             if (currentContactId == startPoint) {
                contactIdsCursor.addRow(new Long[]{currentContactId});
                if (VERBOSE) Log.v(TAG, "contactIdsCursor.addRow: " + currentContactId);
                break;
            }
        }
        BluetoothPbapVcardManager.isPullVcardEntry = false;
        return contactIdsCursor;
    }

    /* Used to fetch handle value from the name in request*/
    public static String getHandle(String value) {
        if (value != null) {
            return value.substring(value.lastIndexOf(',') + 1, value.length());
        }
        return "-1";
    }

    /* Used to position a given vCard entry in list for vCardListing*/
    public static ArrayList<Integer> addToListAtPos(ArrayList<Integer> list,
            int pos, String handle) {
        if (handle != null && Integer.parseInt(handle) >= 0) {
            list.add(Integer.parseInt(handle));
        } else {
            list.add(pos);
        }
        return list;
    }

    /* TO check if given contact_id for given handle value is present or not*/
    public static final boolean checkContactsVcardId(int id, Context mContext) {
        if (id == 0) {
            return true;
        }
        Cursor contactCursor = null;
        try {
            contactCursor = mContext.getContentResolver().query(Phone.CONTENT_URI,
                    PHONES_PROJECTION, Phone.CONTACT_ID+"= ?",
                    new String[] {id + ""}, null);

            if (contactCursor != null && contactCursor.getCount() > 0) {
                return true;
            } else {
                return false;
            }
        } catch (CursorWindowAllocationException e) {
            Log.e(TAG, "CursorWindowAllocationException while checking Contacts id");
        } finally {
            if (contactCursor != null) {
                contactCursor.close();
            }
        }
        return false;
    }

    public static void removeSdpRecord(BluetoothAdapter mAdapter, int mSdpHandle,
        BluetoothPbapService service) {
        if (mAdapter != null && mSdpHandle >= 0 &&
                                SdpManager.getDefaultManager() != null) {
            Log.d(TAG, "Removing SDP record for PBAP with SDP handle: " +
                mSdpHandle);
            boolean status = SdpManager.getDefaultManager().removeSdpRecord(mSdpHandle);
            Log.d(TAG, "RemoveSDPrecord returns " + status);
            service.mSdpHandle = -1;
        }
    }

    public static String initCreateProfileVCard(String vcard, Context mContext,
            int vcardType, byte [] filter,final boolean vcardType21, boolean ignorefilter,
            VCardFilter vcardfilter) {
        vcard = BluetoothPbapUtils.createProfileVCard(mContext, vcardType, filter);
        if ((vcard != null) && !ignorefilter) {
            vcard = vcardfilter.apply(vcard, vcardType21);
        }
        return vcard;
    }

    public static void filterSearchedListByOffset(ArrayList<String> selectedNameList,
            ArrayList<Integer> savedPosList, int listStartOffset, int itemsFound,
            int requestSize, StringBuilder result, BluetoothPbapObexServer server) {
        for (int j = listStartOffset; j < selectedNameList.size() &&
            itemsFound < requestSize; j++) {
            itemsFound++;
            server.writeVCardEntry(savedPosList.get(j), selectedNameList.get(j),result);
        }

        selectedNameList.clear();
        savedPosList.clear();
    }

    public static void handleCleanup(BluetoothPbapService service, int shutdown) {
        Handler mSessionStatusHandler = service.getHandler();
        /* Stop PbapProfile if already started.*/
        if (!service.isPbapStarted()) {
            if (DEBUG) Log.d(TAG, "Service Not Available to STOP or Shutdown" +
                " already in progress - Ignoring");
            return;
        } else {
            if (VERBOSE) Log.v(TAG, "Service Stoping()");
        }
        /* SetState Disconnect already handled from closeService.
         * Handle it otherwise. Redundant for backup */
        if (service.getState() != BluetoothPbap.STATE_DISCONNECTED) {
            service.setState(BluetoothPbap.STATE_DISCONNECTED,
                BluetoothPbap.RESULT_CANCELED);
        }
        /* Cleanup already handled in Stop().
         * Move this  extra check to Handler. */
        if (mSessionStatusHandler != null) {
              Message msg = mSessionStatusHandler.obtainMessage(shutdown);
              mSessionStatusHandler.sendMessage(msg);
        }
        service.mStartError = true;
    }

    /* To check if device is unlocked in Direct Boot Mode */
    public static boolean isUserUnlocked(Context context) {
        UserManager manager = UserManager.get(context);
        return (manager == null || manager.isUserUnlocked());
    }

    /* To remove '-', '(', ')' or ' ' from TEL number. */
    public static String processTelNumberAndTag(String line) {
        if (VERBOSE) Log.v(TAG, "vCard line: " + line);
        String vTag = line.substring(0, line.lastIndexOf(":") + 1);
        String vTel = line.substring(line.lastIndexOf(":") + 1, line.length())
                              .replace("-", "")
                              .replace("(", "")
                              .replace(")", "")
                              .replace(" ", "");
        if (VERBOSE) Log.v(TAG, "vCard Tel Tag:" + vTag + ", Number:" + vTel);
        if (vTag.length() + vTel.length() < line.length())
            line = new StringBuilder().append(vTag).append(vTel).toString();
        return line;
    }

    /* To create spd record when pbap 1.2 support is not there and depending on
     * sim support value*/
    protected static void createSdpRecord(ObexServerSockets serverSockets,
            int supportedRepository, BluetoothPbapService service) {
        if (!isSupportedPbap12 && !isSimSupported) {
            service.mSdpHandle = SdpManager.getDefaultManager().createPbapPseRecord
                ("OBEX Phonebook Access Server",serverSockets.getRfcommChannel(),
                -1, BluetoothPbapFixes.SDP_PBAP_LEGACY_SERVER_VERSION,
                BluetoothPbapFixes.SDP_PBAP_LEGACY_SUPPORTED_REPOSITORIES,
                BluetoothPbapFixes.SDP_PBAP_LEGACY_SUPPORTED_FEATURES);
        } else if (!isSupportedPbap12 && isSimSupported) {
            service.mSdpHandle = SdpManager.getDefaultManager().createPbapPseRecord
                ("OBEX Phonebook Access Server",serverSockets.getRfcommChannel(),
                -1, BluetoothPbapFixes.SDP_PBAP_LEGACY_SERVER_VERSION,
                supportedRepository,
                BluetoothPbapFixes.SDP_PBAP_LEGACY_SUPPORTED_FEATURES);
        }
    }

    /* To close Handler thread with looper */
    protected static void closeHandler(BluetoothPbapService service) {
        Handler mSessionStatusHandler = service.getHandler();
        if (mSessionStatusHandler != null) {
            //Perform cleanup in Handler running on worker Thread
            mSessionStatusHandler.removeCallbacksAndMessages(null);
            Looper looper = mSessionStatusHandler.getLooper();
            if (looper != null) {
                looper.quit();
                Log.d(TAG, "Quit looper");
            }
            mSessionStatusHandler = null;
            Log.d(TAG, "Removed Handler..");
        }
    }

    protected static void updateMtu(ServerSession serverSession, boolean isSrmSupported,
            int rfcommMaxMTU) {
        String offloadSupported = SystemProperties.get("persist.vendor.bt.enable.splita2dp");
        if (DEBUG) Log.d(TAG, "offloadSupported :" + offloadSupported + " isSrmSupported :" +
                isSrmSupported + " isA2DPConnected :" + BTOppUtils.isA2DPConnected +
                " rfcommMaxMTU :" + rfcommMaxMTU);
        if (offloadSupported.isEmpty() || offloadSupported.equals("true")) {
            if (!isSrmSupported && BTOppUtils.isA2DPConnected && rfcommMaxMTU > 0) {
                serverSession.updateMTU(rfcommMaxMTU);
            }
        }
    }
}
