/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include "unity.h"
#include "AppWebRTC.h"
#include "mock_Include.h"

static AppConfiguration mAppConfiguration;

/* Called before each test method. */
void setUp()
{
}

/* Called after each test method. */
void tearDown()
{
}

static PAppConfiguration getContext(void)
{
    return &mAppConfiguration;
}

void test_initWebRtc(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration pAppConfiguration = getContext();
    initKvsWebRtc_IgnoreAndReturn(STATUS_APP_WEBRTC_INIT);
    retStatus = initWebRtc(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_WEBRTC_INIT, retStatus);
    initKvsWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = initWebRtc(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_deinitWebRtc(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration pAppConfiguration = getContext();
    deinitKvsWebRtc_IgnoreAndReturn(STATUS_APP_WEBRTC_DEINIT);
    retStatus = deinitWebRtc(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_WEBRTC_DEINIT, retStatus);

    deinitKvsWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = deinitWebRtc(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}
