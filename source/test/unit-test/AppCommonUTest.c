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
#include "AppCommon.h"
#include "mock_Include.h"
#include "mock_AppSignaling.h"
#include "mock_AppCredential.h"
#include "mock_AppMetrics.h"
#include "mock_AppWebRTC.h"
#include "mock_AppRtspSrc.h"
#include "mock_AppHashTableWrap.h"
#include "mock_AppMessageQueue.h"
#include "mock_AppTimerWrap.h"

#define APP_COMMON_UTEST_FIXTURE_CHANNEL_NAME                 "AppCommonUtestFixtureChannel"
#define APP_COMMON_UTEST_MASTER_CLIENT_ID                     "AppCommonUtestMaster"
#define APP_COMMON_UTEST_VIEWER_CLIENT_ID                     "AppCommonUtestViewer"
#define APP_COMMON_UTEST_OFFER_PAYLOAD                        "offer_payload"
#define APP_COMMON_UTEST_ANSWER_PAYLOAD                       "answer_payload"
#define APP_COMMON_UTEST_ICE_CANDIDATE_PAYLOAD                "ice_candidate_payload"
#define APP_COMMON_UTEST_SIGNALING_CLIENT_STATE_CONNECTED_STR "Connected"
#define APP_COMMON_UTEST_SIGNALING_ERROR_MSG_LEN              256
#define APP_COMMON_UTEST_CANDIDATE_JSON_STRING                "candidate"
#define APP_COMMON_UTEST_CANDIDATE_JSON_LEN                   1024
#define APP_COMMON_UTEST_REGION_ENV_VAR                       "us-west-1"

typedef STATUS (*TimerQueueCallback)(UINT32, UINT64, UINT64);

typedef struct {
    CHAR channelName[128];
    SIGNALING_CHANNEL_ROLE_TYPE roleType;
    BOOL trickleIce;
    BOOL useTurn;
    PRtcCertificate pRtcCertificate;
    PHashTable pRemoteRtcPeerConnections;
    PConnectionMsgQ pConnectionMsgQ;
    PPendingMessageQueue pPendingMsgQ;

    UINT64 receivedSignalingMessage;

    PRtcStats pRtcIceCandidatePairMetrics;
    PStreamingSession pStreamingSession;

    UINT64 onIceCandidateUData;
    RtcOnIceCandidate rtcOnIceCandidateHandler;

    UINT64 getIceCandidatePairStatsCallbackUserData;
    TimerQueueCallback getIceCandidatePairStatsCallback;

    UINT64 pregenerateCertTimerCallbackUserData;
    TimerQueueCallback pregenerateCertTimerCallback;

    MediaSinkHook mediaSinkHook;
    PVOID mediaSinkHookUdata;

    MediaEosHook mediaEosHook;
    PVOID mediaEosHookUdata;

    UINT64 rtcOnConnectionStateChangeUData;
    RtcOnConnectionStateChange rtcOnConnectionStateChange;

    UINT64 rtcOnBandwidthEstimationUData;
    RtcOnBandwidthEstimation rtcOnBandwidthEstimation;

    UINT64 rtcOnSenderBandwidthEstimationUData;
    RtcOnSenderBandwidthEstimation rtcOnSenderBandwidthEstimation;
} AppCommonMock, *PAppCommonMock;

static AppCommonMock mAppCommonMock;
static RtcCertificate mRtcCertificate;
static HashTable mRemoteRtcPeerConnections;
static RtcStats mRtcIceCandidatePairMetrics;
static ConnectionMsgQ mConnectionMsgQ;
static PendingMessageQueue mPendingMsgQ;
static memCalloc BackGlobalMemCalloc;
static createMutex BackGlobalCreateMutex;

static PAppCommonMock getAppCommonMock(void)
{
    return &mAppCommonMock;
}

/* Called before each test method. */
void setUp()
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    memset(pAppCommonMock, 0, sizeof(AppCommonMock));
    memcpy(pAppCommonMock->channelName, APP_COMMON_UTEST_FIXTURE_CHANNEL_NAME, strlen(APP_COMMON_UTEST_FIXTURE_CHANNEL_NAME));
    pAppCommonMock->roleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    pAppCommonMock->trickleIce = TRUE;
    pAppCommonMock->useTurn = TRUE;
    pAppCommonMock->pRtcCertificate = &mRtcCertificate;
    pAppCommonMock->pRtcIceCandidatePairMetrics = &mRtcIceCandidatePairMetrics;
}

/* Called after each test method. */
void tearDown()
{
}

static STATUS appHashTableCreateWithParams_callback(UINT32 bucketCount, UINT32 bucketLength, PHashTable* ppHashTable)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->pRemoteRtcPeerConnections = &mRemoteRtcPeerConnections;
    return STATUS_SUCCESS;
}

static STATUS appHashTableContains_yes_callback(PHashTable pHashTable, UINT64 key, PBOOL pContains)
{
    *pContains = TRUE;
    return STATUS_SUCCESS;
}

static STATUS appHashTableContains_no_callback(PHashTable pHashTable, UINT64 key, PBOOL pContains)
{
    *pContains = FALSE;
    return STATUS_SUCCESS;
}

static STATUS appHashTableGet_callback(PHashTable pHashTable, UINT64 key, PUINT64 pValue)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    *pValue = pAppCommonMock->pStreamingSession;
    return STATUS_SUCCESS;
}

static STATUS appHashTableCreateWithParams_pic_callback(UINT32 bucketCount, UINT32 bucketLength, PHashTable* ppHashTable)
{
    return hashTableCreateWithParams(bucketCount, bucketLength, ppHashTable);
}

static STATUS appHashTableContains_pic_callback(PHashTable pHashTable, UINT64 key, PBOOL pContains)
{
    return hashTableContains(pHashTable, key, pContains);
}

static STATUS appHashTableGet_pic_callback(PHashTable pHashTable, UINT64 key, PUINT64 pValue)
{
    return hashTableGet(pHashTable, key, pValue);
}

static STATUS appHashTablePut_pic_callback(PHashTable pHashTable, UINT64 key, UINT64 value)
{
    return hashTablePut(pHashTable, key, value);
}

static STATUS appHashTableRemove_pic_callback(PHashTable pHashTable, UINT64 key)
{
    return hashTableRemove(pHashTable, key);
}

static STATUS appHashTableClear_pic_callback(PHashTable pHashTable)
{
    return hashTableClear(pHashTable);
}

static STATUS appHashTableFree_pic_callback(PHashTable pHashTable)
{
    return hashTableFree(pHashTable);
}

static STATUS createConnectionMsqQ_success_callback(PConnectionMsgQ* ppConnectionMsgQ)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->pConnectionMsgQ = &mConnectionMsgQ;
    *ppConnectionMsgQ = pAppCommonMock->pConnectionMsgQ;
    return STATUS_SUCCESS;
}

static STATUS createPendingMsgQ_callback(PConnectionMsgQ pConnectionMsgQ, UINT64 hashValue, PPendingMessageQueue* ppPendingMessageQueue)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->pPendingMsgQ = &mPendingMsgQ;
    *ppPendingMessageQueue = pAppCommonMock->pPendingMsgQ;
    return STATUS_SUCCESS;
}

static STATUS getPendingMsgQByHashVal_callback(PConnectionMsgQ pConnectionMsgQ, UINT64 clientHash, BOOL remove, PPendingMessageQueue* ppPendingMsgQ)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    *ppPendingMsgQ = pAppCommonMock->pPendingMsgQ;
    return STATUS_SUCCESS;
}

static STATUS appTimerQueueCreate_callback(PTIMER_QUEUE_HANDLE pHandle)
{
    *pHandle = 1;
    return STATUS_SUCCESS;
}

static STATUS appTimeQueueAdd_getIceCandidatePairStats_callback(TIMER_QUEUE_HANDLE handle, UINT64 start, UINT64 period,
                                                                TimerCallbackFunc timerCallbackFn, UINT64 customData, PUINT32 pIndex)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->getIceCandidatePairStatsCallback = timerCallbackFn;
    pAppCommonMock->getIceCandidatePairStatsCallbackUserData = customData;
    *pIndex = 1;
    return STATUS_SUCCESS;
}

static STATUS appTimeQueueAdd_pregenerateCertTimer_callback(TIMER_QUEUE_HANDLE handle, UINT64 start, UINT64 period, TimerCallbackFunc timerCallbackFn,
                                                            UINT64 customData, PUINT32 pIndex)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->pregenerateCertTimerCallback = timerCallbackFn;
    pAppCommonMock->pregenerateCertTimerCallbackUserData = customData;
    *pIndex = 1;
    return STATUS_SUCCESS;
}

static STATUS initAppSignaling_callback(PAppSignaling pAppSignaling, SignalingClientMessageReceivedFunc onMessageReceived,
                                        SignalingClientStateChangedFunc onStateChanged, SignalingClientErrorReportFunc pOnError, UINT64 udata,
                                        BOOL useTurn)
{
    pAppSignaling->signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pAppSignaling->useTurn = useTurn;
    pAppSignaling->signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    pAppSignaling->signalingClientCallbacks.messageReceivedFn = onMessageReceived;
    pAppSignaling->signalingClientCallbacks.stateChangeFn = onStateChanged;
    pAppSignaling->signalingClientCallbacks.errorReportFn = pOnError;
    pAppSignaling->signalingClientCallbacks.customData = (UINT64) udata;
    return STATUS_SUCCESS;
}

static STATUS queryAppSignalingServer_callback(PAppSignaling pAppSignaling, PRtcIceServer pIceServer, PUINT32 pServerNum)
{
    *pServerNum = 1;
    return STATUS_SUCCESS;
}

static void create_signaling_message(PReceivedSignalingMessage pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE signaling_message_type, UINT32 id)
{
    memset(pReceivedSignalingMessage, 0, sizeof(ReceivedSignalingMessage));
    pReceivedSignalingMessage->signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    pReceivedSignalingMessage->signalingMessage.messageType = signaling_message_type;
    snprintf(pReceivedSignalingMessage->signalingMessage.peerClientId, MAX_SIGNALING_CLIENT_ID_LEN + 1, "%s%d", APP_COMMON_UTEST_VIEWER_CLIENT_ID,
             id);

    switch (signaling_message_type) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            pReceivedSignalingMessage->signalingMessage.payloadLen = strlen(APP_COMMON_UTEST_OFFER_PAYLOAD);
            memcpy(pReceivedSignalingMessage->signalingMessage.payload, APP_COMMON_UTEST_OFFER_PAYLOAD,
                   pReceivedSignalingMessage->signalingMessage.payloadLen);
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            pReceivedSignalingMessage->signalingMessage.payloadLen = strlen(APP_COMMON_UTEST_ANSWER_PAYLOAD);
            memcpy(pReceivedSignalingMessage->signalingMessage.payload, APP_COMMON_UTEST_ANSWER_PAYLOAD,
                   pReceivedSignalingMessage->signalingMessage.payloadLen);
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            pReceivedSignalingMessage->signalingMessage.payloadLen = strlen(APP_COMMON_UTEST_ICE_CANDIDATE_PAYLOAD);
            memcpy(pReceivedSignalingMessage->signalingMessage.payload, APP_COMMON_UTEST_ICE_CANDIDATE_PAYLOAD,
                   pReceivedSignalingMessage->signalingMessage.payloadLen);
            break;
        default:
            break;
    }
}

static STATUS signalingClientGetStateString_callback(SIGNALING_CLIENT_STATE state, PCHAR* ppStateStr)
{
    *ppStateStr = APP_COMMON_UTEST_SIGNALING_CLIENT_STATE_CONNECTED_STR;
    return STATUS_SUCCESS;
}

static void prepareRtcMetrics(PAppConfiguration pAppConfiguration)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PRtcStats pRtcIceCandidatePairMetrics = pAppCommonMock->pRtcIceCandidatePairMetrics;
    UINT32 i;

    for (i = 0; i < pAppConfiguration->streamingSessionCount; ++i) {
        if (i == 0) {
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevTs = 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsSent = 0;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsReceived = 0;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesSent = 0;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesReceived = 0;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevPacketsDiscardedOnSend = 0;
        } else {
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevTs =
                pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevTs + 1 * HUNDREDS_OF_NANOS_IN_A_SECOND;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsSent =
                pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfPacketsSent + 1;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsReceived =
                pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfPacketsReceived + 1;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesSent =
                pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfBytesSent + 1;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesReceived =
                pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfBytesReceived + 1;
            pAppConfiguration->streamingSessionList[i]->rtcMetricsHistory.prevPacketsDiscardedOnSend =
                pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevPacketsDiscardedOnSend + 1;
        }
    }
    memset(pAppCommonMock->pRtcIceCandidatePairMetrics, 0, sizeof(RtcStats));
    pRtcIceCandidatePairMetrics->timestamp =
        pAppConfiguration->streamingSessionList[i / 2]->rtcMetricsHistory.prevTs + 1 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    memcpy(pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.localCandidateId, "local", strlen("local"));
    memcpy(pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.remoteCandidateId, "remote", strlen("remote"));
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.nominated = FALSE;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.packetsSent =
        pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfPacketsSent + 1;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.packetsReceived =
        pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfPacketsReceived + 1;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.bytesSent =
        pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfBytesSent + 1;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.bytesReceived =
        pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevNumberOfBytesReceived + 1;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.packetsDiscardedOnSend =
        pAppConfiguration->streamingSessionList[i - 1]->rtcMetricsHistory.prevPacketsDiscardedOnSend + 1;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.currentRoundTripTime = 10;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.responsesReceived = 10;
}

static STATUS rtcPeerConnectionGetMetrics_callback(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver,
                                                   PRtcStats pRtcMetrics)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PRtcStats pRtcIceCandidatePairMetrics = pAppCommonMock->pRtcIceCandidatePairMetrics;
    pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.nominated =
        pRtcIceCandidatePairMetrics->rtcStatsObject.iceCandidatePairStats.nominated == TRUE ? FALSE : TRUE;
    memcpy(pRtcMetrics, pAppCommonMock->pRtcIceCandidatePairMetrics, sizeof(RtcStats));
    return STATUS_SUCCESS;
}

static STATUS logIceServerStats_callback(PRtcPeerConnection pRtcPeerConnection, UINT32 index, int NumCalls)
{
    if (NumCalls % 5 == 0) {
        return STATUS_APP_METRICS_NULL_ARG;
    } else {
        return STATUS_SUCCESS;
    }
}

static STATUS popGeneratedCert_existed_callback(PAppCredential pAppCredential, PRtcCertificate* ppRtcCertificate)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    *ppRtcCertificate = pAppCommonMock->pRtcCertificate;
    return STATUS_SUCCESS;
}

static STATUS popGeneratedCert_empty_callback(PAppCredential pAppCredential, PRtcCertificate* ppRtcCertificate)
{
    *ppRtcCertificate = NULL;
    return STATUS_SUCCESS;
}

static STATUS addSupportedCodec_callback(PRtcPeerConnection pPeerConnection, RTC_CODEC rtcCodec, int NumCalls)
{
    if (NumCalls == 1)
        return STATUS_NULL_ARG;
    return STATUS_SUCCESS;
}

static STATUS addTransceiver_callback(PRtcPeerConnection pPeerConnection, PRtcMediaStreamTrack pRtcMediaStreamTrack,
                                      PRtcRtpTransceiverInit pRtcRtpTransceiverInit, PRtcRtpTransceiver* ppRtcRtpTransceiver, int NumCalls)
{
    if (NumCalls == 1)
        return STATUS_NULL_ARG;
    return STATUS_SUCCESS;
}

static STATUS transceiverOnBandwidthEstimation_return_callback(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData,
                                                               RtcOnBandwidthEstimation rtcOnBandwidthEstimation, int NumCalls)
{
    if (NumCalls == 1)
        return STATUS_NULL_ARG;
    return STATUS_SUCCESS;
}

static STATUS peerConnectionOnIceCandidate_callback(PRtcPeerConnection pRtcPeerConnection, UINT64 userData, RtcOnIceCandidate rtcOnIceCandidate)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->rtcOnIceCandidateHandler = rtcOnIceCandidate;
    pAppCommonMock->onIceCandidateUData = userData;
    return STATUS_SUCCESS;
}

static STATUS peerConnectionOnConnectionStateChange_callback(PRtcPeerConnection pRtcPeerConnection, UINT64 customData,
                                                             RtcOnConnectionStateChange rtcOnConnectionStateChange)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->rtcOnConnectionStateChange = rtcOnConnectionStateChange;
    pAppCommonMock->rtcOnConnectionStateChangeUData = customData;
    return STATUS_SUCCESS;
}

static STATUS transceiverOnBandwidthEstimation_callback(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData,
                                                        RtcOnBandwidthEstimation rtcOnBandwidthEstimation)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->rtcOnBandwidthEstimation = rtcOnBandwidthEstimation;
    pAppCommonMock->rtcOnBandwidthEstimationUData = customData;
    return STATUS_SUCCESS;
}

static STATUS peerConnectionOnSenderBandwidthEstimation_callback(PRtcPeerConnection pRtcPeerConnection, UINT64 customData,
                                                                 RtcOnSenderBandwidthEstimation rtcOnSenderBandwidthEstimation)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->rtcOnSenderBandwidthEstimation = rtcOnSenderBandwidthEstimation;
    pAppCommonMock->rtcOnSenderBandwidthEstimationUData = customData;
    return STATUS_SUCCESS;
}

static STATUS linkMeidaSinkHook_callback(PMediaContext pMediaContext, MediaSinkHook mediaSinkHook, PVOID udata)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->mediaSinkHook = mediaSinkHook;
    pAppCommonMock->mediaSinkHookUdata = udata;
    return STATUS_SUCCESS;
}

static STATUS linkMeidaEosHook_callback(PMediaContext pMediaContext, MediaEosHook mediaEosHook, PVOID udata)
{
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    pAppCommonMock->mediaEosHook = mediaEosHook;
    pAppCommonMock->mediaEosHookUdata = udata;
    return STATUS_SUCCESS;
}

static PVOID runMediaSource_callback(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    return (PVOID)(ULONG_PTR) retStatus;
}

static STATUS setupFileLogging_callback(PBOOL pEnable)
{
    *pEnable = TRUE;
    return STATUS_SUCCESS;
}

static PVOID null_memCalloc(SIZE_T num, SIZE_T size)
{
    return NULL;
}

static MUTEX null_createMutex_case1(BOOL reentrant)
{
    return NULL;
}

static MUTEX null_createMutex_case2(BOOL reentrant)
{
    if (reentrant == FALSE) {
        return NULL;
    } else {
        return BackGlobalCreateMutex(reentrant);
    }
}

void test_initApp(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;

    unsetenv(APP_WEBRTC_CHANNEL);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_CHANNEL_NAME, retStatus);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    unsetenv(DEFAULT_REGION_ENV_VAR);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_APP_CREDENTIAL_NULL_ARG);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_TIMER, retStatus);

    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_IgnoreAndReturn(STATUS_APP_SIGNALING_INVALID_MUTEX);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_MUTEX, retStatus);

    initAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    createConnectionMsqQ_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_MEDIA_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_MEDIA_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_MEDIA_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_APP_WEBRTC_INIT);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_WEBRTC_INIT, retStatus);

    setenv(DEFAULT_REGION_ENV_VAR, APP_COMMON_UTEST_REGION_ENV_VAR, 1);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pAppCommonMock->pregenerateCertTimerCallback(0, 0, (UINT64) NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    generateCertRoutine_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppCommonMock->pregenerateCertTimerCallback(0, 0, (UINT64) pAppCommonMock->pregenerateCertTimerCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_initApp_setupFileLogging(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_StubWithCallback(setupFileLogging_callback);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    closeFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_initApp_null(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);

    BackGlobalMemCalloc = globalMemCalloc;
    globalMemCalloc = null_memCalloc;

    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NOT_ENOUGH_MEMORY, retStatus);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    globalMemCalloc = BackGlobalMemCalloc;
}

void test_initApp_null_mutex_case1(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_StubWithCallback(setupFileLogging_callback);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);

    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    BackGlobalCreateMutex = globalCreateMutex;
    globalCreateMutex = null_createMutex_case1;

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    closeFileLogging_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_INVALID_MUTEX, retStatus);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    globalCreateMutex = BackGlobalCreateMutex;
}

void test_initApp_null_mutex_case2(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_StubWithCallback(setupFileLogging_callback);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);

    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    BackGlobalCreateMutex = globalCreateMutex;
    globalCreateMutex = null_createMutex_case2;

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    closeFileLogging_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_INVALID_MUTEX, retStatus);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    globalCreateMutex = BackGlobalCreateMutex;
}

void test_runApp(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_APP_SIGNALING_CREATE);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_CREATE, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_freeApp_null_context(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    retStatus = freeApp(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);
}

void test_signalingStateChangeFn(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;

    setenv(APP_WEBRTC_CHANNEL, mAppCommonMock.channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    retStatus =
        pAppSignaling->signalingClientCallbacks.stateChangeFn(pAppSignaling->signalingClientCallbacks.customData, SIGNALING_CLIENT_STATE_CONNECTED);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingErrorReportFn(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    CHAR msg[APP_COMMON_UTEST_SIGNALING_ERROR_MSG_LEN];
    UINT32 msgLen;

    setenv(APP_WEBRTC_CHANNEL, mAppCommonMock.channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    ATOMIC_STORE_BOOL(&pAppConfiguration->restartSignalingClient, FALSE);
    memset(msg, 0, APP_COMMON_UTEST_SIGNALING_ERROR_MSG_LEN);
    memcpy(msg, "STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE", strlen("STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE"));
    msgLen = strlen(msg);
    retStatus =
        pAppSignaling->signalingClientCallbacks.errorReportFn((UINT64) pAppConfiguration, STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE, msg, msgLen);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->restartSignalingClient), FALSE);

    memset(msg, 0, APP_COMMON_UTEST_SIGNALING_ERROR_MSG_LEN);
    memcpy(msg, "STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED", strlen("STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED"));
    msgLen = strlen(msg);
    retStatus =
        pAppSignaling->signalingClientCallbacks.errorReportFn((UINT64) pAppConfiguration, STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED, msg, msgLen);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->restartSignalingClient), TRUE);

    ATOMIC_STORE_BOOL(&pAppConfiguration->restartSignalingClient, FALSE);
    memset(msg, 0, APP_COMMON_UTEST_SIGNALING_ERROR_MSG_LEN);
    memcpy(msg, "STATUS_SIGNALING_RECONNECT_FAILED", strlen("STATUS_SIGNALING_RECONNECT_FAILED"));
    msgLen = strlen(msg);
    retStatus = pAppSignaling->signalingClientCallbacks.errorReportFn((UINT64) pAppConfiguration, STATUS_SIGNALING_RECONNECT_FAILED, msg, msgLen);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->restartSignalingClient), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_offer_before_candidate_non_trickle(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    appHashTableContains_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_MEDIA_NOT_READY);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_READY, retStatus);

    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_IgnoreAndReturn(STATUS_APP_SIGNALING_INVALID_INFO_COUNT);

    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_INFO_COUNT, retStatus);

    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_IgnoreAndReturn(STATUS_APP_CREDENTIAL_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_MEDIA_NOT_EXISTED);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_MEDIA_NOT_EXISTED);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_StubWithCallback(addSupportedCodec_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_StubWithCallback(addTransceiver_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_StubWithCallback(transceiverOnBandwidthEstimation_return_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_INVALID_API_CALL_RETURN_JSON);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_INVALID_API_CALL_RETURN_JSON, retStatus);

    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES, retStatus);

    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_HASH_KEY_ALREADY_PRESENT);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_HASH_KEY_ALREADY_PRESENT, retStatus);

    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_IgnoreAndReturn(STATUS_APP_MSGQ_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_APP_MSGQ_POP_PENDING_MSQ);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_POP_PENDING_MSQ, retStatus);

    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    rtcPeerConnectionGetMetrics_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, pAppCommonMock->getIceCandidatePairStatsCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    prepareRtcMetrics(pAppConfiguration);
    rtcPeerConnectionGetMetrics_StubWithCallback(rtcPeerConnectionGetMetrics_callback);
    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, pAppCommonMock->getIceCandidatePairStatsCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_yes_callback);
    appHashTableGet_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableGet_StubWithCallback(appHashTableGet_callback);
    pAppCommonMock->pStreamingSession = NULL;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    pAppCommonMock->pStreamingSession = pAppConfiguration->streamingSessionList[0];
    deserializeRtcIceCandidateInit_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    deserializeRtcIceCandidateInit_IgnoreAndReturn(STATUS_SUCCESS);
    addIceCandidate_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_offer_before_candidate_trickle(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    appHashTableContains_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_MEDIA_NOT_READY);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_READY, retStatus);

    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_IgnoreAndReturn(STATUS_APP_SIGNALING_INVALID_INFO_COUNT);

    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_INVALID_INFO_COUNT, retStatus);

    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_IgnoreAndReturn(STATUS_APP_CREDENTIAL_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_MEDIA_NOT_EXISTED);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_MEDIA_NOT_EXISTED);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_StubWithCallback(addSupportedCodec_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_StubWithCallback(addTransceiver_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_StubWithCallback(transceiverOnBandwidthEstimation_return_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_INVALID_API_CALL_RETURN_JSON);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_INVALID_API_CALL_RETURN_JSON, retStatus);

    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES, retStatus);

    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_APP_SIGNALING_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_SIGNALING_NULL_ARG, retStatus);

    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_HASH_KEY_ALREADY_PRESENT);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_HASH_KEY_ALREADY_PRESENT, retStatus);

    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_IgnoreAndReturn(STATUS_APP_MSGQ_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_APP_MSGQ_POP_PENDING_MSQ);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_POP_PENDING_MSQ, retStatus);

    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_yes_callback);
    appHashTableGet_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableGet_StubWithCallback(appHashTableGet_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_two_same_offers_coming(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_INVALID_OPERATION, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_createStreamingSession_null(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);

    BackGlobalMemCalloc = globalMemCalloc;
    globalMemCalloc = null_memCalloc;

    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NOT_ENOUGH_MEMORY, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    globalMemCalloc = BackGlobalMemCalloc;
}

void test_signalingMessageReceivedFn_11_offers_coming(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);

    for (i = 0; i < APP_MAX_CONCURRENT_STREAMING_SESSION; ++i) {
        create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, i);
        retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
        TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    }

    getPendingMsgQByHashVal_IgnoreAndReturn(STATUS_APP_MSGQ_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, i);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_offer_with_null_cert(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_empty_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_INVALID_OPERATION, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_freeStreamingSession_case1(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_INVALID_ARG);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_INVALID_ARG, retStatus);
}

void test_signalingMessageReceivedFn_freeStreamingSession_case2(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);

    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    closePeerConnection_IgnoreAndReturn(STATUS_NULL_ARG);
    freePeerConnection_IgnoreAndReturn(STATUS_NULL_ARG);
    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_getIceCandidatePairStatsCallback_cancel_fail(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    appHashTableContains_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    rtcPeerConnectionGetMetrics_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, pAppCommonMock->getIceCandidatePairStatsCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    prepareRtcMetrics(pAppConfiguration);
    rtcPeerConnectionGetMetrics_StubWithCallback(rtcPeerConnectionGetMetrics_callback);
    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, pAppCommonMock->getIceCandidatePairStatsCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_yes_callback);
    appHashTableGet_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableGet_StubWithCallback(appHashTableGet_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_INVALID_ARG);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_INVALID_ARG, retStatus);
}

void test_getIceCandidatePairStatsCallback(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    appHashTableContains_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    rtcPeerConnectionGetMetrics_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, pAppCommonMock->getIceCandidatePairStatsCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    prepareRtcMetrics(pAppConfiguration);
    rtcPeerConnectionGetMetrics_StubWithCallback(rtcPeerConnectionGetMetrics_callback);
    retStatus = pAppCommonMock->getIceCandidatePairStatsCallback(0, 0, pAppCommonMock->getIceCandidatePairStatsCallbackUserData);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appHashTableContains_StubWithCallback(appHashTableContains_yes_callback);
    appHashTableGet_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appHashTableGet_StubWithCallback(appHashTableGet_callback);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_candidate_non_trickle(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // setup the message of ice candidates.
    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    getPendingMsgQByHashVal_IgnoreAndReturn(STATUS_APP_MSGQ_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    createPendingMsgQ_IgnoreAndReturn(STATUS_APP_MSGQ_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    createPendingMsgQ_StubWithCallback(createPendingMsgQ_callback);
    ;
    pushMsqIntoPendingMsgQ_IgnoreAndReturn(STATUS_APP_MSGQ_PUSH_PENDING_MSQ);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_PUSH_PENDING_MSQ, retStatus);

    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeRtcIceCandidateInit_IgnoreAndReturn(STATUS_SUCCESS);
    addIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getPendingMsgQByHashVal_IgnoreAndReturn(STATUS_SUCCESS);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_offer_after_candidate_non_trickle(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    createPendingMsgQ_StubWithCallback(createPendingMsgQ_callback);
    pushMsqIntoPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeRtcIceCandidateInit_IgnoreAndReturn(STATUS_SUCCESS);
    addIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_NULL_ARG);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_offer_after_candidate_non_trickl_pic(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) NULL, pReceivedSignalingMessage);

    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    createPendingMsgQ_StubWithCallback(createPendingMsgQ_callback);
    pushMsqIntoPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);

    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeRtcIceCandidateInit_IgnoreAndReturn(STATUS_SUCCESS);
    addIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);

    for (i = 0; i < APP_MAX_CONCURRENT_STREAMING_SESSION + 1; ++i) {
        create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, i);
        retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
        TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    }

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_signalingMessageReceivedFn_answer(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeRtcIceCandidateInit_IgnoreAndReturn(STATUS_SUCCESS);
    addIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_ANSWER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onIceCandidateHandler_trickle(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    CHAR candidateJson[APP_COMMON_UTEST_CANDIDATE_JSON_LEN];

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    pAppCommonMock->rtcOnIceCandidateHandler(NULL, candidateJson);

    memset(candidateJson, 0, APP_COMMON_UTEST_CANDIDATE_JSON_LEN);
    memcpy(candidateJson, APP_COMMON_UTEST_CANDIDATE_JSON_STRING, strlen(APP_COMMON_UTEST_CANDIDATE_JSON_STRING));
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, candidateJson);

    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onIceCandidateHandler_trickle_error(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    CHAR candidateJson[APP_COMMON_UTEST_CANDIDATE_JSON_LEN];

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    pAppCommonMock->rtcOnIceCandidateHandler(NULL, candidateJson);

    sendAppSignalingMessage_IgnoreAndReturn(STATUS_APP_SIGNALING_NULL_ARG);
    memset(candidateJson, 0, APP_COMMON_UTEST_CANDIDATE_JSON_LEN);
    memcpy(candidateJson, APP_COMMON_UTEST_CANDIDATE_JSON_STRING, strlen(APP_COMMON_UTEST_CANDIDATE_JSON_STRING));
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, candidateJson);

    ATOMIC_STORE_BOOL(&pStreamingSession->peerIdReceived, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, candidateJson);

    createAnswer_IgnoreAndReturn(STATUS_NULL_ARG);
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_NULL_ARG);
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onIceCandidateHandler_non_trickle(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    CHAR candidateJson[APP_COMMON_UTEST_CANDIDATE_JSON_LEN];

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    memset(candidateJson, 0, APP_COMMON_UTEST_CANDIDATE_JSON_LEN);
    memcpy(candidateJson, APP_COMMON_UTEST_CANDIDATE_JSON_STRING, strlen(APP_COMMON_UTEST_CANDIDATE_JSON_STRING));
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, candidateJson);

    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onIceCandidateHandler_non_trickle_error(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    CHAR candidateJson[APP_COMMON_UTEST_CANDIDATE_JSON_LEN];

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, FALSE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    pAppCommonMock->rtcOnIceCandidateHandler(NULL, candidateJson);

    sendAppSignalingMessage_IgnoreAndReturn(STATUS_APP_SIGNALING_NULL_ARG);
    memset(candidateJson, 0, APP_COMMON_UTEST_CANDIDATE_JSON_LEN);
    memcpy(candidateJson, APP_COMMON_UTEST_CANDIDATE_JSON_STRING, strlen(APP_COMMON_UTEST_CANDIDATE_JSON_STRING));
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, candidateJson);

    ATOMIC_STORE_BOOL(&pStreamingSession->peerIdReceived, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, candidateJson);

    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_NULL_ARG);
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_NULL_ARG);
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER);
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler(pAppCommonMock->onIceCandidateUData, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onMediaHook(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    Frame frame;
    PFrame pFrame = &frame;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_StubWithCallback(linkMeidaSinkHook_callback);
    linkMeidaEosHook_StubWithCallback(linkMeidaEosHook_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler((UINT64) pStreamingSession, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    retStatus = pAppCommonMock->mediaSinkHook(NULL, pFrame);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);

    pFrame->flags = FRAME_FLAG_NONE;
    pFrame->trackId = DEFAULT_VIDEO_TRACK_ID;
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(pStreamingSession->firstKeyFrame, FALSE);
    TEST_ASSERT_EQUAL(pFrame->index, 0);

    writeFrame_IgnoreAndReturn(STATUS_SRTP_NOT_READY_YET);
    pFrame->flags = FRAME_FLAG_KEY_FRAME;
    pFrame->trackId = DEFAULT_VIDEO_TRACK_ID;
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(pStreamingSession->firstKeyFrame, TRUE);
    TEST_ASSERT_EQUAL(pFrame->index, 0);

    writeFrame_IgnoreAndReturn(STATUS_SUCCESS);
    pFrame->flags = FRAME_FLAG_KEY_FRAME;
    pFrame->trackId = DEFAULT_VIDEO_TRACK_ID;
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(pStreamingSession->firstKeyFrame, TRUE);
    TEST_ASSERT_EQUAL(pFrame->index, 1);

    pFrame->flags = FRAME_FLAG_KEY_FRAME;
    pFrame->trackId = DEFAULT_AUDIO_TRACK_ID;
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(pFrame->index, 2);

    writeFrame_IgnoreAndReturn(STATUS_SRTP_NOT_READY_YET);
    pFrame->flags = FRAME_FLAG_KEY_FRAME;
    pFrame->trackId = DEFAULT_AUDIO_TRACK_ID;
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(pFrame->index, 3);

    writeFrame_IgnoreAndReturn(STATUS_SUCCESS);
    pFrame->flags = FRAME_FLAG_KEY_FRAME;
    pFrame->trackId = DEFAULT_AUDIO_TRACK_ID;
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(pFrame->index, 4);

    ATOMIC_STORE_BOOL(&pAppConfiguration->terminateApp, TRUE);
    retStatus = pAppCommonMock->mediaSinkHook(pAppCommonMock->mediaSinkHookUdata, pFrame);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_SHUTDOWN_MEDIA, retStatus);

    retStatus = pAppCommonMock->mediaEosHook(pAppCommonMock->mediaEosHookUdata);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->terminateFlag), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onConnectionStateChange(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    StreamingSession fakeStreamingSession;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_StubWithCallback(linkMeidaSinkHook_callback);
    linkMeidaEosHook_StubWithCallback(linkMeidaEosHook_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    runMediaSource_StubWithCallback(runMediaSource_callback);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler((UINT64) pStreamingSession, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    pAppCommonMock->rtcOnConnectionStateChange(NULL, RTC_PEER_CONNECTION_STATE_CONNECTING);

    pStreamingSession = &fakeStreamingSession;
    pStreamingSession->pAppConfiguration = NULL;
    pAppCommonMock->rtcOnConnectionStateChange(pStreamingSession, RTC_PEER_CONNECTION_STATE_CONNECTING);

    pStreamingSession = (PStreamingSession) pAppCommonMock->rtcOnConnectionStateChangeUData;
    pAppCommonMock->rtcOnConnectionStateChange(pAppCommonMock->rtcOnConnectionStateChangeUData, RTC_PEER_CONNECTION_STATE_CONNECTING);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->peerConnectionConnected), FALSE);

    logSelectedIceCandidatesInformation_IgnoreAndReturn(STATUS_APP_METRICS_NULL_ARG);
    pAppCommonMock->rtcOnConnectionStateChange(pAppCommonMock->rtcOnConnectionStateChangeUData, RTC_PEER_CONNECTION_STATE_CONNECTED);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->peerConnectionConnected), TRUE);

    logSelectedIceCandidatesInformation_IgnoreAndReturn(STATUS_SUCCESS);
    pAppCommonMock->rtcOnConnectionStateChange(pAppCommonMock->rtcOnConnectionStateChangeUData, RTC_PEER_CONNECTION_STATE_CONNECTED);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->peerConnectionConnected), TRUE);

    pAppCommonMock->rtcOnConnectionStateChange(pAppCommonMock->rtcOnConnectionStateChangeUData, RTC_PEER_CONNECTION_STATE_DISCONNECTED);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->terminateFlag), TRUE);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_mediaSenderRoutine(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    StreamingSession fakeStreamingSession;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_StubWithCallback(linkMeidaSinkHook_callback);
    linkMeidaEosHook_StubWithCallback(linkMeidaEosHook_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);

    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->rtcOnConnectionStateChangeUData;
    logSelectedIceCandidatesInformation_IgnoreAndReturn(STATUS_SUCCESS);
    pAppCommonMock->rtcOnConnectionStateChange(pAppCommonMock->rtcOnConnectionStateChangeUData, RTC_PEER_CONNECTION_STATE_CONNECTED);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->peerConnectionConnected), TRUE);
    sleep(1);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_null_mediaSenderRoutine(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    StreamingSession fakeStreamingSession;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_StubWithCallback(linkMeidaSinkHook_callback);
    linkMeidaEosHook_StubWithCallback(linkMeidaEosHook_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    pAppConfiguration->mediaSource = NULL;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->rtcOnConnectionStateChangeUData;
    logSelectedIceCandidatesInformation_IgnoreAndReturn(STATUS_SUCCESS);
    pAppCommonMock->rtcOnConnectionStateChange(pAppCommonMock->rtcOnConnectionStateChangeUData, RTC_PEER_CONNECTION_STATE_CONNECTED);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pAppConfiguration->peerConnectionConnected), TRUE);
    sleep(1);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onBandwidthEstimationHandler(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_StubWithCallback(linkMeidaSinkHook_callback);
    linkMeidaEosHook_StubWithCallback(linkMeidaEosHook_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_StubWithCallback(transceiverOnBandwidthEstimation_callback);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler((UINT64) pStreamingSession, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    pAppCommonMock->rtcOnBandwidthEstimation(pAppCommonMock->rtcOnBandwidthEstimationUData, 2.3);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_onSenderBandwidthEstimationHandler(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppConfiguration pAppConfiguration;
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    PStreamingSession pStreamingSession;
    UINT32 txBytes, rxBytes, txPacketsCnt, rxPacketsCnt;
    UINT64 duration;

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);
    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_callback);
    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_StubWithCallback(linkMeidaSinkHook_callback);
    linkMeidaEosHook_StubWithCallback(linkMeidaEosHook_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_IgnoreAndReturn(STATUS_SUCCESS);
    signalingClientGetStateString_StubWithCallback(signalingClientGetStateString_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    pAppSignaling = &pAppConfiguration->appSignaling;
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_StubWithCallback(peerConnectionOnIceCandidate_callback);
    peerConnectionOnConnectionStateChange_StubWithCallback(peerConnectionOnConnectionStateChange_callback);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_StubWithCallback(transceiverOnBandwidthEstimation_callback);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_StubWithCallback(peerConnectionOnSenderBandwidthEstimation_callback);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTablePut_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    getAppSignalingRole_IgnoreAndReturn(SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pStreamingSession = (PStreamingSession) pAppCommonMock->onIceCandidateUData;
    ATOMIC_STORE_BOOL(&pStreamingSession->candidateGatheringDone, FALSE);
    pAppCommonMock->rtcOnIceCandidateHandler((UINT64) pStreamingSession, NULL);
    TEST_ASSERT_EQUAL(ATOMIC_LOAD_BOOL(&pStreamingSession->candidateGatheringDone), TRUE);

    pAppCommonMock->rtcOnBandwidthEstimation(pAppCommonMock->rtcOnBandwidthEstimationUData, 2.3);

    txBytes = 10000;
    rxBytes = 10000;
    txPacketsCnt = 100;
    rxPacketsCnt = 100;
    duration = 1 * 10000ULL;
    pAppCommonMock->rtcOnSenderBandwidthEstimation(pAppCommonMock->rtcOnSenderBandwidthEstimationUData, txBytes, rxBytes, txPacketsCnt, rxPacketsCnt,
                                                   duration);

    txBytes = 10000;
    rxBytes = 500;
    txPacketsCnt = 100;
    rxPacketsCnt = 5;
    duration = 1 * 10000ULL;
    pAppCommonMock->rtcOnSenderBandwidthEstimation(pAppCommonMock->rtcOnSenderBandwidthEstimationUData, txBytes, rxBytes, txPacketsCnt, rxPacketsCnt,
                                                   duration);

    txBytes = 10000;
    rxBytes = 500;
    txPacketsCnt = 100;
    rxPacketsCnt = 95;
    duration = 1 * 10000ULL;
    pAppCommonMock->rtcOnSenderBandwidthEstimation(pAppCommonMock->rtcOnSenderBandwidthEstimationUData, txBytes, rxBytes, txPacketsCnt, rxPacketsCnt,
                                                   duration);

    freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableClear_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableFree_IgnoreAndReturn(STATUS_SUCCESS);
    logIceServerStats_StubWithCallback(logIceServerStats_callback);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = freeApp(&pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_pollApp(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    retStatus = pollApp(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_COMMON_NULL_ARG, retStatus);
}

static PVOID tstAppMain_case1(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration* ppAppConfiguration = (PAppConfiguration*) userData;
    PAppConfiguration pAppConfiguration = NULL;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;
    SET_INSTRUMENTED_ALLOCATORS();

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    *ppAppConfiguration = pAppConfiguration;

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    ;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 1);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    canTrickle.isNull = FALSE;
    canTrickle.value = TRUE;
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // Checking for termination
    checkAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    removeExpiredPendingMsgQ_IgnoreAndReturn(STATUS_APP_MSGQ_NULL_ARG);
    shutdownMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    ATOMIC_STORE_BOOL(&pAppConfiguration->restartSignalingClient, FALSE);
    retStatus = pollApp(pAppConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] pollApp(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[WebRTC App] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] Terminated with status code 0x%08x", retStatus);
    }

    printf("[WebRTC App] Cleaning up....\n");

    if (pAppConfiguration != NULL) {
        freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
        freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
        logIceServerStats_StubWithCallback(logIceServerStats_callback);
        appTimerQueueCancel_IgnoreAndReturn(STATUS_NULL_ARG);
        appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
        deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
        detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
        destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
        retStatus = freeApp(&pAppConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[WebRTC App] freeApp(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[WebRTC App] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();
    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void test_appMain_case1(void)
{
    pthread_t tstMainTid;
    PAppConfiguration pAppConfiguration;

    pthread_create(&tstMainTid, NULL, tstAppMain_case1, &pAppConfiguration);
    sleep(2);
    pthread_kill(tstMainTid, SIGINT);
    pthread_join(tstMainTid, NULL);
}

static PVOID tstAppMain_case2(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration* ppAppConfiguration = (PAppConfiguration*) userData;
    PAppConfiguration pAppConfiguration = NULL;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;
    SET_INSTRUMENTED_ALLOCATORS();

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    *ppAppConfiguration = pAppConfiguration;

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    ;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 1);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    canTrickle.isNull = FALSE;
    canTrickle.value = TRUE;
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    for (i = 0; i < pAppConfiguration->streamingSessionCount; ++i) {
        ATOMIC_STORE_BOOL(&pAppConfiguration->streamingSessionList[i]->terminateFlag, TRUE);
    }
    // Checking for termination
    checkAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    removeExpiredPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    shutdownMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_NULL_ARG);
    ATOMIC_STORE_BOOL(&pAppConfiguration->restartSignalingClient, TRUE);
    restartAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pollApp(pAppConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] pollApp(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[WebRTC App] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] Terminated with status code 0x%08x", retStatus);
    }

    printf("[WebRTC App] Cleaning up....\n");

    if (pAppConfiguration != NULL) {
        freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
        freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
        logIceServerStats_StubWithCallback(logIceServerStats_callback);
        appTimerQueueCancel_IgnoreAndReturn(STATUS_NULL_ARG);
        appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
        deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
        detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
        destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
        retStatus = freeApp(&pAppConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[WebRTC App] freeApp(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[WebRTC App] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();
    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void test_appMain_case2(void)
{
    pthread_t tstMainTid;
    PAppConfiguration pAppConfiguration;

    pthread_create(&tstMainTid, NULL, tstAppMain_case2, &pAppConfiguration);
    sleep(2);
    pthread_kill(tstMainTid, SIGINT);
    pthread_join(tstMainTid, NULL);
}

static PVOID tstAppMain_case3(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration* ppAppConfiguration = (PAppConfiguration*) userData;
    PAppConfiguration pAppConfiguration = NULL;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;
    SET_INSTRUMENTED_ALLOCATORS();

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    *ppAppConfiguration = pAppConfiguration;

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    ;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 1);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    canTrickle.isNull = FALSE;
    canTrickle.value = TRUE;
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    for (i = 0; i < pAppConfiguration->streamingSessionCount; ++i) {
        ATOMIC_STORE_BOOL(&pAppConfiguration->streamingSessionList[i]->terminateFlag, TRUE);
    }
    // Checking for termination
    checkAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    removeExpiredPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    shutdownMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableContains_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pollApp(pAppConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] pollApp(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[WebRTC App] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] Terminated with status code 0x%08x", retStatus);
    }

    printf("[WebRTC App] Cleaning up....\n");

    if (pAppConfiguration != NULL) {
        freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
        freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
        logIceServerStats_StubWithCallback(logIceServerStats_callback);
        appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
        appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
        deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
        detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
        destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
        retStatus = freeApp(&pAppConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[WebRTC App] freeApp(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[WebRTC App] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();
    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void test_appMain_case3(void)
{
    pthread_t tstMainTid;
    PAppConfiguration pAppConfiguration;

    pthread_create(&tstMainTid, NULL, tstAppMain_case3, &pAppConfiguration);
    sleep(2);
    pthread_kill(tstMainTid, SIGINT);
    pthread_join(tstMainTid, NULL);
}

static PVOID tstAppMain_case4(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration* ppAppConfiguration = (PAppConfiguration*) userData;
    PAppConfiguration pAppConfiguration = NULL;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;
    SET_INSTRUMENTED_ALLOCATORS();

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    *ppAppConfiguration = pAppConfiguration;

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    ;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 1);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    canTrickle.isNull = FALSE;
    canTrickle.value = TRUE;
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    for (i = 0; i < pAppConfiguration->streamingSessionCount; ++i) {
        ATOMIC_STORE_BOOL(&pAppConfiguration->streamingSessionList[i]->terminateFlag, TRUE);
    }
    // Checking for termination
    checkAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    removeExpiredPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    shutdownMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableContains_StubWithCallback(appHashTableContains_no_callback);
    retStatus = pollApp(pAppConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] pollApp(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[WebRTC App] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] Terminated with status code 0x%08x", retStatus);
    }

    printf("[WebRTC App] Cleaning up....\n");

    if (pAppConfiguration != NULL) {
        freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
        freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
        logIceServerStats_StubWithCallback(logIceServerStats_callback);
        appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
        appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
        deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
        detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
        destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
        retStatus = freeApp(&pAppConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[WebRTC App] freeApp(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[WebRTC App] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();
    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void test_appMain_case4(void)
{
    pthread_t tstMainTid;
    PAppConfiguration pAppConfiguration;

    pthread_create(&tstMainTid, NULL, tstAppMain_case4, &pAppConfiguration);
    sleep(2);
    pthread_kill(tstMainTid, SIGINT);
    pthread_join(tstMainTid, NULL);
}

static PVOID tstAppMain_case5(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration* ppAppConfiguration = (PAppConfiguration*) userData;
    PAppConfiguration pAppConfiguration = NULL;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;
    SET_INSTRUMENTED_ALLOCATORS();

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    *ppAppConfiguration = pAppConfiguration;

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    ;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 1);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    canTrickle.isNull = FALSE;
    canTrickle.value = TRUE;
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    for (i = 0; i < pAppConfiguration->streamingSessionCount; ++i) {
        ATOMIC_STORE_BOOL(&pAppConfiguration->streamingSessionList[i]->terminateFlag, TRUE);
    }
    // Checking for termination
    checkAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    removeExpiredPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    shutdownMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    appHashTableRemove_IgnoreAndReturn(STATUS_HASH_KEY_NOT_PRESENT);
    retStatus = pollApp(pAppConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] pollApp(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[WebRTC App] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] Terminated with status code 0x%08x", retStatus);
    }

    printf("[WebRTC App] Cleaning up....\n");

    if (pAppConfiguration != NULL) {
        freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
        freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
        logIceServerStats_StubWithCallback(logIceServerStats_callback);
        appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
        appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
        deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
        detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
        destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
        retStatus = freeApp(&pAppConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[WebRTC App] freeApp(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[WebRTC App] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();
    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void test_appMain_case5(void)
{
    pthread_t tstMainTid;
    PAppConfiguration pAppConfiguration;

    pthread_create(&tstMainTid, NULL, tstAppMain_case5, &pAppConfiguration);
    sleep(2);
    pthread_kill(tstMainTid, SIGINT);
    pthread_join(tstMainTid, NULL);
}
static PVOID tstAppMain_case6(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppConfiguration* ppAppConfiguration = (PAppConfiguration*) userData;
    PAppConfiguration pAppConfiguration = NULL;
    PAppCommonMock pAppCommonMock = getAppCommonMock();
    PAppSignaling pAppSignaling;
    ReceivedSignalingMessage receivedSignalingMessage;
    PReceivedSignalingMessage pReceivedSignalingMessage = &receivedSignalingMessage;
    UINT32 i;
    SET_INSTRUMENTED_ALLOCATORS();

    appHashTableCreateWithParams_StubWithCallback(appHashTableCreateWithParams_pic_callback);
    appHashTableContains_StubWithCallback(appHashTableContains_pic_callback);
    appHashTableGet_StubWithCallback(appHashTableGet_pic_callback);
    appHashTablePut_StubWithCallback(appHashTablePut_pic_callback);
    appHashTableRemove_StubWithCallback(appHashTableRemove_pic_callback);
    appHashTableClear_StubWithCallback(appHashTableClear_pic_callback);
    appHashTableFree_StubWithCallback(appHashTableFree_pic_callback);
    createConnectionMsqQ_StubWithCallback(createConnectionMsqQ_success_callback);

    setenv(APP_WEBRTC_CHANNEL, pAppCommonMock->channelName, 1);
    getLogLevel_IgnoreAndReturn(LOG_LEVEL_WARN);
    setupFileLogging_IgnoreAndReturn(STATUS_SUCCESS);
    createCredential_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCreate_StubWithCallback(appTimerQueueCreate_callback);
    initAppSignaling_StubWithCallback(initAppSignaling_callback);

    initMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaSinkHook_IgnoreAndReturn(STATUS_SUCCESS);
    linkMeidaEosHook_IgnoreAndReturn(STATUS_SUCCESS);
    runMediaSource_StubWithCallback(runMediaSource_callback);
    initWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_pregenerateCertTimer_callback);
    retStatus = initApp(pAppCommonMock->trickleIce, pAppCommonMock->useTurn, &pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    *ppAppConfiguration = pAppConfiguration;

    connectAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = runApp(pAppConfiguration);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 0);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    NullableBool canTrickle = {FALSE, TRUE};
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    appTimeQueueAdd_StubWithCallback(appTimeQueueAdd_getIceCandidatePairStats_callback);
    ;
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    pAppSignaling = &pAppConfiguration->appSignaling;
    create_signaling_message(pReceivedSignalingMessage, SIGNALING_MESSAGE_TYPE_OFFER, 1);
    isMediaSourceReady_IgnoreAndReturn(STATUS_SUCCESS);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    closePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    freePeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    queryAppSignalingServer_StubWithCallback(queryAppSignalingServer_callback);
    popGeneratedCert_StubWithCallback(popGeneratedCert_existed_callback);
    createPeerConnection_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnIceCandidate_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnConnectionStateChange_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnDataChannel_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaVideoCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    queryMediaAudioCap_IgnoreAndReturn(STATUS_SUCCESS);
    addSupportedCodec_IgnoreAndReturn(STATUS_SUCCESS);
    addTransceiver_IgnoreAndReturn(STATUS_SUCCESS);
    transceiverOnBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    peerConnectionOnSenderBandwidthEstimation_IgnoreAndReturn(STATUS_SUCCESS);
    deserializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    setRemoteDescription_IgnoreAndReturn(STATUS_SUCCESS);
    canTrickle.isNull = FALSE;
    canTrickle.value = TRUE;
    canTrickleIceCandidates_IgnoreAndReturn(canTrickle);
    setLocalDescription_IgnoreAndReturn(STATUS_SUCCESS);
    createAnswer_IgnoreAndReturn(STATUS_SUCCESS);
    serializeSessionDescriptionInit_IgnoreAndReturn(STATUS_SUCCESS);
    sendAppSignalingMessage_IgnoreAndReturn(STATUS_SUCCESS);
    getPendingMsgQByHashVal_StubWithCallback(getPendingMsgQByHashVal_callback);
    handlePendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = pAppSignaling->signalingClientCallbacks.messageReceivedFn((UINT64) pAppConfiguration, pReceivedSignalingMessage);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    for (i = 0; i < pAppConfiguration->streamingSessionCount; ++i) {
        ATOMIC_STORE_BOOL(&pAppConfiguration->streamingSessionList[i]->terminateFlag, TRUE);
    }
    // Checking for termination
    checkAppSignaling_IgnoreAndReturn(STATUS_APP_SIGNALING_CONNECT);
    removeExpiredPendingMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
    shutdownMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
    appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
    ATOMIC_STORE_BOOL(&pAppConfiguration->restartSignalingClient, TRUE);
    restartAppSignaling_IgnoreAndReturn(STATUS_APP_SIGNALING_RESTART);
    retStatus = pollApp(pAppConfiguration);
    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] pollApp(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[WebRTC App] Streaming session terminated\n");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[WebRTC App] Terminated with status code 0x%08x", retStatus);
    }

    printf("[WebRTC App] Cleaning up....\n");

    if (pAppConfiguration != NULL) {
        freeAppSignaling_IgnoreAndReturn(STATUS_SUCCESS);
        freeConnectionMsgQ_IgnoreAndReturn(STATUS_SUCCESS);
        logIceServerStats_StubWithCallback(logIceServerStats_callback);
        appTimerQueueCancel_IgnoreAndReturn(STATUS_SUCCESS);
        appTimerQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
        deinitWebRtc_IgnoreAndReturn(STATUS_SUCCESS);
        detroyMediaSource_IgnoreAndReturn(STATUS_SUCCESS);
        destroyCredential_IgnoreAndReturn(STATUS_SUCCESS);
        retStatus = freeApp(&pAppConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            printf("[WebRTC App] freeApp(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[WebRTC App] Cleanup done\n");

    RESET_INSTRUMENTED_ALLOCATORS();
    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

void test_appMain_case6(void)
{
    pthread_t tstMainTid;
    PAppConfiguration pAppConfiguration;

    pthread_create(&tstMainTid, NULL, tstAppMain_case6, &pAppConfiguration);
    sleep(2);
    pthread_kill(tstMainTid, SIGINT);
    pthread_join(tstMainTid, NULL);
}