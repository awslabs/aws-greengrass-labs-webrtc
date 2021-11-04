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
#include "AppMetrics.h"
#include "mock_Include.h"
#include "mock_AppMetricsWrap.h"

#define APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_VERBOSE "1"
#define APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_INVALID "15"
#define APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_ZERO    "0"
#define APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_HEX     "0x01"
#define APP_METRICS_UTEST_ENABLE_FILE_LOGGING             ""

static RtcPeerConnection mRtcPeerConnection;
static RtcStatsObject mRtcStatsObject;
static STATUS callBackRetStatus;

static PRtcPeerConnection getPeerConnectionContext(void)
{
    return &mRtcPeerConnection;
}

static PRtcStatsObject getRtcStatsObject(void)
{
    return &mRtcStatsObject;
}

static STATUS rtcPeerConnectionGetMetrics_callBack(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver,
                                                   PRtcStats pRtcMetrics, int NumCalls)
{
    PRtcStatsObject pRtcStatsObject = getRtcStatsObject();
    memcpy(&pRtcMetrics->rtcStatsObject, pRtcStatsObject, sizeof(RtcStatsObject));

    if (NumCalls == 1) {
        return STATUS_APP_METRICS_LOCAL_ICE_CANDIDATE;
    }
    return STATUS_SUCCESS;
}

/* Called before each test method. */
void setUp()
{
}

/* Called after each test method. */
void tearDown()
{
}

void test_getLogLevel(void)
{
    UINT32 logLevel;

    unsetenv(DEBUG_LOG_LEVEL_ENV_VAR);
    logLevel = getLogLevel();
    TEST_ASSERT_EQUAL(LOG_LEVEL_WARN, logLevel);

    setenv(DEBUG_LOG_LEVEL_ENV_VAR, APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_VERBOSE, 1);
    logLevel = getLogLevel();
    TEST_ASSERT_EQUAL(LOG_LEVEL_VERBOSE, logLevel);
    unsetenv(DEBUG_LOG_LEVEL_ENV_VAR);

    setenv(DEBUG_LOG_LEVEL_ENV_VAR, APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_INVALID, 1);
    logLevel = getLogLevel();
    TEST_ASSERT_EQUAL(LOG_LEVEL_WARN, logLevel);
    unsetenv(DEBUG_LOG_LEVEL_ENV_VAR);

    setenv(DEBUG_LOG_LEVEL_ENV_VAR, APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_ZERO, 1);
    logLevel = getLogLevel();
    TEST_ASSERT_EQUAL(LOG_LEVEL_WARN, logLevel);
    unsetenv(DEBUG_LOG_LEVEL_ENV_VAR);

    setenv(DEBUG_LOG_LEVEL_ENV_VAR, APP_METRICS_UTEST_DEBUG_LOG_LEVEL_ENV_VAR_HEX, 1);
    logLevel = getLogLevel();
    TEST_ASSERT_EQUAL(LOG_LEVEL_WARN, logLevel);
    unsetenv(DEBUG_LOG_LEVEL_ENV_VAR);
}

void test_setupFileLogging(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL enable;
    unsetenv(ENABLE_FILE_LOGGING);
    retStatus = setupFileLogging(&enable);
    TEST_ASSERT_EQUAL(FALSE, enable);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    setenv(ENABLE_FILE_LOGGING, APP_METRICS_UTEST_ENABLE_FILE_LOGGING, 1);
    createAppFileLogger_IgnoreAndReturn(STATUS_APP_METRICS_SETUP_LOGGER);
    retStatus = setupFileLogging(&enable);
    TEST_ASSERT_EQUAL(FALSE, enable);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_SETUP_LOGGER, retStatus);

    setenv(ENABLE_FILE_LOGGING, APP_METRICS_UTEST_ENABLE_FILE_LOGGING, 1);
    createAppFileLogger_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = setupFileLogging(&enable);
    TEST_ASSERT_EQUAL(TRUE, enable);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_closeFileLogging(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    freeAppFileLogger_IgnoreAndReturn(STATUS_APP_METRICS_FREE_LOGGER);
    retStatus = closeFileLogging();
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_FREE_LOGGER, retStatus);
    freeAppFileLogger_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = closeFileLogging();
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_logIceServerStats(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtcPeerConnection pRtcPeerConnection = getPeerConnectionContext();
    PRtcStatsObject pRtcStatsObject = getRtcStatsObject();

    retStatus = logIceServerStats(NULL, 0);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_NULL_ARG, retStatus);

    rtcPeerConnectionGetMetrics_IgnoreAndReturn(STATUS_APP_METRICS_ICE_SERVER);
    retStatus = logIceServerStats(pRtcPeerConnection, 0);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_ICE_SERVER, retStatus);

    rtcPeerConnectionGetMetrics_IgnoreAndReturn(STATUS_SUCCESS);
    memcpy(pRtcStatsObject->iceServerStats.url, "192.168.0.1", strlen("192.168.0.1"));
    pRtcStatsObject->iceServerStats.port = 1234;
    memcpy(pRtcStatsObject->iceServerStats.protocol, "udp", strlen("udp"));
    pRtcStatsObject->iceServerStats.totalRequestsSent = 10;
    pRtcStatsObject->iceServerStats.totalResponsesReceived = 10;
    pRtcStatsObject->iceServerStats.totalRoundTripTime = 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    retStatus = logIceServerStats(pRtcPeerConnection, 0);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_logSelectedIceCandidatesInformation(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtcPeerConnection pRtcPeerConnection = getPeerConnectionContext();
    PRtcStatsObject pRtcStatsObject = getRtcStatsObject();

    retStatus = logSelectedIceCandidatesInformation(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_NULL_ARG, retStatus);

    rtcPeerConnectionGetMetrics_IgnoreAndReturn(STATUS_APP_METRICS_LOCAL_ICE_CANDIDATE);
    retStatus = logSelectedIceCandidatesInformation(pRtcPeerConnection);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_LOCAL_ICE_CANDIDATE, retStatus);

    memcpy(pRtcStatsObject->localIceCandidateStats.address, "192.168.0.1", strlen("192.168.0.1"));
    memcpy(pRtcStatsObject->localIceCandidateStats.candidateType, "host", strlen("host"));

    pRtcStatsObject->localIceCandidateStats.port = 1234;
    pRtcStatsObject->localIceCandidateStats.priority = 5678;
    memcpy(pRtcStatsObject->localIceCandidateStats.protocol, "udp", strlen("udp"));
    memcpy(pRtcStatsObject->localIceCandidateStats.relayProtocol, "udp", strlen("udp"));
    memcpy(pRtcStatsObject->localIceCandidateStats.url, "192.168.0.2", strlen("192.168.0.2"));
    memcpy(pRtcStatsObject->remoteIceCandidateStats.address, "192.168.0.1", strlen("192.168.0.1"));
    memcpy(pRtcStatsObject->remoteIceCandidateStats.candidateType, "host", strlen("host"));
    pRtcStatsObject->remoteIceCandidateStats.port = 1234;
    pRtcStatsObject->remoteIceCandidateStats.priority = 5678;
    memcpy(pRtcStatsObject->remoteIceCandidateStats.protocol, "udp", strlen("udp"));
    rtcPeerConnectionGetMetrics_StubWithCallback(rtcPeerConnectionGetMetrics_callBack);
    retStatus = logSelectedIceCandidatesInformation(pRtcPeerConnection);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_REMOTE_ICE_CANDIDATE, retStatus);

    retStatus = logSelectedIceCandidatesInformation(pRtcPeerConnection);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

static SignalingClientMetrics mSignalingClientMetrics;

PSignalingClientMetrics getSignalingContext(void)
{
    return &mSignalingClientMetrics;
}

void test_logSignalingClientStats(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClientMetrics pSignalingClientMetrics = getSignalingContext();
    retStatus = logSignalingClientStats(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_METRICS_NULL_ARG, retStatus);

    pSignalingClientMetrics->signalingClientStats.connectionDuration = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pSignalingClientMetrics->signalingClientStats.numberOfErrors = 0;
    pSignalingClientMetrics->signalingClientStats.numberOfRuntimeErrors = 0;
    pSignalingClientMetrics->signalingClientStats.connectionDuration = 80 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pSignalingClientMetrics->signalingClientStats.cpApiCallLatency = 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pSignalingClientMetrics->signalingClientStats.dpApiCallLatency = 20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    retStatus = logSignalingClientStats(pSignalingClientMetrics);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}
