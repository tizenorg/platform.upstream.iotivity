//******************************************************************
//
// Copyright 2014 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include <string.h>
#include "ocstack.h"
#include "ocstackconfig.h"
#include "ocstackinternal.h"
#include "ocobserve.h"
#include "ocresourcehandler.h"
#include "ocrandom.h"
#include "ocmalloc.h"
#include "ocserverrequest.h"

#include "utlist.h"
#include "pdu.h"

// Module Name
#define MOD_NAME PCF("ocobserve")

#define TAG  PCF("OCStackObserve")

#define VERIFY_NON_NULL(arg) { if (!arg) {OC_LOG(FATAL, TAG, #arg " is NULL"); goto exit;} }

static struct ResourceObserver * serverObsList = NULL;

/**
 * Determine observe QOS based on the QOS of the request.
 * The qos passed as a parameter overrides what the client requested.
 * If we want the client preference taking high priority make:
 *     qos = resourceObserver->qos;
 *
 * @param method RESTful method.
 * @param resourceObserver Observer.
 * @param appQoS Quality of service.
 * @return The quality of service of the observer.
 */
static OCQualityOfService DetermineObserverQoS(OCMethod method,
        ResourceObserver * resourceObserver, OCQualityOfService appQoS)
{
    if(!resourceObserver)
    {
        OC_LOG(ERROR, TAG, "DetermineObserverQoS called with invalid resourceObserver");
        return OC_NA_QOS;
    }

    OCQualityOfService decidedQoS = appQoS;
    if(appQoS == OC_NA_QOS)
    {
        decidedQoS = resourceObserver->qos;
    }

    if(appQoS != OC_HIGH_QOS)
    {
        OC_LOG_V(INFO, TAG, "Current NON count for this observer is %d",
                resourceObserver->lowQosCount);
        #ifdef WITH_PRESENCE
        if((resourceObserver->forceHighQos \
                || resourceObserver->lowQosCount >= MAX_OBSERVER_NON_COUNT) \
                && method != OC_REST_PRESENCE)
        #else
        if(resourceObserver->forceHighQos \
                || resourceObserver->lowQosCount >= MAX_OBSERVER_NON_COUNT)
        #endif
        {
            resourceObserver->lowQosCount = 0;
            // at some point we have to to send CON to check on the
            // availability of observer
            OC_LOG(INFO, TAG, PCF("This time we are sending the  notification as High qos"));
            decidedQoS = OC_HIGH_QOS;
        }
        else
        {
            (resourceObserver->lowQosCount)++;
        }
    }
    return decidedQoS;
}

#ifdef WITH_PRESENCE
OCStackResult SendAllObserverNotification (OCMethod method, OCResource *resPtr, uint32_t maxAge,
        OCResourceType *resourceType, OCQualityOfService qos)
#else
OCStackResult SendAllObserverNotification (OCMethod method, OCResource *resPtr, uint32_t maxAge,
        OCQualityOfService qos)
#endif
{
    OC_LOG(INFO, TAG, PCF("Entering SendObserverNotification"));
    if(!resPtr)
    {
        return OC_STACK_INVALID_PARAM;
    }

    OCStackResult result = OC_STACK_ERROR;
    ResourceObserver * resourceObserver = serverObsList;
    uint8_t numObs = 0;
    OCServerRequest * request = NULL;
    OCEntityHandlerRequest ehRequest = {};
    OCEntityHandlerResult ehResult = OC_EH_ERROR;
    bool observeErrorFlag = false;

    // Find clients that are observing this resource
    while (resourceObserver)
    {
        if (resourceObserver->resource == resPtr)
        {
            numObs++;
            #ifdef WITH_PRESENCE
            if(method != OC_REST_PRESENCE)
            {
            #endif
                qos = DetermineObserverQoS(method, resourceObserver, qos);

                result = AddServerRequest(&request, 0, 0, 0, 1, OC_REST_GET,
                        0, resPtr->sequenceNum, qos, resourceObserver->query,
                        NULL, NULL,
                        resourceObserver->token, resourceObserver->tokenLength,
                        resourceObserver->resUri, 0,
                        &(resourceObserver->addressInfo), resourceObserver->connectivityType);

                if(request)
                {
                    request->observeResult = OC_STACK_OK;
                    if(result == OC_STACK_OK)
                    {
                        result = FormOCEntityHandlerRequest(&ehRequest, (OCRequestHandle) request,
                                    request->method, (OCResourceHandle) resPtr, request->query,
                                    request->reqJSONPayload,
                                    request->numRcvdVendorSpecificHeaderOptions,
                                    request->rcvdVendorSpecificHeaderOptions,
                                    OC_OBSERVE_NO_OPTION, 0);
                        if(result == OC_STACK_OK)
                        {
                            ehResult = resPtr->entityHandler(OC_REQUEST_FLAG, &ehRequest);
                            if(ehResult == OC_EH_ERROR)
                            {
                                FindAndDeleteServerRequest(request);
                            }
                        }
                    }
                }
            #ifdef WITH_PRESENCE
            }
            else
            {
                OCEntityHandlerResponse ehResponse = {};
                char presenceResBuf[MAX_RESPONSE_LENGTH] = {};

                //This is effectively the implementation for the presence entity handler.
                OC_LOG(DEBUG, TAG, PCF("This notification is for Presence"));

                result = AddServerRequest(&request, 0, 0, 0, 1, OC_REST_GET,
                        0, resPtr->sequenceNum, qos, resourceObserver->query,
                        NULL, NULL,
                        resourceObserver->token, resourceObserver->tokenLength,
                        resourceObserver->resUri, 0,
                        &(resourceObserver->addressInfo), resourceObserver->connectivityType);

                if(result == OC_STACK_OK)
                {
                    // we create the payload here
                    if(resourceType && resourceType->resourcetypename)
                    {
                        snprintf((char *)presenceResBuf, sizeof(presenceResBuf), "%u:%u:%s",
                                resPtr->sequenceNum, maxAge, resourceType->resourcetypename);
                    }
                    else
                    {
                        snprintf((char *)presenceResBuf, sizeof(presenceResBuf), "%u:%u",
                                resPtr->sequenceNum, maxAge);
                    }
                    ehResponse.ehResult = OC_EH_OK;
                    ehResponse.payload = presenceResBuf;
                    ehResponse.payloadSize = strlen((const char *)presenceResBuf) + 1;
                    ehResponse.persistentBufferFlag = 0;
                    ehResponse.requestHandle = (OCRequestHandle) request;
                    ehResponse.resourceHandle = (OCResourceHandle) resPtr;
                    strcpy((char *)ehResponse.resourceUri, (const char *)resourceObserver->resUri);
                    result = OCDoResponse(&ehResponse);
                }
            }
            #endif

            // Since we are in a loop, set an error flag to indicate at least one error occurred.
            if (result != OC_STACK_OK)
            {
                observeErrorFlag = true;
            }
        }
        resourceObserver = resourceObserver->next;
    }

    if (numObs == 0)
    {
        OC_LOG(INFO, TAG, PCF("Resource has no observers"));
        result = OC_STACK_NO_OBSERVERS;
    }
    else if (observeErrorFlag)
    {
        OC_LOG(ERROR, TAG, PCF("Observer notification error"));
        result = OC_STACK_ERROR;
    }
    return result;
}

OCStackResult SendListObserverNotification (OCResource * resource,
        OCObservationId  *obsIdList, uint8_t numberOfIds,
        const char *notificationJSONPayload, uint32_t maxAge,
        OCQualityOfService qos)
{
    if(!resource || !obsIdList || !notificationJSONPayload)
    {
        return OC_STACK_INVALID_PARAM;
    }

    uint8_t numIds = numberOfIds;
    ResourceObserver *observer = NULL;
    uint8_t numSentNotification = 0;
    OCServerRequest * request = NULL;
    OCStackResult result = OC_STACK_ERROR;
    bool observeErrorFlag = false;

    OC_LOG(INFO, TAG, PCF("Entering SendListObserverNotification"));
    while(numIds)
    {
        observer = GetObserverUsingId (*obsIdList);
        if(observer)
        {
            // Found observer - verify if it matches the resource handle
            if (observer->resource == resource)
            {
                qos = DetermineObserverQoS(OC_REST_GET, observer, qos);


                result = AddServerRequest(&request, 0, 0, 0, 1, OC_REST_GET,
                        0, resource->sequenceNum, qos, observer->query,
                        NULL, NULL, observer->token, observer->tokenLength,
                        observer->resUri, 0,
                        &(observer->addressInfo), observer->connectivityType);

                if(request)
                {
                    request->observeResult = OC_STACK_OK;
                    if(result == OC_STACK_OK)
                    {
                        OCEntityHandlerResponse ehResponse = {};
                        ehResponse.ehResult = OC_EH_OK;
                        ehResponse.payload = (char *) OCMalloc(MAX_RESPONSE_LENGTH + 1);
                        if(!ehResponse.payload)
                        {
                            FindAndDeleteServerRequest(request);
                            continue;
                        }
                        strncpy(ehResponse.payload, notificationJSONPayload, MAX_RESPONSE_LENGTH-1);
                        ehResponse.payload[MAX_RESPONSE_LENGTH] = '\0';
                        ehResponse.payloadSize = strlen(ehResponse.payload) + 1;
                        ehResponse.persistentBufferFlag = 0;
                        ehResponse.requestHandle = (OCRequestHandle) request;
                        ehResponse.resourceHandle = (OCResourceHandle) resource;
                        result = OCDoResponse(&ehResponse);
                        if(result == OC_STACK_OK)
                        {
                            OC_LOG_V(INFO, TAG, "Observer id %d notified.", *obsIdList);

                            // Increment only if OCDoResponse is successful
                            numSentNotification++;

                            OCFree(ehResponse.payload);
                            FindAndDeleteServerRequest(request);
                        }
                        else
                        {
                            OC_LOG_V(INFO, TAG, "Error notifying observer id %d.", *obsIdList);
                        }
                    }
                    else
                    {
                        FindAndDeleteServerRequest(request);
                    }
                }
                // Since we are in a loop, set an error flag to indicate
                // at least one error occurred.
                if (result != OC_STACK_OK)
                {
                    observeErrorFlag = true;
                }
            }
        }
        obsIdList++;
        numIds--;
    }

    if(numSentNotification == numberOfIds && !observeErrorFlag)
    {
        return OC_STACK_OK;
    }
    else if(numSentNotification == 0)
    {
        return OC_STACK_NO_OBSERVERS;
    }
    else
    {
        OC_LOG(ERROR, TAG, PCF("Observer notification error"));
        return OC_STACK_ERROR;
    }
}

OCStackResult GenerateObserverId (OCObservationId *observationId)
{
    ResourceObserver *resObs = NULL;

    OC_LOG(INFO, TAG, PCF("Entering GenerateObserverId"));
    VERIFY_NON_NULL (observationId);

    do
    {
        *observationId = OCGetRandomByte();
        // Check if observation Id already exists
        resObs = GetObserverUsingId (*observationId);
    } while (NULL != resObs);

    OC_LOG_V(INFO, TAG, "Generated bservation ID is %u", *observationId);

    return OC_STACK_OK;
exit:
    return OC_STACK_ERROR;
}

OCStackResult AddObserver (const char         *resUri,
                           const char         *query,
                           OCObservationId    obsId,
                           CAToken_t          token,
                           uint8_t            tokenLength,
                           OCResource         *resHandle,
                           OCQualityOfService qos,
                           const CAAddress_t  *addressInfo,
                           CATransportType_t connectivityType)
{
    // Check if resource exists and is observable.
    if (!resHandle)
    {
        return OC_STACK_INVALID_PARAM;
    }
    if (!(resHandle->resourceProperties & OC_OBSERVABLE))
    {
        return OC_STACK_RESOURCE_ERROR;
    }
    ResourceObserver *obsNode = NULL;

    if(!resUri || !token || !*token)
    {
        return OC_STACK_INVALID_PARAM;
    }

    obsNode = (ResourceObserver *) OCCalloc(1, sizeof(ResourceObserver));
    if (obsNode)
    {
        obsNode->observeId = obsId;

        obsNode->resUri = (char *)OCMalloc(strlen(resUri)+1);
        VERIFY_NON_NULL (obsNode->resUri);
        memcpy (obsNode->resUri, resUri, strlen(resUri)+1);

        obsNode->qos = qos;
        if(query)
        {
            obsNode->query = (char *)OCMalloc(strlen(query)+1);
            VERIFY_NON_NULL (obsNode->query);
            memcpy (obsNode->query, query, strlen(query)+1);
        }
        // If tokenLength is zero, the return value depends on the
        // particular library implementation (it may or may not be a null pointer).
        if(tokenLength)
        {
            obsNode->token = (CAToken_t)OCMalloc(tokenLength);
            VERIFY_NON_NULL (obsNode->token);
            memcpy(obsNode->token, token, tokenLength);
        }
        obsNode->tokenLength = tokenLength;
        obsNode->addressInfo = *addressInfo;
        obsNode->connectivityType = connectivityType;
        obsNode->resource = resHandle;
        LL_APPEND (serverObsList, obsNode);
        return OC_STACK_OK;
    }

exit:
    if (obsNode)
    {
        OCFree(obsNode->resUri);
        OCFree(obsNode->query);
        OCFree(obsNode);
    }
    return OC_STACK_NO_MEMORY;
}

ResourceObserver* GetObserverUsingId (const OCObservationId observeId)
{
    ResourceObserver *out = NULL;

    if (observeId)
    {
        LL_FOREACH (serverObsList, out)
        {
            if (out->observeId == observeId)
            {
                return out;
            }
        }
    }
    OC_LOG(INFO, TAG, PCF("Observer node not found!!"));
    return NULL;
}

ResourceObserver* GetObserverUsingToken (const CAToken_t token, uint8_t tokenLength)
{
    ResourceObserver *out = NULL;

    if(token && *token)
    {
        LL_FOREACH (serverObsList, out)
        {
            OC_LOG(INFO, TAG,PCF("comparing tokens"));
            OC_LOG_BUFFER(INFO, TAG, (const uint8_t *)token, tokenLength);
            OC_LOG_BUFFER(INFO, TAG, (const uint8_t *)out->token, tokenLength);
            if((memcmp(out->token, token, tokenLength) == 0))
            {
                return out;
            }
        }
    }
    OC_LOG(INFO, TAG, PCF("Observer node not found!!"));
    return NULL;
}

OCStackResult DeleteObserverUsingToken (CAToken_t token, uint8_t tokenLength)
{
    if(!token || !*token)
    {
        return OC_STACK_INVALID_PARAM;
    }

    ResourceObserver *obsNode = NULL;

    obsNode = GetObserverUsingToken (token, tokenLength);
    if (obsNode)
    {
        OC_LOG_V(INFO, TAG, PCF("deleting tokens"));
        OC_LOG_BUFFER(INFO, TAG, (const uint8_t *)obsNode->token, tokenLength);
        LL_DELETE (serverObsList, obsNode);
        OCFree(obsNode->resUri);
        OCFree(obsNode->query);
        OCFree(obsNode->token);
        OCFree(obsNode);
    }
    // it is ok if we did not find the observer...
    return OC_STACK_OK;
}

void DeleteObserverList()
{
    ResourceObserver *out = NULL;
    ResourceObserver *tmp = NULL;
    LL_FOREACH_SAFE (serverObsList, out, tmp)
    {
        if(out)
        {
            DeleteObserverUsingToken ((out->token), out->tokenLength);
        }
    }
    serverObsList = NULL;
}

/*
 * CA layer expects observe registration/de-reg/notiifcations to be passed as a header
 * option, which breaks the protocol abstraction requirement between RI & CA, and
 * has to be fixed in the future. The function below adds the header option for observe.
 * It should be noted that the observe header option is assumed to be the first option
 * in the list of user defined header options and hence it is inserted at the front
 * of the header options list and number of options adjusted accordingly.
 */
OCStackResult
CreateObserveHeaderOption (CAHeaderOption_t **caHdrOpt,
                           OCHeaderOption *ocHdrOpt,
                           uint8_t numOptions,
                           uint8_t observeFlag)
{
    if(!caHdrOpt)
    {
        return OC_STACK_INVALID_PARAM;
    }

    CAHeaderOption_t *tmpHdrOpt = NULL;

    tmpHdrOpt = (CAHeaderOption_t *) OCCalloc ((numOptions+1), sizeof(CAHeaderOption_t));
    if (NULL == tmpHdrOpt)
    {
        return OC_STACK_NO_MEMORY;
    }
    tmpHdrOpt[0].protocolID = CA_COAP_ID;
    tmpHdrOpt[0].optionID = COAP_OPTION_OBSERVE;
    tmpHdrOpt[0].optionLength = sizeof(uint32_t);
    tmpHdrOpt[0].optionData[0] = observeFlag;
    for (uint8_t i = 0; i < numOptions; i++)
    {
        memcpy (&(tmpHdrOpt[i+1]), &(ocHdrOpt[i]), sizeof(CAHeaderOption_t));
    }

    *caHdrOpt = tmpHdrOpt;
    return OC_STACK_OK;
}

/*
 * CA layer passes observe information to the RI layer as a header option, which
 * breaks the protocol abstraction requirement between RI & CA, and has to be fixed
 * in the future. The function below removes the observe header option and processes it.
 * It should be noted that the observe header option is always assumed to be the first
 * option in the list of user defined header options and hence it is deleted from the
 * front of the header options list and the number of options is adjusted accordingly.
 */
OCStackResult
GetObserveHeaderOption (uint32_t * observationOption,
                        CAHeaderOption_t *options,
                        uint8_t * numOptions)
{
    if(!observationOption)
    {
        return OC_STACK_INVALID_PARAM;
    }
    *observationOption = OC_OBSERVE_NO_OPTION;

    if(!options || !numOptions)
    {
        return OC_STACK_INVALID_PARAM;
    }

    for(uint8_t i = 0; i < *numOptions; i++)
    {
        if(options[i].protocolID == CA_COAP_ID &&
                options[i].optionID == COAP_OPTION_OBSERVE)
        {
            *observationOption = options[i].optionData[0];
            for(uint8_t c = i; c < *numOptions-1; c++)
            {
                options[i].protocolID = options[i+1].protocolID;
                options[i].optionID = options[i+1].optionID;
                options[i].optionLength = options[i+1].optionLength;
                memcpy(options[i].optionData, options[i+1].optionData, options[i+1].optionLength);
            }
            (*numOptions)--;
            return OC_STACK_OK;
        }
    }
    return OC_STACK_OK;
}

