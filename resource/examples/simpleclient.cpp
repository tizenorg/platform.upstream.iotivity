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

// OCClient.cpp : Defines the entry point for the console application.
//
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <mutex>
#include <condition_variable>

#include "OCPlatform.h"
#include "OCApi.h"

using namespace OC;

std::shared_ptr<OCResource> curResource;
static ObserveType OBSERVE_TYPE_TO_USE = ObserveType::Observe;

class Light
{
public:

    bool m_state;
    int m_power;
    std::string m_name;

    Light() : m_state(false), m_power(0), m_name("")
    {
    }
};

Light mylight;

int observe_count()
{
    static int oc = 0;
    return ++oc;
}

void onObserve(const HeaderOptions headerOptions, const OCRepresentation& rep,
                    const int& eCode, const int& sequenceNumber)
{
    if(eCode == OC_STACK_OK)
    {
        std::cout << "OBSERVE RESULT:"<<std::endl;
        std::cout << "\tSequenceNumber: "<< sequenceNumber << endl;

        rep.getValue("state", mylight.m_state);
        rep.getValue("power", mylight.m_power);
        rep.getValue("name", mylight.m_name);

        std::cout << "\tstate: " << mylight.m_state << std::endl;
        std::cout << "\tpower: " << mylight.m_power << std::endl;
        std::cout << "\tname: " << mylight.m_name << std::endl;

        if(observe_count() > 30)
        {
            std::cout<<"Cancelling Observe..."<<std::endl;
            OCStackResult result = curResource->cancelObserve();

            std::cout << "Cancel result: "<< result <<std::endl;
            sleep(10);
            std::cout << "DONE"<<std::endl;
            std::exit(0);
        }
    }
    else
    {
        std::cout << "onObserve Response error: " << eCode << std::endl;
        std::exit(-1);
    }
}

void onPost2(const HeaderOptions& headerOptions, const OCRepresentation& rep, const int eCode)
{
    if(eCode == OC_STACK_OK || eCode == OC_STACK_RESOURCE_CREATED)
    {
        std::cout << "POST request was successful" << std::endl;

        if(rep.hasAttribute("createduri"))
        {
            std::cout << "\tUri of the created resource: "
                      << rep.getValue<std::string>("createduri") << std::endl;
        }
        else
        {
            rep.getValue("state", mylight.m_state);
            rep.getValue("power", mylight.m_power);
            rep.getValue("name", mylight.m_name);

            std::cout << "\tstate: " << mylight.m_state << std::endl;
            std::cout << "\tpower: " << mylight.m_power << std::endl;
            std::cout << "\tname: " << mylight.m_name << std::endl;
        }

        if (OBSERVE_TYPE_TO_USE == ObserveType::Observe)
            std::cout << endl << "Observe is used." << endl << endl;
        else if (OBSERVE_TYPE_TO_USE == ObserveType::ObserveAll)
            std::cout << endl << "ObserveAll is used." << endl << endl;

        curResource->observe(OBSERVE_TYPE_TO_USE, QueryParamsMap(), &onObserve);

    }
    else
    {
        std::cout << "onPost2 Response error: " << eCode << std::endl;
        std::exit(-1);
    }
}

void onPost(const HeaderOptions& headerOptions, const OCRepresentation& rep, const int eCode)
{
    if(eCode == OC_STACK_OK || eCode == OC_STACK_RESOURCE_CREATED)
    {
        std::cout << "POST request was successful" << std::endl;

        if(rep.hasAttribute("createduri"))
        {
            std::cout << "\tUri of the created resource: "
                      << rep.getValue<std::string>("createduri") << std::endl;
        }
        else
        {
            rep.getValue("state", mylight.m_state);
            rep.getValue("power", mylight.m_power);
            rep.getValue("name", mylight.m_name);

            std::cout << "\tstate: " << mylight.m_state << std::endl;
            std::cout << "\tpower: " << mylight.m_power << std::endl;
            std::cout << "\tname: " << mylight.m_name << std::endl;
        }

        OCRepresentation rep2;

        std::cout << "Posting light representation..."<<std::endl;

        mylight.m_state = true;
        mylight.m_power = 55;

        rep2.setValue("state", mylight.m_state);
        rep2.setValue("power", mylight.m_power);

        curResource->post(rep2, QueryParamsMap(), &onPost2);
    }
    else
    {
        std::cout << "onPost Response error: " << eCode << std::endl;
        std::exit(-1);
    }
}

// Local function to put a different state for this resource
void postLightRepresentation(std::shared_ptr<OCResource> resource)
{
    if(resource)
    {
        OCRepresentation rep;

        std::cout << "Posting light representation..."<<std::endl;

        mylight.m_state = false;
        mylight.m_power = 105;

        rep.setValue("state", mylight.m_state);
        rep.setValue("power", mylight.m_power);

        // Invoke resource's post API with rep, query map and the callback parameter
        resource->post(rep, QueryParamsMap(), &onPost);
    }
}

// callback handler on PUT request
void onPut(const HeaderOptions& headerOptions, const OCRepresentation& rep, const int eCode)
{
    if(eCode == OC_STACK_OK)
    {
        std::cout << "PUT request was successful" << std::endl;

        rep.getValue("state", mylight.m_state);
        rep.getValue("power", mylight.m_power);
        rep.getValue("name", mylight.m_name);

        std::cout << "\tstate: " << mylight.m_state << std::endl;
        std::cout << "\tpower: " << mylight.m_power << std::endl;
        std::cout << "\tname: " << mylight.m_name << std::endl;

        postLightRepresentation(curResource);
    }
    else
    {
        std::cout << "onPut Response error: " << eCode << std::endl;
        std::exit(-1);
    }
}

// Local function to put a different state for this resource
void putLightRepresentation(std::shared_ptr<OCResource> resource)
{
    if(resource)
    {
        OCRepresentation rep;

        std::cout << "Putting light representation..."<<std::endl;

        mylight.m_state = true;
        mylight.m_power = 15;

        rep.setValue("state", mylight.m_state);
        rep.setValue("power", mylight.m_power);

        // Invoke resource's put API with rep, query map and the callback parameter
        resource->put(rep, QueryParamsMap(), &onPut);
    }
}

// Callback handler on GET request
void onGet(const HeaderOptions& headerOptions, const OCRepresentation& rep, const int eCode)
{
    if(eCode == OC_STACK_OK)
    {
        std::cout << "GET request was successful" << std::endl;
        std::cout << "Resource URI: " << rep.getUri() << std::endl;

        rep.getValue("state", mylight.m_state);
        rep.getValue("power", mylight.m_power);
        rep.getValue("name", mylight.m_name);

        std::cout << "\tstate: " << mylight.m_state << std::endl;
        std::cout << "\tpower: " << mylight.m_power << std::endl;
        std::cout << "\tname: " << mylight.m_name << std::endl;

        putLightRepresentation(curResource);
    }
    else
    {
        std::cout << "onGET Response error: " << eCode << std::endl;
        std::exit(-1);
    }
}

// Local function to get representation of light resource
void getLightRepresentation(std::shared_ptr<OCResource> resource)
{
    if(resource)
    {
        std::cout << "Getting Light Representation..."<<std::endl;
        // Invoke resource's get API with the callback parameter

        QueryParamsMap test;
        resource->get(test, &onGet);
    }
}

// Callback to found resources
void foundResource(std::shared_ptr<OCResource> resource)
{
    if(curResource)
    {
        std::cout << "Found another resource, ignoring"<<std::endl;
    }

    std::string resourceURI;
    std::string hostAddress;
    try
    {
        // Do some operations with resource object.
        if(resource)
        {
            std::cout<<"DISCOVERED Resource:"<<std::endl;
            // Get the resource URI
            resourceURI = resource->uri();
            std::cout << "\tURI of the resource: " << resourceURI << std::endl;

            // Get the resource host address
            hostAddress = resource->host();
            std::cout << "\tHost address of the resource: " << hostAddress << std::endl;

            // Get the resource types
            std::cout << "\tList of resource types: " << std::endl;
            for(auto &resourceTypes : resource->getResourceTypes())
            {
                std::cout << "\t\t" << resourceTypes << std::endl;
            }

            // Get the resource interfaces
            std::cout << "\tList of resource interfaces: " << std::endl;
            for(auto &resourceInterfaces : resource->getResourceInterfaces())
            {
                std::cout << "\t\t" << resourceInterfaces << std::endl;
            }

            if(resourceURI == "/a/light")
            {
                curResource = resource;
                // Call a local function which will internally invoke get API on the resource pointer
                getLightRepresentation(resource);
            }
        }
        else
        {
            // Resource is invalid
            std::cout << "Resource is invalid" << std::endl;
        }

    }
    catch(std::exception& e)
    {
        //log(e.what());
    }
}

void PrintUsage()
{
    std::cout << std::endl;
    std::cout << "Usage : simpleclient <ObserveType>" << std::endl;
    std::cout << "   ObserveType : 1 - Observe" << std::endl;
    std::cout << "   ObserveType : 2 - ObserveAll" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 1)
    {
        OBSERVE_TYPE_TO_USE = ObserveType::Observe;
    }
    else if (argc == 2)
    {
        int value = atoi(argv[1]);
        if (value == 1)
            OBSERVE_TYPE_TO_USE = ObserveType::Observe;
        else if (value == 2)
            OBSERVE_TYPE_TO_USE = ObserveType::ObserveAll;
        else
            OBSERVE_TYPE_TO_USE = ObserveType::Observe;
    }
    else
    {
        PrintUsage();
        return -1;
    }

    // Create PlatformConfig object
    PlatformConfig cfg {
        OC::ServiceType::InProc,
        OC::ModeType::Client,
        "0.0.0.0",
        0,
        OC::QualityOfService::LowQos
    };

    OCPlatform::Configure(cfg);
    try
    {
        // makes it so that all boolean values are printed as 'true/false' in this stream
        std::cout.setf(std::ios::boolalpha);
        // Find all resources
        OCPlatform::findResource("", "coap://224.0.1.187/oc/core?rt=core.light", &foundResource);
        std::cout<< "Finding Resource... " <<std::endl;

        // A condition variable will free the mutex it is given, then do a non-
        // intensive block until 'notify' is called on it.  In this case, since we
        // don't ever call cv.notify, this should be a non-processor intensive version
        // of while(true);
        std::mutex blocker;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lock(blocker);
        cv.wait(lock);

    }catch(OCException& e)
    {
        //log(e.what());
    }

    return 0;
}

