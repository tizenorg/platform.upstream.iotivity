/******************************************************************
 *
 * Copyright 2015 Samsung Electronics All Rights Reserved.
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

#ifndef RECLIENT_H__
#define RECLIENT_H__

#include <string>

typedef void(*ClientMenuHandler)();
typedef int ReturnValue;

const std::string TEMPERATURE_URI = "/a/TempSensor";
const std::string LIGHT_URI = "/a/light";
const std::string TEMPERATURE_RT = "oic.r.temperaturesensor";
const std::string LIGHT_RT = "oic.r.light";
const std::string TEMPERATURE_AK = "Temperature";
const std::string LIGHT_AK = "Brightness";

void client_cb(void *data);

void *showClientAPIs(void *data);

#endif // RECLIENT_H__