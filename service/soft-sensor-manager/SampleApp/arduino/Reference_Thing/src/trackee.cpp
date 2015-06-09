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

// Do not remove the include below
#include "Arduino.h"
#include "bleLib.h"
#include <stdio.h>

#include "logger.h"
#include "ocstack.h"
#include <string.h>

#include "oic_lanLib.h"

// proximity code s
#define DATA_EA     400
#define SLAVER_EA   2

#define ARDUINO_AVR_MEGA2560 1
/// This is the port which Arduino Server will use for all unicast communication with it's peers
#define OC_WELL_KNOWN_PORT 5683

PROGMEM const char TAG[] = "TrackeeSensor";

OCResourceHandle m_handle;



Cble ble;
char trackeeID[13] = "9059AF16FEF7";
int slaver_num = 0;
//char slaveList[SLAVER_EA][13]={"9059AF1700EE"};
char slaveList[SLAVER_EA][13] = {"9059AF1700EE", "34B1F7D004D2"};
int g_PROXIUnderObservation = 0;



const char *getResult(OCStackResult result);
void createResource();


#define LENGTH_VAR      50
bool JsonGenerator( char *jsonBuf, uint16_t buf_length )
{

    return true;

}


// On Arduino Atmel boards with Harvard memory architecture, the stack grows
// downwards from the top and the heap grows upwards. This method will print
// the distance(in terms of bytes) between those two.
// See here for more details :
// http://www.atmel.com/webdoc/AVRLibcReferenceManual/malloc_1malloc_intro.html
void PrintArduinoMemoryStats()
{
#ifdef ARDUINO_AVR_MEGA2560
    //This var is declared in avr-libc/stdlib/malloc.c
    //It keeps the largest address not allocated for heap
    extern char *__brkval;
    //address of tmp gives us the current stack boundry
    int tmp;
    OC_LOG_V(INFO, TAG, "Stack: %u         Heap: %u", (unsigned int)&tmp, (unsigned int)__brkval);
    OC_LOG_V(INFO, TAG, "Unallocated Memory between heap and stack: %u",
             ((unsigned int)&tmp - (unsigned int)__brkval));
#endif
}

// This is the entity handler for the registered resource.
// This is invoked by OCStack whenever it recevies a request for this resource.
OCEntityHandlerResult OCEntityHandlerCb(OCEntityHandlerFlag flag,
                                        OCEntityHandlerRequest *entityHandlerRequest )
{
    OCEntityHandlerResult ehRet = OC_EH_OK;

    if (entityHandlerRequest && (flag & OC_REQUEST_FLAG))
    {
        OC_LOG (INFO, TAG, PCF("Flag includes OC_REQUEST_FLAG"));
        if (OC_REST_GET == entityHandlerRequest->method)
        {
            if ( JsonGenerator( (char *)entityHandlerRequest->resJSONPayload,
                                entityHandlerRequest->resJSONPayloadLen ) == false )
            {
                ehRet  = OC_EH_ERROR;
            }
        }
        if (OC_REST_PUT == entityHandlerRequest->method)
        {
            if (JsonGenerator( (char *)entityHandlerRequest->resJSONPayload,
                               entityHandlerRequest->resJSONPayloadLen ) == false )
            {
                ehRet  = OC_EH_ERROR;
            }
        }
    }
    else if (entityHandlerRequest && (flag & OC_OBSERVE_FLAG))
    {
        if (OC_OBSERVE_REGISTER == entityHandlerRequest->obsInfo->action)
        {
            OC_LOG (INFO, TAG, PCF("Received OC_OBSERVE_REGISTER from client"));
            g_PROXIUnderObservation = 1;
        }
        else if (OC_OBSERVE_DEREGISTER == entityHandlerRequest->obsInfo->action)
        {
            OC_LOG (INFO, TAG, PCF("Received OC_OBSERVE_DEREGISTER from client"));
        }
    }

    Serial.println((char *)entityHandlerRequest->resJSONPayload);

    return ehRet;
}







//The setup function is called once at startup of the sketch
void setup()
{
    Serial.begin(115200);

    // Add your initialization code here
    OC_LOG_INIT();

    OC_LOG(DEBUG, TAG, PCF("OCServer is starting..."));
    //    uint16_t port = OC_WELL_KNOWN_PORT;

    // Connect to Ethernet or WiFi network
    if (ConnectToNetwork() != 0)
    {
        OC_LOG(ERROR, TAG, "Unable to connect to network");
        return;
    }

    // Initialize the OC Stack in Server mode
    if (OCInit(NULL, OC_WELL_KNOWN_PORT, OC_SERVER) != OC_STACK_OK)
    {
        OC_LOG(ERROR, TAG, PCF("OCStack init error"));
        return;
    }

    // Declare and create the example resource
    createResource();

    // This call displays the amount of free SRAM available on Arduino
    PrintArduinoMemoryStats();

    ble.init( (long)115200, BLE_SLAVER, slaveList[0]);


//  ble.StatusRead();

    OC_LOG_V(INFO, TAG, "Program Start-\r\n");
}


// The loop function is called in an endless loop
void loop()
{
    // This artificial delay is kept here to avoid endless spinning
    // of Arduino microcontroller. Modify it as per specfic application needs.

    if (OCProcess() != OC_STACK_OK)
    {
        OC_LOG(ERROR, TAG, PCF("OCStack process error"));
        return;
    }

    char *user_cmd = NULL;

//  ble.pollingDisconnect();

    user_cmd = ble.Debug2BLE(true);
    ble.BLE2Debug( true );

    if ( user_cmd )
    {
        free( user_cmd );
        user_cmd = NULL;
    }

}







void createResource()
{

    OCStackResult res = OCCreateResource(&m_handle,
                                         "SSManager.Sensor",
                                         OC_RSRVD_INTERFACE_DEFAULT,
                                         "/Tracker_Thing",
                                         OCEntityHandlerCb,
                                         OC_DISCOVERABLE | OC_OBSERVABLE);
    OC_LOG_V(INFO, TAG, "Created PROXI resource with result: %s", getResult(res));
}

const char *getResult(OCStackResult result)
{
    switch (result)
    {
        case OC_STACK_OK:
            return "OC_STACK_OK";
        case OC_STACK_INVALID_URI:
            return "OC_STACK_INVALID_URI";
        case OC_STACK_INVALID_QUERY:
            return "OC_STACK_INVALID_QUERY";
        case OC_STACK_INVALID_IP:
            return "OC_STACK_INVALID_IP";
        case OC_STACK_INVALID_PORT:
            return "OC_STACK_INVALID_PORT";
        case OC_STACK_INVALID_CALLBACK:
            return "OC_STACK_INVALID_CALLBACK";
        case OC_STACK_INVALID_METHOD:
            return "OC_STACK_INVALID_METHOD";
        case OC_STACK_NO_MEMORY:
            return "OC_STACK_NO_MEMORY";
        case OC_STACK_COMM_ERROR:
            return "OC_STACK_COMM_ERROR";
        case OC_STACK_INVALID_PARAM:
            return "OC_STACK_INVALID_PARAM";
        case OC_STACK_NOTIMPL:
            return "OC_STACK_NOTIMPL";
        case OC_STACK_NO_RESOURCE:
            return "OC_STACK_NO_RESOURCE";
        case OC_STACK_RESOURCE_ERROR:
            return "OC_STACK_RESOURCE_ERROR";
        case OC_STACK_SLOW_RESOURCE:
            return "OC_STACK_SLOW_RESOURCE";
        case OC_STACK_NO_OBSERVERS:
            return "OC_STACK_NO_OBSERVERS";
        case OC_STACK_ERROR:
            return "OC_STACK_ERROR";
        default:
            return "UNKNOWN";
    }
}

