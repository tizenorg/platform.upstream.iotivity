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

/**
 * @file camessagehandler.h
 * @brief This file contains message functionality.
 */

#ifndef CA_MESSAGE_HANDLER_H_
#define CA_MESSAGE_HANDLER_H_

#include "cacommon.h"
#include "coap.h"

/**
 * @def VERIFY_NON_NULL
 * @brief Macro to verify the validity of input argument.
 */
#define VERIFY_NON_NULL(arg, log_tag, log_message) \
    if (NULL == arg ){ \
        OIC_LOG_V(ERROR, log_tag, "Invalid input:%s", log_message); \
        return CA_STATUS_INVALID_PARAM; \
    } \

/**
 * @def VERIFY_NON_NULL_VOID
 * @brief Macro to verify the validity of input argument.
 */
#define VERIFY_NON_NULL_VOID(arg, log_tag, log_message) \
    if (NULL == arg ){ \
        OIC_LOG_V(ERROR, log_tag, "Invalid input:%s", log_message); \
        return; \
    } \

#define CA_MEMORY_ALLOC_CHECK(arg) { if (NULL == arg) {OIC_LOG(ERROR, TAG, "Out of memory"); \
goto memory_error_exit;} }

typedef enum
{
    SEND_TYPE_MULTICAST = 0, SEND_TYPE_UNICAST
} CASendDataType_t;

typedef enum
{
    CA_REQUEST_DATA = 1,
    CA_RESPONSE_DATA = 2,
    CA_ERROR_DATA = 3,
} CADataType_t;

typedef struct
{
    CASendDataType_t type;
    CAEndpoint_t *remoteEndpoint;
    CARequestInfo_t *requestInfo;
    CAResponseInfo_t *responseInfo;
    CAErrorInfo_t *errorInfo;
    CAHeaderOption_t *options;
    CADataType_t dataType;
    uint8_t numOptions;
} CAData_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   Detaches control from the caller for sending unicast request
 * @param   endpoint       [IN]    endpoint information where the data has to be sent
 * @param   request        [IN]    request that needs to be sent
 * @return  CA_STATUS_OK or ERROR CODES (CAResult_t error codes in cacommon.h)
 */
CAResult_t CADetachRequestMessage(const CAEndpoint_t *endpoint,
                                  const CARequestInfo_t *request);

/**
 * @brief   Detaches control from the caller for sending multicast request
 * @param   object         [IN]    Group endpoint information where the data has to be sent
 * @param   request        [IN]    request that needs to be sent
 * @return  CA_STATUS_OK or ERROR CODES (CAResult_t error codes in cacommon.h)
 */
CAResult_t CADetachRequestToAllMessage(const CAEndpoint_t *object,
                                       const CARequestInfo_t *request);

/**
 * @brief   Detaches control from the caller for sending response
 * @param   endpoint       [IN]    endpoint information where the data has to be sent
 * @param   response       [IN]    response that needs to be sent
 * @return  CA_STATUS_OK or ERROR CODES (CAResult_t error codes in cacommon.h)
 */
CAResult_t CADetachResponseMessage(const CAEndpoint_t *endpoint,
                                   const CAResponseInfo_t *response);

/**
 * @brief   Detaches control from the caller for sending request
 * @param   resourceUri    [IN]    resource uri that needs to  be sent in the request
 * @param   token          [IN]    token information of the request
 * @param   tokenLength    [IN]    length of the token
 * @param   options        [IN]    header options that need to be append in the request
 * @param   numOptions     [IN]    number of options be appended
 * @return  CA_STATUS_OK or ERROR CODES (CAResult_t error codes in cacommon.h)
 */
CAResult_t CADetachMessageResourceUri(const CAURI_t resourceUri, const CAToken_t token,
                                      uint8_t tokenLength, const CAHeaderOption_t *options,
                                      uint8_t numOptions);

/**
 * @brief   Setting the request and response callbacks for network packets
 * @param   ReqHandler     [IN]    callback for receiving the requests
 * @param   RespHandler    [IN]    callback for receiving the response
 * @param   ErrorHandler   [IN]    callback for receiving error response
 * @return  NONE
 */
void CASetInterfaceCallbacks(CARequestCallback ReqHandler, CAResponseCallback RespHandler,
                             CAErrorCallback ErrorHandler);

/**
 * @brief   Initialize the message handler by starting thread pool and initializing the
 *          send and receive queue
 * @return  CA_STATUS_OK or ERROR CODES (CAResult_t error codes in cacommon.h)
 */
CAResult_t CAInitializeMessageHandler();

/**
 * @brief   Terminate the message handler by stopping  the thread pool and destroying the queues
 * @return  NONE
 */
void CATerminateMessageHandler();

/**
 * @brief   Handler for receiving request and response callback in single thread model
 */
void CAHandleRequestResponseCallbacks();

/**
 * @brief To log the PDU data
 * @param   pdu            [IN]    pdu data
 */
void CALogPDUInfo(coap_pdu_t *pdu);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CA_MESSAGE_HANDLER_H_ */

