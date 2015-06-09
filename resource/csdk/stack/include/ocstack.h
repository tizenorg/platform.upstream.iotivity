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

#ifndef OCSTACK_H_
#define OCSTACK_H_

#include <stdint.h>
#include "octypes.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#define WITH_PRESENCE
#define USE_RANDOM_PORT (0)

//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------

/**
 * Initialize the OC Stack.  Must be called prior to starting the stack.
 *
 * @param ipAddr
 *     IP Address of host device. Deprecated parameter.
 * @param port
 *     Port of host device. Deprecated parameter.
 * @param mode
 *     Host device is client, server, or client-server.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCInit(const char *ipAddr, uint16_t port, OCMode mode);

/**
 * Stop the OC stack.  Use for a controlled shutdown.
 *
 * Note: OCStop() performs operations similar to OCStopPresence(), as well as OCDeleteResource() on
 * all resources this server is hosting. OCDeleteResource() performs operations similar to
 * OCNotifyAllObservers() to notify all client observers that the respective resource is being
 * deleted.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCStop();

/**
 * Called in main loop of OC client or server.  Allows low-level processing of
 * stack services.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCProcess();

/**
 * Discover or Perform requests on a specified resource (specified by that Resource's respective
 * URI).
 *
 * @param handle             @ref OCDoHandle to refer to the request sent out on behalf of
 *                           calling this API. This handle can be used to cancel this operation
 *                           via the OCCancel API.
 *                           Note: This reference is handled internally, and
 *                           should not be free'd by the consumer.  A NULL handle is permitted
 *                           in the event where the caller has no use for the return value.
 * @param method             @ref OCMethod to perform on the resource.
 * @param requiredUri        URI of the resource to interact with.
 * @param referenceUri       URI of the reference resource.
 * @param request            JSON encoded request.
 * @param conType            @ref OCConnectivityType type of connectivity indicating the
 *                           interface. Example: ::OC_WIFI, ::OC_ETHERNET, ::OC_ALL.
 * @param qos                Quality of service. Note that if this API is called on a uri with
 *                           the well-known multicast IP address, the qos will be forced to
 *                           ::OC_LOW_QOS
 *                           since it is impractical to send other QOS levels on such addresses.
 * @param cbData             Asynchronous callback function that is invoked
 *                           by the stack when discovery or resource interaction is complete.
 * @param options            The address of an array containing the vendor specific
 *                           header options to be sent with the request.
 * @param numOptions         Number of header options to be included.
 *
 * Note: Presence subscription amendments (ie. adding additional resource type filters by calling
 * this API again) require the use of the same base URI as the original request to successfully
 * amend the presence filters.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCDoResource(OCDoHandle *handle, OCMethod method, const char *requiredUri,
            const char *referenceUri, const char *request, OCConnectivityType conType,
            OCQualityOfService qos, OCCallbackData *cbData,
            OCHeaderOption * options, uint8_t numOptions);

/**
 * Cancel a request associated with a specific @ref OCDoResource invocation.
 *
 * @param handle - Used to identify a specific OCDoResource invocation.
 * @param qos    - used to specify Quality of Service (read below for more info)
 * @param options- used to specify vendor specific header options when sending
 *                 explicit observe cancellation
 * @param numOptions- Number of header options to be included
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCCancel(OCDoHandle handle, OCQualityOfService qos, OCHeaderOption * options,
        uint8_t numOptions);

#ifdef WITH_PRESENCE
/**
 * When operating in @ref OCServer or @ref OCClientServer mode, this API will start sending out
 * presence notifications to clients via multicast. Once this API has been called with a success,
 * clients may query for this server's presence and this server's stack will respond via multicast.
 *
 * Server can call this function when it comes online for the first time, or when it comes back
 * online from offline mode, or when it re enters network.
 *
 * @param ttl Time To Live in seconds.
 * Note: If ttl is '0', then the default stack value will be used (60 Seconds).
 *
 *       If ttl is greater than OC_MAX_PRESENCE_TTL_SECONDS, then the ttl will be set to
 *       OC_MAX_PRESENCE_TTL_SECONDS.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCStartPresence(const uint32_t ttl);

/**
 * When operating in @ref OCServer or @ref OCClientServer mode, this API will stop sending out
 * presence notifications to clients via multicast. Once this API has been called with a success,
 * this server's stack will not respond to clients querying for this server's presence.
 *
 * Server can call this function when it is terminating, going offline, or when going
 * away from network.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */

OCStackResult OCStopPresence();
#endif


/**
 * Set default device entity handler.
 *
 * @param entityHandler Entity handler function that is called by ocstack to handle requests for
 *                      any undefined resources or default actions.
 *                      If NULL is passed it removes the device default entity handler.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCSetDefaultDeviceEntityHandler(OCDeviceEntityHandler entityHandler);

/**
 * Set device information.
 *
 * @param deviceInfo - Structure passed by the server application containing
 *                     the device information.
 *
 *
 * @return
 *     OC_STACK_OK              - no errors
 *     OC_STACK_INVALID_PARAM   - invalid paramerter
 *     OC_STACK_ERROR           - stack process error
 */
OCStackResult OCSetDeviceInfo(OCDeviceInfo deviceInfo);

/**
 * Set platform information.
 *
 * @param platformInfo - Structure passed by the server application containing
 *                     the platform information.
 *
 *
 * @return
 *     OC_STACK_OK              - no errors
 *     OC_STACK_INVALID_PARAM   - invalid paramerter
 *     OC_STACK_ERROR           - stack process error
 */
OCStackResult OCSetPlatformInfo(OCPlatformInfo platformInfo);

/**
 * Create a resource.
 *
 * @param handle Pointer to handle to newly created resource.  Set by ocstack and
 *               used to refer to resource.
 * @param resourceTypeName Name of resource type.  Example: "core.led".
 * @param resourceInterfaceName Name of resource interface.  Example: "core.rw".
 * @param uri URI of the resource.  Example:  "/a/led".
 * @param entityHandler Entity handler function that is called by ocstack to handle requests, etc.
 *                      NULL for default entity handler.
 * @param resourceProperties Properties supported by resource.
 *                           Example: ::OC_DISCOVERABLE|::OC_OBSERVABLE.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCCreateResource(OCResourceHandle *handle,
                               const char *resourceTypeName,
                               const char *resourceInterfaceName,
                               const char *uri,
                               OCEntityHandler entityHandler,
                               uint8_t resourceProperties);

/**
 * Create a resource. with host ip address for remote resource.
 *
 * @param handle Pointer to handle to newly created resource.  Set by ocstack.
 *               Used to refer to resource.
 * @param resourceTypeName Name of resource type.  Example: "core.led".
 * @param resourceInterfaceName Name of resource interface.  Example: "core.rw".
 * @param host HOST address of the remote resource.  Example:  "coap://xxx.xxx.xxx.xxx:xxxxx".
 * @param uri URI of the resource.  Example:  "/a/led".
 * @param entityHandler Entity handler function that is called by ocstack to handle requests, etc.
 *                      NULL for default entity handler.
 * @param resourceProperties Properties supported by resource.
 *                           Example: ::OC_DISCOVERABLE|::OC_OBSERVABLE
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCCreateResourceWithHost(OCResourceHandle *handle,
                               const char *resourceTypeName,
                               const char *resourceInterfaceName,
                               const char *host,
                               const char *uri,
                               OCEntityHandler entityHandler,
                               uint8_t resourceProperties);

/**
 * Add a resource to a collection resource.
 *
 * @param collectionHandle Handle to the collection resource.
 * @param resourceHandle Handle to resource to be added to the collection resource.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCBindResource(OCResourceHandle collectionHandle, OCResourceHandle resourceHandle);

/**
 * Remove a resource from a collection resource.
 *
 * @param collectionHandle Handle to the collection resource.
 * @param resourceHandle Handle to resource to be removed from the collection resource.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCUnBindResource(OCResourceHandle collectionHandle, OCResourceHandle resourceHandle);

/**
 * Bind a resourcetype to a resource.
 *
 * @param handle Handle to the resource.
 * @param resourceTypeName Name of resource type.  Example: "core.led".
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCBindResourceTypeToResource(OCResourceHandle handle,
                                           const char *resourceTypeName);
/**
 * Bind a resource interface to a resource.
 *
 * @param handle Handle to the resource.
 * @param resourceInterfaceName Name of resource interface.  Example: "core.rw".
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCBindResourceInterfaceToResource(OCResourceHandle handle,
                                                const char *resourceInterfaceName);

/**
 * Bind an entity handler to the resource.
 *
 * @param handle Handle to the resource that the contained resource is to be bound.
 * @param entityHandler Entity handler function that is called by ocstack to handle requests, etc.
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCBindResourceHandler(OCResourceHandle handle, OCEntityHandler entityHandler);

/**
 * Get the number of resources that have been created in the stack.
 *
 * @param numResources Pointer to count variable.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCGetNumberOfResources(uint8_t *numResources);

/**
 * Get a resource handle by index.
 *
 * @param index Index of resource, 0 to Count - 1.
 *
 * @return Found resource handle or NULL if not found.
 */
OCResourceHandle OCGetResourceHandle(uint8_t index);

/**
 * Delete resource specified by handle.  Deletes resource and all resourcetype and resourceinterface
 * linked lists.
 *
 * Note: OCDeleteResource() performs operations similar to OCNotifyAllObservers() to notify all
 * client observers that "this" resource is being deleted.
 *
 * @param handle Handle of resource to be deleted.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCDeleteResource(OCResourceHandle handle);

/**
 * Get the URI of the resource specified by handle.
 *
 * @param handle Handle of resource.
 * @return URI string if resource found or NULL if not found.
 */
const char *OCGetResourceUri(OCResourceHandle handle);

/**
 * Get the properties of the resource specified by handle.
 * NOTE: that after a resource is created, the OC_ACTIVE property is set
 * for the resource by the stack.
 *
 * @param handle Handle of resource.
 * @return OCResourceProperty Bitmask or -1 if resource is not found.
 */
OCResourceProperty OCGetResourceProperties(OCResourceHandle handle);

/**
 * Get the number of resource types of the resource.
 *
 * @param handle Handle of resource.
 * @param numResourceTypes Pointer to count variable.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCGetNumberOfResourceTypes(OCResourceHandle handle, uint8_t *numResourceTypes);

/**
 * Get name of resource type of the resource.
 *
 * @param handle Handle of resource.
 * @param index Index of resource, 0 to Count - 1.
 *
 * @return Resource type name if resource found or NULL if resource not found.
 */
const char *OCGetResourceTypeName(OCResourceHandle handle, uint8_t index);

/**
 * Get the number of resource interfaces of the resource.
 *
 * @param handle Handle of resource.
 * @param numResourceInterfaces Pointer to count variable.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCGetNumberOfResourceInterfaces(OCResourceHandle handle,
        uint8_t *numResourceInterfaces);

/**
 * Get name of resource interface of the resource.
 *
 * @param handle Handle of resource.
 * @param index Index of resource, 0 to Count - 1.
 *
 * @return Resource interface name if resource found or NULL if resource not found.
 */
const char *OCGetResourceInterfaceName(OCResourceHandle handle, uint8_t index);

/**
 * Get methods of resource interface of the resource.
 *
 * @param handle Handle of resource.
 * @param index Index of resource, 0 to Count - 1.
 *
 * @return Allowed methods if resource found or NULL if resource not found.
 */
uint8_t OCGetResourceInterfaceAllowedMethods(OCResourceHandle handle, uint8_t index);

/**
 * Get resource handle from the collection resource by index.
 *
 * @param collectionHandle Handle of collection resource.
 * @param index Index of contained resource, 0 to Count - 1.
 *
 * @return Handle to contained resource if resource found or NULL if resource not found.
 */
OCResourceHandle OCGetResourceHandleFromCollection(OCResourceHandle collectionHandle,
        uint8_t index);

/**
 * Get the entity handler for a resource.
 *
 * @param handle Handle of resource.
 *
 * @return Entity handler if resource found or NULL resource not found.
 */
OCEntityHandler OCGetResourceHandler(OCResourceHandle handle);

/**
 * Notify all registered observers that the resource representation has
 * changed. If observation includes a query the client is notified only
 * if the query is valid after the resource representation has changed.
 *
 * @param handle Handle of resource.
 * @param qos    Desired quality of service for the observation notifications.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCNotifyAllObservers(OCResourceHandle handle, OCQualityOfService qos);

/**
 * Notify specific observers with updated value of representation.
 * Before this API is invoked by entity handler it has finished processing
 * queries for the associated observers.
 *
 * @param handle Handle of resource.
 * @param obsIdList List of observation ids that need to be notified.
 * @param numberOfIds Number of observation ids included in obsIdList.
 * @param notificationJSONPayload JSON encoded payload to send in notification.
 * @param qos Desired quality of service of the observation notifications.
 * NOTE: The memory for obsIdList and notificationJSONPayload is managed by the
 * entity invoking the API. The maximum size of the notification is 1015 bytes
 * for non-Arduino platforms. For Arduino the maximum size is 247 bytes.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult
OCNotifyListOfObservers (OCResourceHandle handle,
                            OCObservationId  *obsIdList,
                            uint8_t          numberOfIds,
                            const char    *notificationJSONPayload,
                            OCQualityOfService qos);


/**
 * Send a response to a request.
 * The response can be a normal, slow, or block (i.e. a response that
 * is too large to be sent in a single PDU and must span multiple transmissions).
 *
 * @param response Pointer to structure that contains response parameters.
 *
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
OCStackResult OCDoResponse(OCEntityHandlerResponse *response);


//Utility methods

/**
 * This method is used to retrieved the IPv4 address from OCDev address
 * data structure.
 *
 * @param ipAddr OCDevAddr address.
 * @param a first byte of IPv4 address.
 * @param b second byte of IPv4 address.
 * @param c third byte of IPv4 address.
 * @param d fourth byte of IPv4 address.
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
int32_t OCDevAddrToIPv4Addr(OCDevAddr *ipAddr, uint8_t *a, uint8_t *b,
            uint8_t *c, uint8_t *d );

/**
 * This method is used to retrieve the port number from OCDev address
 * data structure.
 *
 * @param ipAddr OCDevAddr address.
 * @param port Port number.
 * @return ::OC_STACK_OK on success, some other value upon failure.
 */
int32_t OCDevAddrToPort(OCDevAddr *ipAddr, uint16_t *port);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* OCSTACK_H_ */


