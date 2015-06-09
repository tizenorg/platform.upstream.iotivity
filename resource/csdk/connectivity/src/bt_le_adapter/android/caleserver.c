/******************************************************************
 *
 * Copyright 2014 Samsung Electronics All Rights Reserved.
 *
 *
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
 *
 ******************************************************************/

#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>
#include <unistd.h>
#include "caleserver.h"
#include "caleutils.h"
#include "caleinterface.h"
#include "caadapterutils.h"

#include "logger.h"
#include "oic_malloc.h"
#include "cathreadpool.h"
#include "camutex.h"
#include "uarraylist.h"
#include "org_iotivity_jar_caleserverinterface.h"

#define TAG PCF("CA_LE_SERVER")

static JavaVM *g_jvm = NULL;
static jobject g_context = NULL;
static jobject g_bluetoothGattServer = NULL;
static jobject g_bluetoothGattServerCallback = NULL;
static jobject g_leAdvertiseCallback = NULL;

static CAPacketReceiveCallback g_packetReceiveCallback = NULL;
static u_arraylist_t *g_connectedDeviceList = NULL;
static ca_thread_pool_t g_threadPoolHandle = NULL;

static bool g_isStartServer = false;
static bool g_isInitializedServer = false;

static CABLEServerDataReceivedCallback g_CABLEServerDataReceivedCallback = NULL;
static ca_mutex g_bleReqRespCbMutex = NULL;
static ca_mutex g_bleClientBDAddressMutex = NULL;
static ca_mutex g_connectedDeviceListMutex = NULL;

void CALEServerJNISetContext()
{
    OIC_LOG(DEBUG, TAG, "CALEServerJNISetContext");
    g_context = (jobject) CANativeJNIGetContext();
}

void CALeServerJniInit()
{
    OIC_LOG(DEBUG, TAG, "CALeServerJniInit");
    g_jvm = (JavaVM*) CANativeJNIGetJavaVM();
}

CAResult_t CALEServerCreateJniInterfaceObject()
{
    OIC_LOG(DEBUG, TAG, "CALEServerCreateJniInterfaceObject");

    if (!g_context)
    {
        OIC_LOG(ERROR, TAG, "g_context is null");
        return CA_STATUS_FAILED;
    }

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return CA_STATUS_FAILED;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(INFO, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return CA_STATUS_FAILED;
        }
        isAttached = true;
    }

    jclass jni_LEInterface = (*env)->FindClass(env, "org/iotivity/jar/caleserverinterface");
    if (!jni_LEInterface)
    {
        OIC_LOG(ERROR, TAG, "Could not get caleserverinterface class");
        goto exit;
    }

    jmethodID LeInterfaceConstructorMethod = (*env)->GetMethodID(env, jni_LEInterface, "<init>",
                                                                 "()V");
    if (!LeInterfaceConstructorMethod)
    {
        OIC_LOG(ERROR, TAG, "Could not get caleserverinterface constructor method");
        goto exit;
    }

    (*env)->NewObject(env, jni_LEInterface, LeInterfaceConstructorMethod, g_context);
    OIC_LOG(DEBUG, TAG, "Create instance for caleserverinterface");

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    return CA_STATUS_OK;

    exit:

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    return CA_STATUS_FAILED;
}

jobject CALEServerSetResponseData(JNIEnv *env, jbyteArray responseData)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerSetResponseData");
    VERIFY_NON_NULL_RET(env, TAG, "env is null", NULL);
    VERIFY_NON_NULL_RET(responseData, TAG, "responseData is null", NULL);

    if (!g_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "Check BluetoothGattServer status");
        return NULL;
    }

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return NULL;
    }

    OIC_LOG(DEBUG, TAG, "CALEServerSetResponseData");

    jclass jni_cid_bluetoothGattServer = (*env)->FindClass(env,
                                                           "android/bluetooth/BluetoothGattServer");
    if (!jni_cid_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattServer is null");
        return NULL;
    }

    jclass jni_cid_bluetoothGattService = (*env)->FindClass(env, "android/bluetooth/"
                                                            "BluetoothGattService");
    if (!jni_cid_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattService is null");
        return NULL;
    }

    jclass jni_cid_bluetoothGattCharacteristic = (*env)->FindClass(env, "android/bluetooth/"
                                                                   "BluetoothGattCharacteristic");
    if (!jni_cid_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattCharacteristic is null");
        return NULL;
    }

    jmethodID jni_mid_getService = (*env)->GetMethodID(env, jni_cid_bluetoothGattServer,
                                                       "getService",
                                                       "(Ljava/util/UUID;)Landroid/bluetooth/"
                                                       "BluetoothGattService;");
    if (!jni_cid_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_getService is null");
        return NULL;
    }

    jobject jni_obj_serviceUUID = CALEGetUuidFromString(env, OIC_GATT_SERVICE_UUID);
    if (!jni_obj_serviceUUID)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_serviceUUID is null");
        return NULL;
    }

    jobject jni_obj_bluetoothGattService = (*env)->CallObjectMethod(env, g_bluetoothGattServer,
                                                                    jni_mid_getService,
                                                                    jni_obj_serviceUUID);
    if (!jni_obj_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_bluetoothGattService is null");
        return NULL;
    }

    jmethodID jni_mid_getCharacteristic = (*env)->GetMethodID(env, jni_cid_bluetoothGattService,
                                                              "getCharacteristic",
                                                              "(Ljava/util/UUID;)"
                                                              "Landroid/bluetooth/"
                                                              "BluetoothGattCharacteristic;");
    if (!jni_mid_getCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_getCharacteristic is null");
        return NULL;
    }

    jobject jni_obj_responseUUID = CALEGetUuidFromString(env,
                                                         OIC_GATT_CHARACTERISTIC_RESPONSE_UUID);
    if (!jni_obj_responseUUID)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_responseUUID is null");
        return NULL;
    }

    jobject jni_obj_bluetoothGattCharacteristic = (*env)->CallObjectMethod(
            env, jni_obj_bluetoothGattService, jni_mid_getCharacteristic, jni_obj_responseUUID);
    if (!jni_obj_bluetoothGattCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_bluetoothGattCharacteristic is null");
        return NULL;
    }

    jmethodID jni_mid_setValue = (*env)->GetMethodID(env, jni_cid_bluetoothGattCharacteristic,
                                                     "setValue", "([B)Z");
    if (!jni_mid_setValue)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_setValue is null");
        return NULL;
    }

    jboolean jni_boolean_setValue = (*env)->CallBooleanMethod(env,
                                                              jni_obj_bluetoothGattCharacteristic,
                                                              jni_mid_setValue, responseData);
    if (JNI_FALSE == jni_boolean_setValue)
    {
        OIC_LOG(ERROR, TAG, "Fail to set response data");
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSetResponseData");
    return jni_obj_bluetoothGattCharacteristic;
}

CAResult_t CALEServerSendResponseData(JNIEnv *env, jobject device, jobject responseData)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerSendResponseData");
    VERIFY_NON_NULL(responseData, TAG, "responseData is null");
    VERIFY_NON_NULL(device, TAG, "device is null");
    VERIFY_NON_NULL(env, TAG, "env is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_bluetoothGattServer = (*env)->FindClass(env,
                                                           "android/bluetooth/BluetoothGattServer");
    if (!jni_cid_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_notifyCharacteristicChanged = (*env)->GetMethodID(
            env, jni_cid_bluetoothGattServer, "notifyCharacteristicChanged",
            "(Landroid/bluetooth/BluetoothDevice;"
            "Landroid/bluetooth/BluetoothGattCharacteristic;Z)Z");
    if (!jni_mid_notifyCharacteristicChanged)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_notifyCharacteristicChanged is null");
        return CA_STATUS_FAILED;
    }

    jboolean jni_boolean_notifyCharacteristicChanged = (*env)->CallBooleanMethod(
            env, g_bluetoothGattServer, jni_mid_notifyCharacteristicChanged, device, responseData,
            JNI_FALSE);
    if (JNI_FALSE == jni_boolean_notifyCharacteristicChanged)
    {
        OIC_LOG(ERROR, TAG, "Fail to notify characteristic");
        return CA_SEND_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSendResponseData");
    return CA_STATUS_OK;
}

CAResult_t CALEServerSendResponse(JNIEnv *env, jobject device, jint requestId, jint status,
                                        jint offset, jbyteArray value)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerSendResponse");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(device, TAG, "device is null");
    VERIFY_NON_NULL(value, TAG, "value is null");

    OIC_LOG(DEBUG, TAG, "CALEServerSendResponse");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_bluetoothGattServer = (*env)->FindClass(env,
                                                           "android/bluetooth/BluetoothGattServer");
    if (!jni_cid_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_sendResponse = (*env)->GetMethodID(env, jni_cid_bluetoothGattServer,
                                                         "sendResponse",
                                                         "(Landroid/bluetooth/BluetoothDevice;"
                                                         "III[B)Z");
    if (!jni_mid_sendResponse)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_sendResponse is null");
        return CA_STATUS_FAILED;
    }

    jboolean jni_boolean_sendResponse = (*env)->CallBooleanMethod(env, g_bluetoothGattServer,
                                                                  jni_mid_sendResponse, device,
                                                                  requestId, status, offset,
                                                                  value);
    if (JNI_FALSE == jni_boolean_sendResponse)
    {
        OIC_LOG(ERROR, TAG, "Fail to send response for gatt characteristic write request");
        return CA_SEND_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSendResponse");
    return CA_STATUS_OK;
}

CAResult_t CALEServerStartAdvertise(JNIEnv *env, jobject advertiseCallback)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerStartAdvertise");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(advertiseCallback, TAG, "advertiseCallback is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_AdvertiseSettings = (*env)->FindClass(env,
                                                         "android/bluetooth/le/"
                                                         "AdvertiseSettings$Builder");
    if (!jni_cid_AdvertiseSettings)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_AdvertiseSettings is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_AdvertiseSettings = (*env)->GetMethodID(env, jni_cid_AdvertiseSettings,
                                                              "<init>", "()V");
    if (!jni_mid_AdvertiseSettings)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_AdvertiseSettings is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_AdvertiseSettings = (*env)->NewObject(env, jni_cid_AdvertiseSettings,
                                                      jni_mid_AdvertiseSettings);
    if (!jni_AdvertiseSettings)
    {
        OIC_LOG(ERROR, TAG, "jni_AdvertiseSettings is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_setAdvertiseMode = (*env)->GetMethodID(env, jni_cid_AdvertiseSettings,
                                                             "setAdvertiseMode",
                                                             "(I)Landroid/bluetooth/le/"
                                                             "AdvertiseSettings$Builder;");
    if (!jni_mid_setAdvertiseMode)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_setAdvertiseMode is null");
        return CA_STATUS_FAILED;
    }

    // 0: Low power, 1: Balanced
    jobject jni_obj_setAdvertiseMode = (*env)->CallObjectMethod(env, jni_AdvertiseSettings,
                                                                jni_mid_setAdvertiseMode, 0);
    if (!jni_obj_setAdvertiseMode)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_setAdvertiseMode is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_setConnectable = (*env)->GetMethodID(env, jni_cid_AdvertiseSettings,
                                                           "setConnectable",
                                                           "(Z)Landroid/bluetooth/le/"
                                                           "AdvertiseSettings$Builder;");
    if (!jni_mid_setConnectable)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_setConnectable is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_setConnectable = (*env)->CallObjectMethod(env, jni_AdvertiseSettings,
                                                              jni_mid_setConnectable, JNI_TRUE);
    if (!jni_obj_setConnectable)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_setConnectable is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_setTimeout = (*env)->GetMethodID(env, jni_cid_AdvertiseSettings, "setTimeout",
                                                       "(I)Landroid/bluetooth/le/"
                                                       "AdvertiseSettings$Builder;");
    if (!jni_mid_setTimeout)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_setTimeout is null");
        return CA_STATUS_FAILED;
    }

    //A value of 0 will disable the time limit
    jobject jni_obj_setTimeout = (*env)->CallObjectMethod(env, jni_AdvertiseSettings,
                                                          jni_mid_setTimeout, 0);
    if (!jni_obj_setTimeout)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_setTimeout is null");
        return CA_STATUS_FAILED;
    }

    jclass jni_cid_AdvertiseDataBuilder = (*env)->FindClass(env,
                                                            "android/bluetooth/le/"
                                                            "AdvertiseData$Builder");
    if (!jni_cid_AdvertiseDataBuilder)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_AdvertiseDataBuilder is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_AdvertiseDataBuilder = (*env)->GetMethodID(env, jni_cid_AdvertiseDataBuilder,
                                                                 "<init>", "()V");
    if (!jni_mid_AdvertiseDataBuilder)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_AdvertiseDataBuilder is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_AdvertiseDataBuilder = (*env)->NewObject(env, jni_cid_AdvertiseDataBuilder,
                                                         jni_mid_AdvertiseDataBuilder);
    if (!jni_AdvertiseDataBuilder)
    {
        OIC_LOG(ERROR, TAG, "jni_AdvertiseDataBuilder is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_serviceUUID = CALEGetUuidFromString(env, OIC_GATT_SERVICE_UUID);
    if (!jni_obj_serviceUUID)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_serviceUUID is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_ParcelUuid = CALEGetParcelUuid(env, jni_obj_serviceUUID);
    if (!jni_ParcelUuid)
    {
        OIC_LOG(ERROR, TAG, "jni_ParcelUuid is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_addServiceUuid = (*env)->GetMethodID(env, jni_cid_AdvertiseDataBuilder,
                                                           "addServiceUuid",
                                                           "(Landroid/os/ParcelUuid;)Landroid/"
                                                           "bluetooth/le/AdvertiseData$Builder;");
    if (!jni_mid_addServiceUuid)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_addServiceUuid is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_addServiceUuid = (*env)->CallObjectMethod(env, jni_AdvertiseDataBuilder,
                                                              jni_mid_addServiceUuid,
                                                              jni_ParcelUuid);
    if (!jni_obj_addServiceUuid)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_addServiceUuid is null");
        return CA_STATUS_FAILED;
    }

    jclass jni_cid_BTAdapter = (*env)->FindClass(env, "android/bluetooth/BluetoothAdapter");
    if (!jni_cid_BTAdapter)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_BTAdapter is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_getDefaultAdapter = (*env)->GetStaticMethodID(env, jni_cid_BTAdapter,
                                                                    "getDefaultAdapter",
                                                                    "()Landroid/bluetooth/"
                                                                    "BluetoothAdapter;");
    if (!jni_mid_getDefaultAdapter)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_getDefaultAdapter is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_BTAdapter = (*env)->CallStaticObjectMethod(env, jni_cid_BTAdapter,
                                                               jni_mid_getDefaultAdapter);
    if (!jni_obj_BTAdapter)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_BTAdapter is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_getBluetoothLeAdvertiser = (*env)->GetMethodID(env, jni_cid_BTAdapter,
                                                                     "getBluetoothLeAdvertiser",
                                                                     "()Landroid/bluetooth/le/"
                                                                     "BluetoothLeAdvertiser;");
    if (!jni_mid_getBluetoothLeAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_getBluetoothLeAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_getBluetoothLeAdvertiser = (*env)->CallObjectMethod(
            env, jni_obj_BTAdapter, jni_mid_getBluetoothLeAdvertiser);
    if (!jni_obj_getBluetoothLeAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_getBluetoothLeAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_build_LeAdvertiseSettings = (*env)->GetMethodID(env,
                                                                      jni_cid_AdvertiseSettings,
                                                                      "build",
                                                                      "()Landroid/bluetooth/le/"
                                                                      "AdvertiseSettings;");
    if (!jni_mid_build_LeAdvertiseSettings)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_build_LeAdvertiseSettings is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_build_LeAdvertiseSettings = (*env)->CallObjectMethod(
            env, jni_AdvertiseSettings, jni_mid_build_LeAdvertiseSettings);
    if (!jni_obj_build_LeAdvertiseSettings)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_build_LeAdvertiseSettings is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_build_LeAdvertiseData = (*env)->GetMethodID(env, jni_cid_AdvertiseDataBuilder,
                                                                  "build",
                                                                  "()Landroid/bluetooth/le/"
                                                                  "AdvertiseData;");
    if (!jni_mid_build_LeAdvertiseData)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_build_LeAdvertiseData is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_build_LeAdvertiseData = (*env)->CallObjectMethod(env, jni_AdvertiseDataBuilder,
                                                                     jni_mid_build_LeAdvertiseData);
    if (!jni_obj_build_LeAdvertiseData)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_build_LeAdvertiseData is null");
        return CA_STATUS_FAILED;
    }

    jclass jni_cid_leAdvertiser = (*env)->FindClass(env,
                                                    "android/bluetooth/le/BluetoothLeAdvertiser");
    if (!jni_cid_leAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_leAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_startAdvertising = (*env)->GetMethodID(env, jni_cid_leAdvertiser,
                                                             "startAdvertising",
                                                             "(Landroid/bluetooth/le/"
                                                             "AdvertiseSettings;Landroid/bluetooth/"
                                                             "le/AdvertiseData;Landroid/bluetooth/"
                                                             "le/AdvertiseCallback;)V");
    if (!jni_mid_startAdvertising)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_startAdvertising is null");
        return CA_STATUS_FAILED;
    }

    (*env)->CallVoidMethod(env, jni_obj_getBluetoothLeAdvertiser, jni_mid_startAdvertising,
                           jni_obj_build_LeAdvertiseSettings, jni_obj_build_LeAdvertiseData,
                           advertiseCallback);

    if ((*env)->ExceptionCheck(env))
    {
        OIC_LOG(ERROR, TAG, "StartAdvertising has failed");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "Advertising started!!");

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerStartAdvertise");
    return CA_STATUS_OK;
}

CAResult_t CALEServerStopAdvertise(JNIEnv *env, jobject advertiseCallback)
{
    OIC_LOG(DEBUG, TAG, "IN - LEServerStopAdvertise");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(advertiseCallback, TAG, "advertiseCallback is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_BTAdapter = (*env)->FindClass(env, "android/bluetooth/BluetoothAdapter");
    if (!jni_cid_BTAdapter)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_BTAdapter is null");
        return CA_STATUS_FAILED;
    }

    jclass jni_cid_leAdvertiser = (*env)->FindClass(env,
                                                    "android/bluetooth/le/BluetoothLeAdvertiser");
    if (!jni_cid_leAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_leAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_getDefaultAdapter = (*env)->GetStaticMethodID(env, jni_cid_BTAdapter,
                                                                    "getDefaultAdapter",
                                                                    "()Landroid/bluetooth/"
                                                                    "BluetoothAdapter;");
    if (!jni_cid_leAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_leAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_getBTLeAdvertiser = (*env)->GetMethodID(env, jni_cid_BTAdapter,
                                                                     "getBluetoothLeAdvertiser",
                                                                     "()Landroid/bluetooth/le/"
                                                                     "BluetoothLeAdvertiser;");
    if (!jni_mid_getBTLeAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_getBTLeAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_stopAdvertising = (*env)->GetMethodID(env, jni_cid_leAdvertiser,
                                                            "stopAdvertising",
                                                            "(Landroid/bluetooth/le/"
                                                            "AdvertiseCallback;)V");
    if (!jni_mid_stopAdvertising)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_stopAdvertising is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_BTAdapter = (*env)->CallStaticObjectMethod(env, jni_cid_BTAdapter,
                                                               jni_mid_getDefaultAdapter);
    if (!jni_obj_BTAdapter)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_BTAdapter is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_getBluetoothLeAdvertiser = (*env)->CallObjectMethod(env, jni_obj_BTAdapter,
                                                                        jni_mid_getBTLeAdvertiser);
    if (!jni_obj_getBluetoothLeAdvertiser)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_getBluetoothLeAdvertiser is null");
        return CA_STATUS_FAILED;
    }

    (*env)->CallVoidMethod(env, jni_obj_getBluetoothLeAdvertiser, jni_mid_stopAdvertising,
                           advertiseCallback);
    if ((*env)->ExceptionCheck(env))
    {
        OIC_LOG(ERROR, TAG, "getBluetoothLeAdvertiser has failed");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "Advertising stopped!!");

    OIC_LOG(DEBUG, TAG, "OUT - LEServerStopAdvertise");
    return CA_STATUS_OK;
}

CAResult_t CALEServerStartGattServer(JNIEnv *env, jobject gattServerCallback)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerStartGattServer");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(gattServerCallback, TAG, "gattServerCallback is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    if (g_isStartServer)
    {
        OIC_LOG(DEBUG, TAG, "Gatt server already started");
    }

    g_bluetoothGattServerCallback = (*env)->NewGlobalRef(env, gattServerCallback);

    // open gatt server
    jobject bluetoothGattServer = CALEServerOpenGattServer(env);
    if (!bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    g_bluetoothGattServer = (*env)->NewGlobalRef(env, bluetoothGattServer);
    if (!g_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "g_bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    // create gatt service
    jobject bluetoothGattService = CALEServerCreateGattService(env);
    if (!bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "bluetoothGattService is null");
        return CA_STATUS_FAILED;
    }

    // add gatt service
    CAResult_t res = CALEServerAddGattService(env, g_bluetoothGattServer,
                                              bluetoothGattService);
    if (CA_STATUS_OK != res)
    {
        OIC_LOG(ERROR, TAG, "CALEServerAddGattService has failed");
    }
    return res;
}

jobject CALEServerOpenGattServer(JNIEnv *env)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerOpenGattServer");
    VERIFY_NON_NULL_RET(env, TAG, "env is null", NULL);

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return NULL;
    }

    jclass jni_cid_context = (*env)->FindClass(env, "android/content/Context");
    if (!jni_cid_context)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_context is null");
        return NULL;
    }

    jclass jni_cid_bluetoothManager = (*env)->FindClass(env, "android/bluetooth/BluetoothManager");
    if (!jni_cid_bluetoothManager)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothManager is null");
        return NULL;
    }

    jfieldID jni_fid_bluetoothService = (*env)->GetStaticFieldID(env, jni_cid_context,
                                                                 "BLUETOOTH_SERVICE",
                                                                 "Ljava/lang/String;");
    if (!jni_fid_bluetoothService)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_bluetoothService is null");
        return NULL;
    }

    jmethodID jni_mid_getSystemService = (*env)->GetMethodID(env, jni_cid_context,
                                                             "getSystemService",
                                                             "(Ljava/lang/String;)"
                                                             "Ljava/lang/Object;");
    if (!jni_mid_getSystemService)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_getSystemService is null");
        return NULL;
    }

    jmethodID jni_mid_openGattServer = (*env)->GetMethodID(env, jni_cid_bluetoothManager,
                                                           "openGattServer",
                                                           "(Landroid/content/Context;"
                                                           "Landroid/bluetooth/"
                                                           "BluetoothGattServerCallback;)"
                                                           "Landroid/bluetooth/"
                                                           "BluetoothGattServer;");
    if (!jni_mid_openGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_openGattServer is null");
        return NULL;
    }

    jobject jni_obj_bluetoothService = (*env)->GetStaticObjectField(env, jni_cid_context,
                                                                    jni_fid_bluetoothService);
    if (!jni_obj_bluetoothService)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_bluetoothService is null");
        return NULL;
    }

    jobject jni_obj_bluetoothManager = (*env)->CallObjectMethod(env, g_context,
                                                                jni_mid_getSystemService,
                                                                jni_obj_bluetoothService);
    if (!jni_obj_bluetoothManager)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_bluetoothManager is null");
        return NULL;
    }

    jobject jni_obj_bluetoothGattServer = (*env)->CallObjectMethod(env, jni_obj_bluetoothManager,
                                                                   jni_mid_openGattServer,
                                                                   g_context,
                                                                   g_bluetoothGattServerCallback);
    if (!jni_obj_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_bluetoothGattServer is null");
        return NULL;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerOpenGattServer");
    return jni_obj_bluetoothGattServer;
}

jobject CALEServerCreateGattService(JNIEnv *env)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerCreateGattService");
    VERIFY_NON_NULL_RET(env, TAG, "env is null", NULL);

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return NULL;
    }

    jclass jni_cid_bluetoothGattService = (*env)->FindClass(env, "android/bluetooth/"
                                                            "BluetoothGattService");
    if (!jni_cid_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattService is null");
        return NULL;
    }

    jclass jni_cid_bluetoothGattCharacteristic = (*env)->FindClass(env, "android/bluetooth/"
                                                                   "BluetoothGattCharacteristic");
    if (!jni_cid_bluetoothGattCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattCharacteristic is null");
        return NULL;
    }

    jfieldID jni_fid_serviceType = (*env)->GetStaticFieldID(env, jni_cid_bluetoothGattService,
                                                            "SERVICE_TYPE_PRIMARY", "I");
    if (!jni_fid_serviceType)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_serviceType is null");
        return NULL;
    }

    jfieldID jni_fid_readProperties = (*env)->GetStaticFieldID(env,
                                                               jni_cid_bluetoothGattCharacteristic,
                                                               "PROPERTY_READ", "I");
    if (!jni_fid_readProperties)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_readProperties is null");
        return NULL;
    }

#ifdef USE_PROPERTY_WRITE_RESPONSE
    jfieldID jni_fid_writeProperties = (*env)->GetStaticFieldID(env,
                                                                jni_cid_bluetoothGattCharacteristic,
                                                                "PROPERTY_WRITE", "I");
#else
    jfieldID jni_fid_writeProperties = (*env)->GetStaticFieldID(env,
                                                                jni_cid_bluetoothGattCharacteristic,
                                                                "PROPERTY_WRITE_NO_RESPONSE", "I");
#endif

    if (!jni_fid_writeProperties)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_writeProperties is null");
        return NULL;
    }

    jfieldID jni_fid_readPermissions = (*env)->GetStaticFieldID(env,
                                                                jni_cid_bluetoothGattCharacteristic,
                                                                "PERMISSION_READ", "I");
    if (!jni_fid_readPermissions)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_readPermissions is null");
        return NULL;
    }

    jfieldID jni_fid_writePermissions = (*env)->GetStaticFieldID(
            env, jni_cid_bluetoothGattCharacteristic, "PERMISSION_WRITE", "I");
    if (!jni_fid_writePermissions)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_writePermissions is null");
        return NULL;
    }

    jmethodID jni_mid_bluetoothGattService = (*env)->GetMethodID(env, jni_cid_bluetoothGattService,
                                                                 "<init>", "(Ljava/util/UUID;I)V");
    if (!jni_mid_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_bluetoothGattService is null");
        return NULL;
    }

    jmethodID jni_mid_addCharacteristic = (*env)->GetMethodID(env, jni_cid_bluetoothGattService,
                                                              "addCharacteristic",
                                                              "(Landroid/bluetooth/"
                                                              "BluetoothGattCharacteristic;)Z");
    if (!jni_mid_addCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_addCharacteristic is null");
        return NULL;
    }

    jmethodID jni_mid_bluetoothGattCharacteristic = (*env)->GetMethodID(
            env, jni_cid_bluetoothGattCharacteristic, "<init>", "(Ljava/util/UUID;II)V");
    if (!jni_mid_bluetoothGattCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_bluetoothGattCharacteristic is null");
        return NULL;
    }

    jobject jni_obj_serviceUUID = CALEGetUuidFromString(env, OIC_GATT_SERVICE_UUID);
    if (!jni_obj_serviceUUID)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_serviceUUID is null");
        return NULL;
    }

    jint jni_int_serviceType = (*env)->GetStaticIntField(env, jni_cid_bluetoothGattService,
                                                         jni_fid_serviceType);
    jobject jni_bluetoothGattService = (*env)->NewObject(env, jni_cid_bluetoothGattService,
                                                         jni_mid_bluetoothGattService,
                                                         jni_obj_serviceUUID, jni_int_serviceType);
    if (!jni_bluetoothGattService)
    {
        OIC_LOG(ERROR, TAG, "jni_bluetoothGattService is null");
        return NULL;
    }

    jobject jni_obj_readUuid = CALEGetUuidFromString(env, OIC_GATT_CHARACTERISTIC_RESPONSE_UUID);
    if (!jni_obj_readUuid)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_readUuid is null");
        return NULL;
    }

    jint jni_int_readProperties = (*env)->GetStaticIntField(env,
                                                            jni_cid_bluetoothGattCharacteristic,
                                                            jni_fid_readProperties);

    jint jni_int_readPermissions = (*env)->GetStaticIntField(env,
                                                             jni_cid_bluetoothGattCharacteristic,
                                                             jni_fid_readPermissions);

    jobject jni_readCharacteristic = (*env)->NewObject(env, jni_cid_bluetoothGattCharacteristic,
                                                       jni_mid_bluetoothGattCharacteristic,
                                                       jni_obj_readUuid, jni_int_readProperties,
                                                       jni_int_readPermissions);
    if (!jni_readCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_readCharacteristic is null");
        return NULL;
    }

    CAResult_t res = CALEServerAddDescriptor(env, jni_readCharacteristic);
    if (CA_STATUS_OK != res)
    {
        OIC_LOG(ERROR, TAG, "CALEServerAddDescriptor has failed");
        return NULL;
    }

    jboolean jni_boolean_addReadCharacteristic = (*env)->CallBooleanMethod(
            env, jni_bluetoothGattService, jni_mid_addCharacteristic, jni_readCharacteristic);
    if (!jni_boolean_addReadCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_boolean_addReadCharacteristic is null");
        return NULL;
    }

    jobject jni_obj_writeUuid = CALEGetUuidFromString(env, OIC_GATT_CHARACTERISTIC_REQUEST_UUID);
    if (!jni_obj_writeUuid)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_bluetoothGattServer is null");
        return NULL;
    }

    jint jni_int_writeProperties = (*env)->GetStaticIntField(env,
                                                             jni_cid_bluetoothGattCharacteristic,
                                                             jni_fid_writeProperties);

    jint jni_int_writePermissions = (*env)->GetStaticIntField(env,
                                                              jni_cid_bluetoothGattCharacteristic,
                                                              jni_fid_writePermissions);

    jobject jni_writeCharacteristic = (*env)->NewObject(env, jni_cid_bluetoothGattCharacteristic,
                                                        jni_mid_bluetoothGattCharacteristic,
                                                        jni_obj_writeUuid, jni_int_writeProperties,
                                                        jni_int_writePermissions);
    if (!jni_writeCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_writeCharacteristic is null");
        return NULL;
    }

    jboolean jni_boolean_addWriteCharacteristic = (*env)->CallBooleanMethod(
            env, jni_bluetoothGattService, jni_mid_addCharacteristic, jni_writeCharacteristic);
    if (JNI_FALSE == jni_boolean_addWriteCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "Fail to add jni_boolean_addReadCharacteristic");
        return NULL;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerCreateGattService");
    return jni_bluetoothGattService;
}

CAResult_t CALEServerAddDescriptor(JNIEnv *env, jobject characteristic)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerAddDescriptor");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(characteristic, TAG, "characteristic is null");

    jclass jni_cid_bluetoothGattDescriptor = (*env)->FindClass(env, "android/bluetooth/"
                                                               "BluetoothGattDescriptor");
    if (!jni_cid_bluetoothGattDescriptor)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattDescriptor is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_bluetoothGattDescriptor = (*env)->GetMethodID(env,
                                                                    jni_cid_bluetoothGattDescriptor,
                                                                    "<init>",
                                                                    "(Ljava/util/UUID;I)V");
    if (!jni_mid_bluetoothGattDescriptor)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_bluetoothGattDescriptor is null");
        return CA_STATUS_FAILED;
    }

    jfieldID jni_fid_readPermissions = (*env)->GetStaticFieldID(env,
                                                                jni_cid_bluetoothGattDescriptor,
                                                                "PERMISSION_READ", "I");
    if (!jni_fid_readPermissions)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_readPermissions is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_readUuid = CALEGetUuidFromString(env, OIC_GATT_CHARACTERISTIC_CONFIG_UUID);
    if (!jni_obj_readUuid)
    {
        OIC_LOG(ERROR, TAG, "jni_obj_readUuid is null");
        return CA_STATUS_FAILED;
    }

    jint jni_int_readPermissions = (*env)->GetStaticIntField(env, jni_cid_bluetoothGattDescriptor,
                                                             jni_fid_readPermissions);

    OIC_LOG(DEBUG, TAG, "initialize new Descriptor");

    jobject jni_readDescriptor = (*env)->NewObject(env, jni_cid_bluetoothGattDescriptor,
                                                   jni_mid_bluetoothGattDescriptor,
                                                   jni_obj_readUuid, jni_int_readPermissions);
    if (!jni_readDescriptor)
    {
        OIC_LOG(ERROR, TAG, "jni_readDescriptor is null");
        return CA_STATUS_FAILED;
    }

    jclass jni_cid_GattCharacteristic = (*env)->FindClass(env, "android/bluetooth/"
                                                          "BluetoothGattCharacteristic");
    if (!jni_cid_GattCharacteristic)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_GattCharacteristic is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_addDescriptor = (*env)->GetMethodID(env, jni_cid_GattCharacteristic,
                                                          "addDescriptor",
                                                          "(Landroid/bluetooth/"
                                                          "BluetoothGattDescriptor;)Z");
    if (!jni_mid_addDescriptor)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_addDescriptor is null");
        return CA_STATUS_FAILED;
    }

    jboolean jni_boolean_addDescriptor = (*env)->CallBooleanMethod(env, characteristic,
                                                                   jni_mid_addDescriptor,
                                                                   jni_readDescriptor);

    if (JNI_FALSE == jni_boolean_addDescriptor)
    {
        OIC_LOG(ERROR, TAG, "addDescriptor has failed");
        return CA_STATUS_FAILED;
    }
    else
    {
        OIC_LOG(DEBUG, TAG, "addDescriptor success");
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerAddDescriptor");
    return CA_STATUS_OK;
}

CAResult_t CALEServerAddGattService(JNIEnv *env, jobject bluetoothGattServer,
                                          jobject bluetoothGattService)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerAddGattService");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(bluetoothGattServer, TAG, "bluetoothGattServer is null");
    VERIFY_NON_NULL(bluetoothGattService, TAG, "bluetoothGattService is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_bluetoothGattServer = (*env)->FindClass(env,
                                                           "android/bluetooth/BluetoothGattServer");
    if (!jni_cid_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_addService = (*env)->GetMethodID(env, jni_cid_bluetoothGattServer,
                                                       "addService",
                                                       "(Landroid/bluetooth/BluetoothGattService;)"
                                                       "Z");
    if (!jni_mid_addService)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_addService is null");
        return CA_STATUS_FAILED;
    }

    jboolean jni_boolean_addService = (*env)->CallBooleanMethod(env, bluetoothGattServer,
                                                                jni_mid_addService,
                                                                bluetoothGattService);

    if (JNI_FALSE == jni_boolean_addService)
    {
        OIC_LOG(ERROR, TAG, "Fail to add GATT service");
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerAddGattService");
    return CA_STATUS_OK;
}

CAResult_t CALEServerConnect(JNIEnv *env, jobject bluetoothDevice)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerConnect");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(bluetoothDevice, TAG, "bluetoothDevice is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_bluetoothGattServer = (*env)->FindClass(env,
                                                           "android/bluetooth/BluetoothGattServer");
    if (!jni_cid_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_connect = (*env)->GetMethodID(env, jni_cid_bluetoothGattServer, "connect",
                                                    "(Landroid/bluetooth/BluetoothDevice;Z)Z");
    if (!jni_mid_connect)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_connect is null");
        return CA_STATUS_FAILED;
    }

    jboolean jni_boolean_connect = (*env)->CallBooleanMethod(env, g_bluetoothGattServer,
                                                             jni_mid_connect, bluetoothDevice,
                                                             JNI_FALSE);
    if (JNI_FALSE == jni_boolean_connect)
    {
        OIC_LOG(ERROR, TAG, "Fail to connect");
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerConnect");
    return CA_STATUS_OK;
}

CAResult_t CALEServerDisconnectAllDevices(JNIEnv *env)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerDisconnectAllDevices");
    VERIFY_NON_NULL(env, TAG, "env is null");

    ca_mutex_lock(g_connectedDeviceListMutex);
    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "g_connectedDeviceList is null");
        ca_mutex_unlock(g_connectedDeviceListMutex);
        return CA_STATUS_FAILED;
    }

    uint32_t length = u_arraylist_length(g_connectedDeviceList);
    for (uint32_t index = 0; index < length; index++)
    {
        jobject jarrayObj = (jobject) u_arraylist_get(g_connectedDeviceList, index);
        if (!jarrayObj)
        {
            OIC_LOG_V(ERROR, TAG, "object[%d] is null", index);
            continue;
        }

        // disconnect for device obj
        CAResult_t res = CALEServerDisconnect(env, jarrayObj);
        if (CA_STATUS_OK != res)
        {
            OIC_LOG_V(ERROR, TAG, "Disconnect for this device[%d] has failed", index);
            continue;
        }
    }

    ca_mutex_unlock(g_connectedDeviceListMutex);
    OIC_LOG(DEBUG, TAG, "OUT - CALEServerDisconnectAllDevices");
    return CA_STATUS_OK;
}

CAResult_t CALEServerDisconnect(JNIEnv *env, jobject bluetoothDevice)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerDisconnect");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(bluetoothDevice, TAG, "bluetoothDevice is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jclass jni_cid_bluetoothGattServer = (*env)->FindClass(env,
                                                           "android/bluetooth/BluetoothGattServer");
    if (!jni_cid_bluetoothGattServer)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothGattServer is null");
        return CA_STATUS_FAILED;
    }

    jmethodID jni_mid_cancelConnection = (*env)->GetMethodID(env, jni_cid_bluetoothGattServer,
                                                             "cancelConnection",
                                                             "(Landroid/bluetooth/BluetoothDevice;)"
                                                             "V");
    if (!jni_mid_cancelConnection)
    {
        OIC_LOG(ERROR, TAG, "jni_mid_cancelConnection is null");
        return CA_STATUS_FAILED;
    }

    (*env)->CallVoidMethod(env, g_bluetoothGattServer, jni_mid_cancelConnection, bluetoothDevice);

    if ((*env)->ExceptionCheck(env))
    {
        OIC_LOG(ERROR, TAG, "cancelConnection has failed");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerDisconnect");
    return CA_STATUS_OK;
}

CAResult_t CALEServerSend(JNIEnv *env, jobject bluetoothDevice, jbyteArray responseData)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerSend");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(bluetoothDevice, TAG, "bluetoothDevice is null");
    VERIFY_NON_NULL(responseData, TAG, "responseData is null");

    if (!CALEIsEnableBTAdapter(env))
    {
        OIC_LOG(ERROR, TAG, "BT adapter is not enabled");
        return CA_ADAPTER_NOT_ENABLED;
    }

    jobject responseChar = CALEServerSetResponseData(env, responseData);
    if (!responseChar)
    {
        OIC_LOG(ERROR, TAG, "responseChar is null");
        return CA_STATUS_FAILED;
    }

    CAResult_t result = CALEServerSendResponseData(env, bluetoothDevice, responseChar);
    if (CA_STATUS_OK != result)
    {
        OIC_LOG(ERROR, TAG, "Fail to send response data");
        return result;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSend");
    return result;
}

CAResult_t CALEServerInitialize(ca_thread_pool_t handle)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerInitialize");

    CALeServerJniInit();

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return CA_STATUS_FAILED;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(INFO, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return CA_STATUS_FAILED;
        }
        isAttached = true;
    }

    CAResult_t ret = CALECheckPlatformVersion(env, 21);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "it is not supported");

        if (isAttached)
        {
            (*g_jvm)->DetachCurrentThread(g_jvm);
        }
        return ret;
    }

    g_threadPoolHandle = handle;

    ret = CALEServerInitMutexVaraibles();
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerInitMutexVaraibles has failed");

        if (isAttached)
        {
            (*g_jvm)->DetachCurrentThread(g_jvm);
        }
        return CA_STATUS_FAILED;
    }

    CALEServerJNISetContext();
    CALEServerCreateCachedDeviceList();

    ret = CALEServerCreateJniInterfaceObject();
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerCreateJniInterfaceObject has failed");

        if (isAttached)
        {
            (*g_jvm)->DetachCurrentThread(g_jvm);
        }
        return CA_STATUS_FAILED;
    }

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    g_isInitializedServer = true;
    OIC_LOG(DEBUG, TAG, "OUT - CALEServerInitialize");
    return CA_STATUS_OK;
}

void CALEServerTerminate()
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerTerminate");

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(ERROR, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return;
        }
        isAttached = true;
    }

    CAResult_t ret = CALEServerStopMulticastServer(0);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerStopMulticastServer has failed");
    }

    ret = CALEServerDisconnectAllDevices(env);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerDisconnectAllDevices has failed");
    }

    ret = CALEServerRemoveAllDevices(env);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerRemoveAllDevices has failed");
    }

    if (g_leAdvertiseCallback)
    {
        (*env)->DeleteGlobalRef(env, g_leAdvertiseCallback);
    }

    if (g_bluetoothGattServer)
    {
        (*env)->DeleteGlobalRef(env, g_bluetoothGattServer);
    }

    if (g_bluetoothGattServerCallback)
    {
        (*env)->DeleteGlobalRef(env, g_bluetoothGattServerCallback);
    }

    CALEServerTerminateMutexVaraibles();
    CALEServerTerminateConditionVaraibles();

    g_isStartServer = false;
    g_isInitializedServer = false;

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerTerminate");
}

CAResult_t CALEServerSendUnicastMessage(const char* address, const char* data, uint32_t dataLen)
{
    OIC_LOG_V(DEBUG, TAG, "IN - CALEServerSendUnicastMessage(%s, %s)", address, data);
    VERIFY_NON_NULL(address, TAG, "address is null");
    VERIFY_NON_NULL(data, TAG, "data is null");

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return CA_STATUS_FAILED;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(ERROR, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return CA_STATUS_FAILED;
        }
        isAttached = true;
    }

    CAResult_t ret = CALEServerSendUnicastMessageImpl(env, address, data, dataLen);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerSendUnicastMessageImpl has failed");
    }

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSendUnicastMessage");
    return ret;
}

CAResult_t CALEServerSendMulticastMessage(const char* data, uint32_t dataLen)
{
    OIC_LOG_V(DEBUG, TAG, "IN - CALEServerSendMulticastMessage(%s)", data);
    VERIFY_NON_NULL(data, TAG, "data is null");

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return CA_STATUS_FAILED;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(ERROR, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return CA_STATUS_FAILED;
        }
        isAttached = true;
    }

    CAResult_t ret = CALEServerSendMulticastMessageImpl(env, data, dataLen);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerSendMulticastMessageImpl has failed");
    }

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSendMulticastMessage");
    return ret;
}

CAResult_t CALEServerStartMulticastServer()
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerStartMulticastServer");

    if (!g_isInitializedServer)
    {
        OIC_LOG(INFO, TAG, "server is not initialized");
        return CA_STATUS_FAILED;
    }

    if (g_isStartServer)
    {
        OIC_LOG(INFO, TAG, "server is already started..it will be skipped");
        return CA_STATUS_FAILED;
    }

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return CA_STATUS_FAILED;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(ERROR, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return CA_STATUS_FAILED;
        }
        isAttached = true;
    }

    g_isStartServer = true;

    // start gatt server
    CAResult_t ret = CALEServerStartGattServer(env, g_bluetoothGattServerCallback);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "Fail to start gatt server");
        return ret;
    }

    // start advertise
    ret = CALEServerStartAdvertise(env, g_leAdvertiseCallback);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerSendMulticastMessageImpl has failed");
    }

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerStartMulticastServer");
    return ret;
}

CAResult_t CALEServerStopMulticastServer()
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerStopMulticastServer");

    if (false == g_isStartServer)
    {
        OIC_LOG(INFO, TAG, "server is already stopped..it will be skipped");
        return CA_STATUS_FAILED;
    }

    if (!g_jvm)
    {
        OIC_LOG(ERROR, TAG, "g_jvm is null");
        return CA_STATUS_FAILED;
    }

    bool isAttached = false;
    JNIEnv* env;
    jint res = (*g_jvm)->GetEnv(g_jvm, (void**) &env, JNI_VERSION_1_6);
    if (JNI_OK != res)
    {
        OIC_LOG(ERROR, TAG, "Could not get JNIEnv pointer");
        res = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);

        if (JNI_OK != res)
        {
            OIC_LOG(ERROR, TAG, "AttachCurrentThread has failed");
            return CA_STATUS_FAILED;
        }
        isAttached = true;
    }

    CAResult_t ret = CALEServerStopAdvertise(env, g_leAdvertiseCallback);
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerSendMulticastMessageImpl has failed");
    }

    g_isStartServer = false;

    if (isAttached)
    {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerStopMulticastServer");
    return ret;
}

void CALEServerSetCallback(CAPacketReceiveCallback callback)
{
    OIC_LOG(DEBUG, TAG, "CALEServerSetCallback");
    g_packetReceiveCallback = callback;
}

CAResult_t CALEServerSendUnicastMessageImpl(JNIEnv *env, const char* address, const char* data,
                                            uint32_t dataLen)
{
    OIC_LOG_V(DEBUG, TAG, "IN - CALEServerSendUnicastMessageImpl, address: %s, data: %s",
            address, data);
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(address, TAG, "address is null");
    VERIFY_NON_NULL(data, TAG, "data is null");

    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "g_connectedDeviceList is null");
        return CA_STATUS_FAILED;
    }

    jobject jni_obj_bluetoothDevice = NULL;
    uint32_t length = u_arraylist_length(g_connectedDeviceList);
    for (uint32_t index = 0; index < length; index++)
    {
        OIC_LOG(DEBUG, TAG, "check device address");
        jobject jarrayObj = (jobject) u_arraylist_get(g_connectedDeviceList, index);
        if (!jarrayObj)
        {
            OIC_LOG(ERROR, TAG, "jarrayObj is null");
            return CA_STATUS_FAILED;
        }

        jstring jni_setAddress = CALEGetAddressFromBTDevice(env, jarrayObj);
        if (!jni_setAddress)
        {
            OIC_LOG(ERROR, TAG, "jni_setAddress is null");
            return CA_STATUS_FAILED;
        }
        const char* setAddress = (*env)->GetStringUTFChars(env, jni_setAddress, NULL);
        if (!setAddress)
        {
            OIC_LOG(ERROR, TAG, "setAddress is null");
            return CA_STATUS_FAILED;
        }

        OIC_LOG_V(DEBUG, TAG, "setAddress : %s", setAddress);
        OIC_LOG_V(DEBUG, TAG, "address : %s", address);

        if (!strcmp(setAddress, address))
        {
            OIC_LOG(DEBUG, TAG, "found the device");
            jni_obj_bluetoothDevice = jarrayObj;
            (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
            break;
        }
        (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
    }

    if (jni_obj_bluetoothDevice)
    {
        jbyteArray jni_bytearr_data = (*env)->NewByteArray(env, dataLen);
        (*env)->SetByteArrayRegion(env, jni_bytearr_data, 0, dataLen, (jbyte*) data);

        CAResult_t res = CALEServerSend(env, jni_obj_bluetoothDevice, jni_bytearr_data);
        if (CA_STATUS_OK != res)
        {
            OIC_LOG(ERROR, TAG, "send has failed");
            return CA_SEND_FAILED;
        }
    }
    else
    {
        OIC_LOG(ERROR, TAG, "There are no device to send in the list");
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSendUnicastMessageImpl");
    return CA_STATUS_OK;
}

CAResult_t CALEServerSendMulticastMessageImpl(JNIEnv *env, const char *data, uint32_t dataLen)
{
    OIC_LOG_V(DEBUG, TAG, "IN - CALEServerSendMulticastMessageImpl, send to, data: %s", data);
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(data, TAG, "data is null");

    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "g_connectedDeviceList is null");
        return CA_STATUS_FAILED;
    }

    uint32_t length = u_arraylist_length(g_connectedDeviceList);
    for (uint32_t index = 0; index < length; index++)
    {
        jobject jarrayObj = (jobject) u_arraylist_get(g_connectedDeviceList, index);
        if (!jarrayObj)
        {
            OIC_LOG(ERROR, TAG, "jarrayObj is null");
            return CA_STATUS_FAILED;
        }

        // send data for all device
        jbyteArray jni_bytearr_data = (*env)->NewByteArray(env, dataLen);
        (*env)->SetByteArrayRegion(env, jni_bytearr_data, 0, dataLen, (jbyte*) data);
        CAResult_t res = CALEServerSend(env, jarrayObj, jni_bytearr_data);
        if (CA_STATUS_OK != res)
        {
            OIC_LOG(ERROR, TAG, "send has failed");
            return CA_SEND_FAILED;
        }
    }

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerSendMulticastMessageImpl");
    return CA_STATUS_OK;
}

void CALEServerCreateCachedDeviceList()
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerCreateCachedDeviceList");

    ca_mutex_lock(g_connectedDeviceListMutex);
    // create new object array
    if (!g_connectedDeviceList)
    {
        OIC_LOG(DEBUG, TAG, "Create device list");
        g_connectedDeviceList = u_arraylist_create();
    }
    ca_mutex_unlock(g_connectedDeviceListMutex);

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerCreateCachedDeviceList");
}

bool CALEServerIsDeviceInList(JNIEnv *env, const char* remoteAddress)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerIsDeviceInList");
    VERIFY_NON_NULL_RET(env, TAG, "env is null", false);
    VERIFY_NON_NULL_RET(remoteAddress, TAG, "remoteAddress is null", false);

    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "list is null");
        return false;
    }

    uint32_t length = u_arraylist_length(g_connectedDeviceList);
    for (uint32_t index = 0; index < length; index++)
    {
        jobject jarrayObj = (jobject) u_arraylist_get(g_connectedDeviceList, index);

        if (!jarrayObj)
        {
            OIC_LOG(ERROR, TAG, "jarrayObj is null");
            return false;
        }

        jstring jni_setAddress = CALEGetAddressFromBTDevice(env, jarrayObj);
        if (!jni_setAddress)
        {
            OIC_LOG(ERROR, TAG, "jni_setAddress is null");
            return false;
        }

        const char* setAddress = (*env)->GetStringUTFChars(env, jni_setAddress, NULL);
        if (!setAddress)
        {
            OIC_LOG(ERROR, TAG, "setAddress is null");
            return false;
        }

        if (!strcmp(remoteAddress, setAddress))
        {
            OIC_LOG(ERROR, TAG, "the device is already set");
            (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
            return true;
        }
        else
        {
            (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
            continue;
        }
    }

    OIC_LOG(DEBUG, TAG, "there are no device in the list");
    OIC_LOG(DEBUG, TAG, "OUT - CALEServerCreateCachedDeviceList");
    return false;
}

CAResult_t CALEServerAddDeviceToList(JNIEnv *env, jobject device)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerAddDeviceToList");
    VERIFY_NON_NULL(device, TAG, "device is null");
    VERIFY_NON_NULL(env, TAG, "env is null");

    ca_mutex_lock(g_connectedDeviceListMutex);

    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "list is null");
        ca_mutex_unlock(g_connectedDeviceListMutex);
        return CA_STATUS_FAILED;
    }

    jstring jni_remoteAddress = CALEGetAddressFromBTDevice(env, device);
    if (!jni_remoteAddress)
    {
        OIC_LOG(ERROR, TAG, "jni_remoteAddress is null");
        ca_mutex_unlock(g_connectedDeviceListMutex);
        return CA_STATUS_FAILED;
    }

    const char* remoteAddress = (*env)->GetStringUTFChars(env, jni_remoteAddress, NULL);
    if (!remoteAddress)
    {
        OIC_LOG(ERROR, TAG, "remoteAddress is null");
        ca_mutex_unlock(g_connectedDeviceListMutex);
        return CA_STATUS_FAILED;
    }

    if (false == CALEServerIsDeviceInList(env, remoteAddress))
    {
        jobject jni_obj_device = (*env)->NewGlobalRef(env, device);
        u_arraylist_add(g_connectedDeviceList, jni_obj_device);
        OIC_LOG_V(DEBUG, TAG, "Set the object to ArrayList as Element : %s", remoteAddress);
    }

    (*env)->ReleaseStringUTFChars(env, jni_remoteAddress, remoteAddress);
    ca_mutex_unlock(g_connectedDeviceListMutex);
    OIC_LOG(DEBUG, TAG, "OUT - CALEServerAddDeviceToList");
    return CA_STATUS_OK;
}

CAResult_t CALEServerRemoveAllDevices(JNIEnv *env)
{
    OIC_LOG(DEBUG, TAG, "IN - CALEServerRemoveAllDevices");
    VERIFY_NON_NULL(env, TAG, "env is null");

    ca_mutex_lock(g_connectedDeviceListMutex);
    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "g_connectedDeviceList is null");
        ca_mutex_unlock(g_connectedDeviceListMutex);
        return CA_STATUS_FAILED;
    }

    uint32_t length = u_arraylist_length(g_connectedDeviceList);
    for (uint32_t index = 0; index < length; index++)
    {
        jobject jarrayObj = (jobject) u_arraylist_get(g_connectedDeviceList, index);
        if (jarrayObj)
        {
            (*env)->DeleteGlobalRef(env, jarrayObj);
        }
    }

    OICFree(g_connectedDeviceList);
    g_connectedDeviceList = NULL;
    ca_mutex_unlock(g_connectedDeviceListMutex);

    OIC_LOG(DEBUG, TAG, "OUT - CALEServerRemoveAllDevices");
    return CA_STATUS_OK;
}

CAResult_t CALEServerRemoveDevice(JNIEnv *env, jstring address)
{
    OIC_LOG(DEBUG, TAG, "IN CALEServerRemoveDevice");
    VERIFY_NON_NULL(env, TAG, "env is null");
    VERIFY_NON_NULL(address, TAG, "address is null");

    ca_mutex_lock(g_connectedDeviceListMutex);
    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "no deviceList");
        ca_mutex_unlock(g_connectedDeviceListMutex);
        return CA_STATUS_FAILED;
    }

    uint32_t length = u_arraylist_length(g_connectedDeviceList);
    for (uint32_t index = 0; index < length; index++)
    {
        jobject jarrayObj = (jobject) u_arraylist_get(g_connectedDeviceList, index);

        if (jarrayObj)
        {
            jstring jni_setAddress = CALEGetAddressFromBTDevice(env, jarrayObj);
            if (!jni_setAddress)
            {
                OIC_LOG(ERROR, TAG, "wrong device address");
                continue;
            }
            const char* setAddress = (*env)->GetStringUTFChars(env, jni_setAddress, NULL);
            if (!setAddress)
            {
                OIC_LOG(ERROR, TAG, "setAddress is null");
                continue;
            }

            const char* remoteAddress = (*env)->GetStringUTFChars(env, address, NULL);
            if (!remoteAddress)
            {
                OIC_LOG(ERROR, TAG, "remoteAddress is null");
                (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
                continue;
            }

            if (!strcmp(setAddress, remoteAddress))
            {
                OIC_LOG_V(DEBUG, TAG, "device address : %s", remoteAddress);

                (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
                (*env)->ReleaseStringUTFChars(env, address, remoteAddress);
                (*env)->DeleteGlobalRef(env, jarrayObj);

                CAResult_t res = CALEServerReorderinglist(index);
                if (CA_STATUS_OK != res)
                {
                    OIC_LOG(ERROR, TAG, "CALEServerReorderinglist has failed");
                    ca_mutex_unlock(g_connectedDeviceListMutex);
                    return res;
                }
                ca_mutex_unlock(g_connectedDeviceListMutex);
                return CA_STATUS_OK;
            }
            (*env)->ReleaseStringUTFChars(env, jni_setAddress, setAddress);
            (*env)->ReleaseStringUTFChars(env, address, remoteAddress);
        }
    }

    ca_mutex_unlock(g_connectedDeviceListMutex);

    OIC_LOG(DEBUG, TAG, "there are no device in the device list");

    OIC_LOG(DEBUG, TAG, "IN CALEServerRemoveDevice");
    return CA_STATUS_FAILED;
}

CAResult_t CALEServerReorderinglist(uint32_t index)
{
    if (!g_connectedDeviceList)
    {
        OIC_LOG(ERROR, TAG, "g_connectedDeviceList is null");
        return CA_STATUS_FAILED;
    }

    if (index >= g_connectedDeviceList->length)
    {
        OIC_LOG(ERROR, TAG, "index is not available");
        return CA_STATUS_FAILED;
    }

    if (index < g_connectedDeviceList->length - 1)
    {
        memmove(&g_connectedDeviceList->data[index], &g_connectedDeviceList->data[index + 1],
                (g_connectedDeviceList->length - index - 1) * sizeof(void *));
    }

    g_connectedDeviceList->size--;
    g_connectedDeviceList->length--;

    return CA_STATUS_OK;
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CARegisterLeGattServerCallback(JNIEnv *env, jobject obj,
                                                                         jobject callback)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - Register Le Gatt Server Callback");
    VERIFY_NON_NULL_VOID(env, TAG, "env is null");
    VERIFY_NON_NULL_VOID(callback, TAG, "callback is null");

    g_bluetoothGattServerCallback = (*env)->NewGlobalRef(env, callback);
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CARegisterBluetoothLeAdvertiseCallback(JNIEnv *env,
                                                                                 jobject obj,
                                                                                 jobject callback)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - Register Le Advertise Callback");
    VERIFY_NON_NULL_VOID(env, TAG, "env is null");
    VERIFY_NON_NULL_VOID(callback, TAG, "callback is null");

    g_leAdvertiseCallback = (*env)->NewGlobalRef(env, callback);
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerConnectionStateChangeCallback(
        JNIEnv *env, jobject obj, jobject device, jint status, jint newState)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - Gatt Server ConnectionStateChange Callback");
    OIC_LOG_V(DEBUG, TAG, "New connection State: %d", newState);

    VERIFY_NON_NULL_VOID(env, TAG, "env is null");
    VERIFY_NON_NULL_VOID(device, TAG, "device is null");

    jclass jni_cid_bluetoothProfile = (*env)->FindClass(env, "android/bluetooth/BluetoothProfile");
    if (!jni_cid_bluetoothProfile)
    {
        OIC_LOG(ERROR, TAG, "jni_cid_bluetoothProfile is null");
        return;
    }

    jfieldID jni_fid_state_connected = (*env)->GetStaticFieldID(env, jni_cid_bluetoothProfile,
                                                                "STATE_CONNECTED", "I");
    if(!jni_fid_state_connected)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_state_connected is null");
        return;
    }

    jfieldID jni_fid_state_disconnected = (*env)->GetStaticFieldID(env, jni_cid_bluetoothProfile,
                                                                   "STATE_DISCONNECTED", "I");
    if(!jni_fid_state_disconnected)
    {
        OIC_LOG(ERROR, TAG, "jni_fid_state_disconnected is null");
        return;
    }

    // STATE_CONNECTED
    jint jni_int_state_connected = (*env)->GetStaticIntField(env, jni_cid_bluetoothProfile,
                                                             jni_fid_state_connected);

    // STATE_DISCONNECTED
    jint jni_int_state_disconnected = (*env)->GetStaticIntField(env, jni_cid_bluetoothProfile,
                                                                jni_fid_state_disconnected);

    if (newState == jni_int_state_connected)
    {

        OIC_LOG(DEBUG, TAG, "LE CONNECTED");

        jstring jni_remoteAddress = CALEGetAddressFromBTDevice(env, device);
        if (!jni_remoteAddress)
        {
            OIC_LOG(ERROR, TAG, "jni_remoteAddress is null");
            return;
        }

        const char* remoteAddress = (*env)->GetStringUTFChars(env, jni_remoteAddress, NULL);
        if (!remoteAddress)
        {
            OIC_LOG(ERROR, TAG, "remoteAddress is null");
            return;
        }

        if (false == CALEServerIsDeviceInList(env, remoteAddress))
        {
            OIC_LOG(DEBUG, TAG, "add connected device to cache");
            CALEServerAddDeviceToList(env, device);
        }
        (*env)->ReleaseStringUTFChars(env, jni_remoteAddress, remoteAddress);
    }
    else if (newState == jni_int_state_disconnected)
    {
        OIC_LOG(DEBUG, TAG, "LE DISCONNECTED");
    }
    else
    {
        OIC_LOG_V(DEBUG, TAG, "LE Connection state is [newState : %d, status %d]", newState,
                status);
    }
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerServiceAddedCallback(JNIEnv *env,
                                                                             jobject obj,
                                                                             jint status,
                                                                             jobject gattService)
{
    OIC_LOG_V(DEBUG, TAG, "caleserverinterface - Gatt Service Added Callback(%d)", status);
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerCharacteristicReadRequestCallback(
        JNIEnv *env, jobject obj, jobject device, jint requestId, jint offset,
        jobject characteristic, jbyteArray data)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - Gatt Server Characteristic Read Request Callback");
    VERIFY_NON_NULL_VOID(env, TAG, "env is null");
    VERIFY_NON_NULL_VOID(device, TAG, "device is null");

#ifdef USE_PROPERTY_WRITE_RESPONSE
    CALEServerSendResponse(env, device, requestId, 0, offset, NULL);
#endif

}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerCharacteristicWriteRequestCallback(
        JNIEnv *env, jobject obj, jobject device, jint requestId, jobject characteristic,
        jbyteArray data, jboolean preparedWrite, jboolean responseNeeded, jint offset,
        jbyteArray value)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - Gatt Server Characteristic Write Request Callback");
    VERIFY_NON_NULL_VOID(env, TAG, "env is null");
    VERIFY_NON_NULL_VOID(device, TAG, "device is null");
    VERIFY_NON_NULL_VOID(value, TAG, "value is null");
    VERIFY_NON_NULL_VOID(data, TAG, "data is null");

#ifdef USE_PROPERTY_WRITE_RESPONSE
    CALEServerSendResponse(env, device, requestId, 0, offset, value);
#endif

    // get Byte Array and covert to char*
    jint length = (*env)->GetArrayLength(env, data);

    jboolean isCopy;
    jbyte *jni_byte_requestData = (jbyte *) (*env)->GetByteArrayElements(env, data, &isCopy);

    char* requestData = NULL;
    requestData = (char*) OICMalloc(sizeof(char) * length + 1);
    if (!requestData)
    {
        OIC_LOG(ERROR, TAG, "requestData is null");
        return;
    }

    memcpy(requestData, (const char*) jni_byte_requestData, length);
    requestData[length] = '\0';
    (*env)->ReleaseByteArrayElements(env, data, jni_byte_requestData, JNI_ABORT);

    jstring jni_address = CALEGetAddressFromBTDevice(env, device);
    if (!jni_address)
    {
        OIC_LOG(ERROR, TAG, "jni_address is null");
        OICFree(requestData);
        return;
    }

    const char* address = (*env)->GetStringUTFChars(env, jni_address, NULL);
    if (!address)
    {
        OIC_LOG(ERROR, TAG, "address is null");
        OICFree(requestData);
        return;
    }

    OIC_LOG_V(DEBUG, TAG, "remote device address : %s, %s, %d", address, requestData, length);

    ca_mutex_lock(g_bleClientBDAddressMutex);
    uint32_t sentLength = 0;
    g_CABLEServerDataReceivedCallback(address, OIC_GATT_SERVICE_UUID, requestData, length,
                                      &sentLength);
    ca_mutex_unlock(g_bleClientBDAddressMutex);

    (*env)->ReleaseStringUTFChars(env, jni_address, address);
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerDescriptorReadRequestCallback(
        JNIEnv *env, jobject obj, jobject device, jint requestId, jint offset, jobject descriptor)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface_CALeGattServerDescriptorReadRequestCallback");
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerDescriptorWriteRequestCallback(
        JNIEnv *env, jobject obj, jobject device, jint requestId, jobject descriptor,
        jboolean preparedWrite, jboolean responseNeeded, jint offset, jbyteArray value)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface_CALeGattServerDescriptorWriteRequestCallback");
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerExecuteWriteCallback(JNIEnv *env,
                                                                             jobject obj,
                                                                             jobject device,
                                                                             jint requestId,
                                                                             jboolean execute)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface_CALeGattServerExecuteWriteCallback");
    VERIFY_NON_NULL_VOID(env, TAG, "env is null");
    VERIFY_NON_NULL_VOID(device, TAG, "device is null");

//    CALEServerSendResponse(env, device, requestId, 0, 0, NULL);
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeGattServerNotificationSentCallback(JNIEnv *env,
                                                                                 jobject obj,
                                                                                 jobject device,
                                                                                 jint status)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - Gatt Server Notification Sent Callback");
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeAdvertiseStartSuccessCallback(
        JNIEnv *env, jobject obj, jobject settingsInEffect)
{
    OIC_LOG(DEBUG, TAG, "caleserverinterface - LE Advertise Start Success Callback");
}

JNIEXPORT void JNICALL
Java_org_iotivity_jar_caleserverinterface_CALeAdvertiseStartFailureCallback(JNIEnv *env,
                                                                            jobject obj,
                                                                            jint errorCode)
{
    OIC_LOG_V(ERROR, TAG, "caleserverinterface - LE Advertise Start Failure Callback(%)",
              errorCode);
}

/**
 * adapter common
 */

CAResult_t CAStartBleGattServer()
{
    OIC_LOG(DEBUG, TAG, "IN");

    CAResult_t ret = CALEServerInitMutexVaraibles();
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerInitMutexVaraibles has failed!");
        CALEServerTerminateMutexVaraibles();
        return CA_SERVER_NOT_STARTED;
    }

    ret = CALEServerInitConditionVaraibles();
    if (CA_STATUS_OK != ret)
    {
        OIC_LOG(ERROR, TAG, "CALEServerInitConditionVaraibles has failed!");
        CALEServerTerminateConditionVaraibles();
        return CA_SERVER_NOT_STARTED;
    }

    // start gatt service
    CALEServerStartMulticastServer();

    OIC_LOG(DEBUG, TAG, "OUT");
    return CA_STATUS_OK;
}

CAResult_t CAStopBleGattServer()
{
    OIC_LOG(DEBUG, TAG, "IN");

    OIC_LOG(DEBUG, TAG, "OUT");
    return CA_STATUS_OK;
}

void CATerminateBleGattServer()
{
    OIC_LOG(DEBUG, TAG, "IN");

    OIC_LOG(DEBUG, TAG, "Terminat Gatt Server");
    CALEServerTerminate();

    OIC_LOG(DEBUG, TAG, "OUT");
}

void CASetBLEReqRespServerCallback(CABLEServerDataReceivedCallback callback)
{
    OIC_LOG(DEBUG, TAG, "IN");

    ca_mutex_lock(g_bleReqRespCbMutex);
    g_CABLEServerDataReceivedCallback = callback;
    ca_mutex_unlock(g_bleReqRespCbMutex);

    OIC_LOG(DEBUG, TAG, "OUT");
}

CAResult_t CAUpdateCharacteristicsToGattClient(const char* address, const char *charValue,
                                               const uint32_t charValueLen)
{
    OIC_LOG(DEBUG, TAG, "IN");
    VERIFY_NON_NULL(address, TAG, "env is null");
    VERIFY_NON_NULL(charValue, TAG, "device is null");

    if (address)
    {
        OIC_LOG(DEBUG, TAG, "CALEServerSendUnicastData");
        CALEServerSendUnicastMessage(address, charValue, charValueLen);
    }

    OIC_LOG(DEBUG, TAG, "OUT");

    return CA_STATUS_OK;
}

CAResult_t CAUpdateCharacteristicsToAllGattClients(const char *charValue,
                                                   const uint32_t charValueLen)
{
    OIC_LOG(DEBUG, TAG, "IN");
    VERIFY_NON_NULL(charValue, TAG, "device is null");

    OIC_LOG(DEBUG, TAG, "CALEServerSendMulticastMessage");
    CALEServerSendMulticastMessage(charValue, charValueLen);

    OIC_LOG(DEBUG, TAG, "OUT");
    return CA_STATUS_OK;
}

void CASetBleServerThreadPoolHandle(ca_thread_pool_t handle)
{
    OIC_LOG(DEBUG, TAG, "IN");

    CALEServerInitialize(handle);

    OIC_LOG(DEBUG, TAG, "OUT");
}

CAResult_t CALEServerInitMutexVaraibles()
{
    OIC_LOG(DEBUG, TAG, "IN");
    if (NULL == g_bleReqRespCbMutex)
    {
        g_bleReqRespCbMutex = ca_mutex_new();
        if (NULL == g_bleReqRespCbMutex)
        {
            OIC_LOG(ERROR, TAG, "ca_mutex_new has failed");
            return CA_STATUS_FAILED;
        }
    }

    if (NULL == g_bleClientBDAddressMutex)
    {
        g_bleClientBDAddressMutex = ca_mutex_new();
        if (NULL == g_bleClientBDAddressMutex)
        {
            OIC_LOG(ERROR, TAG, "ca_mutex_new has failed");
            return CA_STATUS_FAILED;
        }
    }

    if (NULL == g_connectedDeviceListMutex)
    {
        g_connectedDeviceListMutex = ca_mutex_new();
        if (NULL == g_connectedDeviceListMutex)
        {
            OIC_LOG(ERROR, TAG, "ca_mutex_new has failed");
            return CA_STATUS_FAILED;
        }
    }

    OIC_LOG(DEBUG, TAG, "OUT");
    return CA_STATUS_OK;
}

CAResult_t CALEServerInitConditionVaraibles()
{
    OIC_LOG(DEBUG, TAG, "this method is not supported");
    return CA_STATUS_OK;
}

void CALEServerTerminateMutexVaraibles()
{
    OIC_LOG(DEBUG, TAG, "IN");

    ca_mutex_free(g_bleReqRespCbMutex);
    g_bleReqRespCbMutex = NULL;

    ca_mutex_free(g_bleClientBDAddressMutex);
    g_bleClientBDAddressMutex = NULL;

    ca_mutex_free(g_connectedDeviceListMutex);
    g_connectedDeviceListMutex = NULL;

    OIC_LOG(DEBUG, TAG, "OUT");
}

void CALEServerTerminateConditionVaraibles()
{
    OIC_LOG(DEBUG, TAG, "this method is not supported");
}
