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



extern "C" {
    #include "ocrandom.h"
}

#include "gtest/gtest.h"
#include "math.h"

#define ARR_SIZE (20)

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(RandomGeneration,OCSeedRandom) {
    EXPECT_EQ((uint32_t )0, OCSeedRandom());
}

TEST(RandomGeneration,OCGetRandomByte) {
    uint8_t value = OCGetRandomByte();
    EXPECT_LE((uint8_t )0, value);
    EXPECT_GT(pow(2, 8), value);
}

TEST(RandomGeneration,OCGetRandom) {
    uint32_t value = OCGetRandom();
    EXPECT_LE((uint8_t )0, value);
    EXPECT_GT(pow(2, 32), value);
}

TEST(RandomGeneration,OCFillRandomMem) {
    uint8_t array[ARR_SIZE];
    memset(array, 0, ARR_SIZE);
    OCFillRandomMem(array + 1, ARR_SIZE - 2);

    for (int i = 1; i <= ARR_SIZE - 2; i++) {
        uint8_t value = array[i];
        EXPECT_LE((uint8_t )0, value);
        EXPECT_GT(pow(2, 8), value);
    }
    EXPECT_EQ((uint8_t )0, array[0]);
    EXPECT_EQ((uint8_t )0, array[ARR_SIZE - 1]);
}

