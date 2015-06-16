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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cainterface.h"
#include "caremotehandler.h"
#include "camessagehandler.h"
#include "caprotocolmessage.h"
#include "canetworkconfigurator.h"
#include "cainterfacecontroller.h"
#include "logger.h"
#ifdef __WITH_DTLS__
#include "caadapternetdtls.h"
#endif

CAGlobals_t caglobals;

#define TAG PCF("CA")

static bool g_isInitialized = false;

#ifdef __WITH_DTLS__
// CAAdapterNetDTLS will register the callback.
// Taking callback all the way through adapters not the right approach, hence calling here.
extern void CADTLSSetCredentialsCallback(CAGetDTLSCredentialsHandler credCallback);
#endif

CAResult_t CAInitialize()
{
    OIC_LOG(DEBUG, TAG, "CAInitialize");

    if (!g_isInitialized)
    {
        CAResult_t res = CAInitializeMessageHandler();
        if (res != CA_STATUS_OK)
        {
            OIC_LOG(ERROR, TAG, "CAInitialize has failed");
            return res;
        }
        g_isInitialized = true;
    }
    return CA_STATUS_OK;
}

void CATerminate()
{
    OIC_LOG(DEBUG, TAG, "CATerminate");

    if (g_isInitialized)
    {
        CATerminateMessageHandler();
        CATerminateNetworkType();

        g_isInitialized = false;
    }
}

CAResult_t CAStartListeningServer()
{
    OIC_LOG(DEBUG, TAG, "CAStartListeningServer");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    return CAStartListeningServerAdapters();
}

CAResult_t CAStartDiscoveryServer()
{
    OIC_LOG(DEBUG, TAG, "CAStartDiscoveryServer");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    return CAStartDiscoveryServerAdapters();
}

void CARegisterHandler(CARequestCallback ReqHandler, CAResponseCallback RespHandler,
                       CAErrorCallback ErrorHandler)
{
    OIC_LOG(DEBUG, TAG, "CARegisterHandler");

    if(!g_isInitialized)
    {
        OIC_LOG(DEBUG, TAG, "CA is not initialized");
        return;
    }

    CASetInterfaceCallbacks(ReqHandler, RespHandler, ErrorHandler);
}

#ifdef __WITH_DTLS__
CAResult_t CARegisterDTLSCredentialsHandler(CAGetDTLSCredentialsHandler GetDTLSCredentialsHandler)
{
    OIC_LOG(DEBUG, TAG, "CARegisterDTLSCredentialsHandler");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    CADTLSSetCredentialsCallback(GetDTLSCredentialsHandler);
    return CA_STATUS_OK;
}
#endif //__WITH_DTLS__

void CADestroyEndpoint(CAEndpoint_t *rep)
{
    OIC_LOG(DEBUG, TAG, "CADestroyEndpoint");

    CADestroyEndpointInternal(rep);
}

CAResult_t CAGenerateToken(CAToken_t *token, uint8_t tokenLength)
{
    OIC_LOG(DEBUG, TAG, "CAGenerateToken");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }
    return CAGenerateTokenInternal(token, tokenLength);
}

void CADestroyToken(CAToken_t token)
{
    OIC_LOG(DEBUG, TAG, "CADestroyToken");
    CADestroyTokenInternal(token);
}

CAResult_t CAGetNetworkInformation(CAEndpoint_t **info, uint32_t *size)
{
    OIC_LOG(DEBUG, TAG, "CAGetNetworkInformation");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    return CAGetNetworkInformationInternal(info, size);
}

CAResult_t CASendRequest(const CAEndpoint_t *object,const CARequestInfo_t *requestInfo)
{
    OIC_LOG(DEBUG, TAG, "CASendGetRequest");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    return CADetachRequestMessage(object, requestInfo);
}

CAResult_t CASendNotification(const CAEndpoint_t *object, const CAResponseInfo_t *responseInfo)
{
    OIC_LOG(DEBUG, TAG, "CASendNotification");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    return CADetachResponseMessage(object, responseInfo);

}

CAResult_t CASendResponse(const CAEndpoint_t *object, const CAResponseInfo_t *responseInfo)
{
    OIC_LOG(DEBUG, TAG, "CASendResponse");

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    return CADetachResponseMessage(object, responseInfo);

}

CAResult_t CASelectNetwork(const uint32_t interestedNetwork)
{
    OIC_LOG_V(DEBUG, TAG, "Selected network : %d", interestedNetwork);

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    CAResult_t res = CA_STATUS_OK;

    if (interestedNetwork & CA_ADAPTER_IP)
    {
        res = CAAddNetworkType(CA_ADAPTER_IP);
        OIC_LOG_V(ERROR, TAG, "CAAddNetworkType(CA_IP_ADAPTER) function returns error : %d", res);
    }
    else if (interestedNetwork & CA_ADAPTER_RFCOMM_BTEDR)
    {
        res = CAAddNetworkType(CA_ADAPTER_RFCOMM_BTEDR);
        OIC_LOG_V(ERROR, TAG, "CAAddNetworkType(CA_RFCOMM_ADAPTER) function returns error : %d", res);
    }
    else if (interestedNetwork & CA_ADAPTER_GATT_BTLE)
    {
        res = CAAddNetworkType(CA_ADAPTER_GATT_BTLE);
        OIC_LOG_V(ERROR, TAG, "CAAddNetworkType(CA_GATT_ADAPTER) function returns error : %d", res);
    }
    else
    {
        res = CA_NOT_SUPPORTED;
    }

    return res;
}

CAResult_t CAUnSelectNetwork(const uint32_t nonInterestedNetwork)
{
    OIC_LOG_V(DEBUG, TAG, "unselected network : %d", nonInterestedNetwork);

    if(!g_isInitialized)
    {
        return CA_STATUS_NOT_INITIALIZED;
    }

    CAResult_t res = CA_STATUS_OK;

    if (nonInterestedNetwork & CA_ADAPTER_IP)
    {
        res = CARemoveNetworkType(CA_ADAPTER_IP);
        OIC_LOG_V(ERROR, TAG, "CARemoveNetworkType(CA_IP_ADAPTER) function returns error : %d", res);
    }
    else if (nonInterestedNetwork & CA_ADAPTER_RFCOMM_BTEDR)
    {
        res = CARemoveNetworkType(CA_ADAPTER_RFCOMM_BTEDR);
        OIC_LOG_V(ERROR, TAG, "CARemoveNetworkType(CA_RFCOMM_ADAPTER) function returns error : %d", res);
    }
    else if (nonInterestedNetwork & CA_ADAPTER_GATT_BTLE)
    {
        res = CARemoveNetworkType(CA_ADAPTER_GATT_BTLE);
        OIC_LOG_V(ERROR, TAG, "CARemoveNetworkType(CA_GATT_ADAPTER) function returns error : %d", res);
    }
    else
    {
        res = CA_STATUS_FAILED;
    }

    return res;
}

CAResult_t CAHandleRequestResponse()
{
    OIC_LOG(DEBUG, TAG, "CAHandleRequestResponse");

    if (!g_isInitialized)
    {
        OIC_LOG(ERROR, TAG, "not initialized");
        return CA_STATUS_NOT_INITIALIZED;
    }

    CAHandleRequestResponseCallbacks();

    return CA_STATUS_OK;
}

#ifdef __WITH_DTLS__

CAResult_t CASelectCipherSuite(const uint16_t cipher)
{
    OIC_LOG_V(DEBUG, TAG, "CASelectCipherSuite");

    return CADtlsSelectCipherSuite(cipher);
}

CAResult_t CAEnableAnonECDHCipherSuite(const bool enable)
{
    OIC_LOG_V(DEBUG, TAG, "CAEnableAnonECDHCipherSuite");

    return CADtlsEnableAnonECDHCipherSuite(enable);
}

CAResult_t CAGenerateOwnerPSK(const CAEndpoint_t* endpoint,
                    const uint8_t* label, const size_t labelLen,
                    const uint8_t* rsrcServerDeviceID, const size_t rsrcServerDeviceIDLen,
                    const uint8_t* provServerDeviceID, const size_t provServerDeviceIDLen,
                    uint8_t* ownerPSK, const size_t ownerPSKSize)
{
    OIC_LOG_V(DEBUG, TAG, "IN : CAGenerateOwnerPSK");

    CAResult_t res = CA_STATUS_OK;

    //newOwnerLabel and prevOwnerLabe can be NULL
    if (!endpoint || !label || 0 == labelLen || !ownerPSK || 0 == ownerPSKSize)
    {
        return CA_STATUS_INVALID_PARAM;
    }

    res = CADtlsGenerateOwnerPSK(endpoint, label, labelLen,
                                  rsrcServerDeviceID, rsrcServerDeviceIDLen,
                                  provServerDeviceID, provServerDeviceIDLen,
                                  ownerPSK, ownerPSKSize);
    if (CA_STATUS_OK != res)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to CAGenerateOwnerPSK : %d", res);
    }

    OIC_LOG_V(DEBUG, TAG, "OUT : CAGenerateOwnerPSK");

    return res;
}

CAResult_t CAInitiateHandshake(const CAEndpoint_t *endpoint)
{
    OIC_LOG_V(DEBUG, TAG, "IN : CAInitiateHandshake");
    CAResult_t res = CA_STATUS_OK;

    if (!endpoint)
    {
        return CA_STATUS_INVALID_PARAM;
    }

    res = CADtlsInitiateHandshake(endpoint);
    if (CA_STATUS_OK != res)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to CADtlsInitiateHandshake : %d", res);
    }

    OIC_LOG_V(DEBUG, TAG, "OUT : CAInitiateHandshake");

    return res;
}

CAResult_t CACloseDtlsSession(const CAEndpoint_t *endpoint)
{
    OIC_LOG_V(DEBUG, TAG, "IN : CACloseDtlsSession");
    CAResult_t res = CA_STATUS_OK;

    if (!endpoint)
    {
        return CA_STATUS_INVALID_PARAM;
    }

    res = CADtlsClose(endpoint);
    if (CA_STATUS_OK != res)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to CADtlsClose : %d", res);
    }

    OIC_LOG_V(DEBUG, TAG, "OUT : CACloseDtlsSession");

    return res;
}

#endif /* __WITH_DTLS__ */