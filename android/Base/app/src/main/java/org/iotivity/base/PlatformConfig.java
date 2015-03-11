//******************************************************************
//
// Copyright 2014 MediaTek All Rights Reserved.
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

package org.iotivity.base;

public class PlatformConfig extends NativeInstance
{
    final private static String TAG = "PlatformConfig";
    static {
        System.loadLibrary("ocstack-jni");
    }

    // ENUM definition. Need to match with OCApi.h
    public class ServiceType
    {
        static final public int INPROC  = 0;
        static final public int OUTPROC = 1;
    };

    public class ModeType
    {
        static final public int SERVER = 0;
        static final public int CLIENT = 1;
        static final public int BOTH   = 2;

    };

    public class QualityOfService
    {
        static final public int LO_QOS  = 0;
        static final public int ME_QOS  = 1;
        static final public int HI_QOS  = 2;
        static final public int NA_QOS  = 3;
    }

    public PlatformConfig(int serviceType,
                          int mode,
                          String ipAddress,
                          int port,
                          int QoS)
    {
        super.nativeHandle = createNativeInstance(serviceType, mode, ipAddress, port, QoS);
    }
   

    protected native long createNativeInstance(int serviceType,
             int mode,
             String ipAddress,
             int port,
             int QoS);
}
