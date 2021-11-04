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
#include "AppDataChannel.h"
#include "mock_Include.h"

/* Called before each test method. */
void setUp()
{
}

/* Called after each test method. */
void tearDown()
{
}

#define APP_DATA_CHANNEL_UTEST_CHANNEL_NAME "test_channel"
#define APP_DATA_CHANNEL_UTEST_DATA_MESSAGE "test_message"
static RtcOnMessage mRtcOnMessage;
static RtcDataChannel mRtcDataChannel;

static PRtcDataChannel getContext(void)
{
    return &mRtcDataChannel;
}

static void triggerCallback(UINT64 userData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    return &mRtcDataChannel;
}

static STATUS dataChannelOnMessageCallback(PRtcDataChannel pRtcDataChannel, UINT64 userData, RtcOnMessage rtcOnMessage)
{
    mRtcOnMessage = rtcOnMessage;
    return STATUS_SUCCESS;
}

void test_onDataChannel(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtcDataChannel pRtcDataChannel = getContext();
    PBYTE message[64];
    UINT32 messageLen = strlen(APP_DATA_CHANNEL_UTEST_DATA_MESSAGE);
    memcpy(message, APP_DATA_CHANNEL_UTEST_DATA_MESSAGE, messageLen);
    memcpy(pRtcDataChannel->name, APP_DATA_CHANNEL_UTEST_CHANNEL_NAME, strlen(APP_DATA_CHANNEL_UTEST_CHANNEL_NAME));
    dataChannelOnMessage_StubWithCallback(dataChannelOnMessageCallback);
    onDataChannel(NULL, pRtcDataChannel);
    mRtcOnMessage(NULL, pRtcDataChannel, TRUE, NULL, 0);
    mRtcOnMessage(NULL, pRtcDataChannel, FALSE, message, messageLen);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}