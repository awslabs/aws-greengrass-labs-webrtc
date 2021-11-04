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
#include "AppSignaling.h"
#include "mock_Include.h"

#define APP_SIGNALING_UTEST_TURN_SERVER "turns:12-345-678-89.t-abcdefgh.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp"
#define APP_SIGNALING_UTEST_USERNAME    "username"
#define APP_SIGNALING_UTEST_PASSWORD    "password"
#define APP_SIGNALING_UTEST_HANDLE      0x01

static AppCredential mAppCredential;
static AppSignaling mAppSignaling;
static IceConfigInfo mIceConfigInfo = {};
static SIGNALING_CLIENT_STATE mSignalingClientState;
static createMutex BackGlobalCreateMutex;

typedef struct {
    SIGNALING_CLIENT_STATE signalingClientState;
    PAppSignaling pAppSignaling;
    PIceConfigInfo pIceConfigInfo;
} AppSigMock, *PAppSigMock;

static AppSigMock mAppSigMock;

static PAppSigMock getAppSigMock(void)
{
    return &mAppSigMock;
}

/* Called before each test method. */
void setUp()
{
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling;
    PChannelInfo pChannelInfo;
    memset(pAppSigMock, 0, sizeof(AppSigMock));
    pAppSigMock->pAppSignaling = &mAppSignaling;
    pAppSignaling = pAppSigMock->pAppSignaling;
    pAppSignaling->pAppCredential = &mAppCredential;
    pAppSignaling->signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pAppSignaling->signalingSendMessageLock = INVALID_MUTEX_VALUE;

    pChannelInfo = &pAppSignaling->channelInfo;
    pChannelInfo->channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    pChannelInfo->pRegion = (PCHAR) "us-east-1";

    pAppSigMock->pIceConfigInfo = &mIceConfigInfo;
}

static MUTEX null_createMutex(BOOL reentrant)
{
    return NULL;
}

/* Called after each test method. */
void tearDown()
{
}

static STATUS signalingClientGetIceConfigInfoCount_callback(SIGNALING_CLIENT_HANDLE signalingClientHandle, PUINT32 pIceConfigCount)
{
    PAppSigMock pAppSigMock = getAppSigMock();
    PIceConfigInfo pIceConfigInfo = pAppSigMock->pIceConfigInfo;
    *pIceConfigCount = pIceConfigInfo->uriCount;
    return STATUS_SUCCESS;
}

static STATUS signalingClientGetIceConfigInfo_callback(SIGNALING_CLIENT_HANDLE signalingClientHandle, UINT32 index, PIceConfigInfo* ppIceConfigInfo)
{
    PAppSigMock pAppSigMock = getAppSigMock();
    *ppIceConfigInfo = pAppSigMock->pIceConfigInfo;
    return STATUS_SUCCESS;
}

static STATUS signalingClientGetCurrentState_callback(SIGNALING_CLIENT_HANDLE signalingClientHandle, PSIGNALING_CLIENT_STATE pState)
{
    PAppSigMock pAppSigMock = getAppSigMock();
    *pState = pAppSigMock->signalingClientState;
    return STATUS_SUCCESS;
}

static STATUS createSignalingClientSync_callback(PSignalingClientInfo pClientInfo, PChannelInfo pChannelInfo, PSignalingClientCallbacks pCallbacks,
                                                 PAwsCredentialProvider pCredentialProvider, PSIGNALING_CLIENT_HANDLE pSignalingHandle)
{
    *pSignalingHandle = APP_SIGNALING_UTEST_HANDLE + 1;
    return STATUS_SUCCESS;
}

void test_initAppSignaling_null_mutex(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;
    SIGNALING_CHANNEL_ROLE_TYPE channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN;
    PSignalingMessage message;

    BackGlobalCreateMutex = globalCreateMutex;
    globalCreateMutex = null_createMutex;

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_MUTEX, retStatus);

    retStatus = sendAppSignalingMessage(pAppSignaling, &message);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_MUTEX, retStatus);

    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    globalCreateMutex = BackGlobalCreateMutex;
}

void test_getAppSignalingRole(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;
    SIGNALING_CHANNEL_ROLE_TYPE channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN;

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    channelRoleType = getAppSignalingRole(pAppSignaling);
    TEST_ASSERT_EQUAL(channelRoleType, SIGNALING_CHANNEL_ROLE_TYPE_MASTER);

    pAppSignaling->channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    channelRoleType = getAppSignalingRole(pAppSignaling);
    TEST_ASSERT_EQUAL(channelRoleType, SIGNALING_CHANNEL_ROLE_TYPE_VIEWER);

    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_queryAppSignalingServer(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;
    PIceConfigInfo pIceConfigInfo = pAppSigMock->pIceConfigInfo;
    RtcConfiguration rtcConfiguration;
    UINT32 uriCount = MAX_ICE_SERVERS_COUNT;

    pIceConfigInfo->uriCount = 0;
    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, FALSE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    signalingClientGetIceConfigInfoCount_StubWithCallback(signalingClientGetIceConfigInfoCount_callback);
    signalingClientGetIceConfigInfo_StubWithCallback(signalingClientGetIceConfigInfo_callback);
    retStatus = queryAppSignalingServer(pAppSignaling, rtcConfiguration.iceServers, &uriCount);
    TEST_ASSERT_EQUAL(1, uriCount);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    signalingClientGetIceConfigInfoCount_StubWithCallback(signalingClientGetIceConfigInfoCount_callback);
    signalingClientGetIceConfigInfo_StubWithCallback(signalingClientGetIceConfigInfo_callback);
    retStatus = queryAppSignalingServer(pAppSignaling, rtcConfiguration.iceServers, &uriCount);
    TEST_ASSERT_EQUAL(1, uriCount);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pIceConfigInfo->version = 0;
    pIceConfigInfo->ttl = 3000000000;
    pIceConfigInfo->uriCount = 1;
    memcpy(pIceConfigInfo->uris, APP_SIGNALING_UTEST_TURN_SERVER, sizeof(APP_SIGNALING_UTEST_TURN_SERVER));
    memcpy(pIceConfigInfo->userName, APP_SIGNALING_UTEST_USERNAME, sizeof(APP_SIGNALING_UTEST_USERNAME));
    memcpy(pIceConfigInfo->password, APP_SIGNALING_UTEST_PASSWORD, sizeof(APP_SIGNALING_UTEST_PASSWORD));
    signalingClientGetIceConfigInfoCount_StubWithCallback(signalingClientGetIceConfigInfoCount_callback);
    signalingClientGetIceConfigInfo_StubWithCallback(signalingClientGetIceConfigInfo_callback);
    retStatus = queryAppSignalingServer(pAppSignaling, rtcConfiguration.iceServers, &uriCount);
    TEST_ASSERT_EQUAL(2, uriCount);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    signalingClientGetIceConfigInfoCount_IgnoreAndReturn(STATUS_APP_SIGNALING_INVALID_INFO_COUNT);
    signalingClientGetIceConfigInfo_StubWithCallback(signalingClientGetIceConfigInfo_callback);
    retStatus = queryAppSignalingServer(pAppSignaling, rtcConfiguration.iceServers, &uriCount);
    TEST_ASSERT_EQUAL(1, uriCount);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_INFO_COUNT, retStatus);

    signalingClientGetIceConfigInfoCount_StubWithCallback(signalingClientGetIceConfigInfoCount_callback);
    signalingClientGetIceConfigInfo_IgnoreAndReturn(STATUS_APP_SIGNALING_INVALID_INFO);
    retStatus = queryAppSignalingServer(pAppSignaling, rtcConfiguration.iceServers, &uriCount);
    TEST_ASSERT_EQUAL(1, uriCount);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_INFO, retStatus);

    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_connectAppSignaling(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    createSignalingClientSync_IgnoreAndReturn(STATUS_APP_SIGNALING_CREATE);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_CREATE, retStatus);

    createSignalingClientSync_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_APP_SIGNALING_CONNECT);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_CONNECT, retStatus);

    signalingClientConnectSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_checkAppSignaling(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = checkAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_HANDLE, retStatus);

    createSignalingClientSync_StubWithCallback(createSignalingClientSync_callback);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    signalingClientGetCurrentState_IgnoreAndReturn(STATUS_APP_SIGNALING_NOT_READY);
    retStatus = checkAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_NOT_READY, retStatus);

    pAppSigMock->signalingClientState = SIGNALING_CLIENT_STATE_UNKNOWN;
    signalingClientGetCurrentState_StubWithCallback(signalingClientGetCurrentState_callback);
    retStatus = checkAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSigMock->signalingClientState = SIGNALING_CLIENT_STATE_READY;
    signalingClientGetCurrentState_StubWithCallback(signalingClientGetCurrentState_callback);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_APP_SIGNALING_CONNECT);
    retStatus = checkAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_CONNECT, retStatus);

    signalingClientGetCurrentState_StubWithCallback(signalingClientGetCurrentState_callback);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = checkAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeSignalingClient_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_sendAppSignalingMessage(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;
    PSignalingMessage message;

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = sendAppSignalingMessage(NULL, &message);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_NULL_ARG, retStatus);

    retStatus = sendAppSignalingMessage(pAppSignaling, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_NULL_ARG, retStatus);

    retStatus = sendAppSignalingMessage(pAppSignaling, &message);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_HANDLE, retStatus);

    createSignalingClientSync_StubWithCallback(createSignalingClientSync_callback);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    signalingClientSendMessageSync_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = sendAppSignalingMessage(pAppSignaling, &message);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_SEND, retStatus);

    signalingClientSendMessageSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = sendAppSignalingMessage(pAppSignaling, &message);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeSignalingClient_ExpectAnyArgsAndReturn(STATUS_SUCCESS);
    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_restartAppSignaling(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;

    freeSignalingClient_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = restartAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_RESTART, retStatus);

    freeSignalingClient_IgnoreAndReturn(STATUS_SUCCESS);
    createSignalingClientSync_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = restartAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_RESTART, retStatus);

    createSignalingClientSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = restartAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_freeAppSignaling(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppSigMock pAppSigMock = getAppSigMock();
    PAppSignaling pAppSignaling = pAppSigMock->pAppSignaling;

    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    createSignalingClientSync_StubWithCallback(createSignalingClientSync_callback);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeSignalingClient_IgnoreAndReturn(STATUS_APP_SIGNALING_FREE);
    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_FREE, retStatus);

    retStatus = initAppSignaling(pAppSignaling, NULL, NULL, NULL, NULL, TRUE);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    createSignalingClientSync_StubWithCallback(createSignalingClientSync_callback);
    signalingClientConnectSync_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = connectAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    freeSignalingClient_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeAppSignaling(pAppSignaling);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}
