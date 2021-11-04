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
#include "AppMessageQueue.h"
#include "mock_Include.h"
#include "mock_AppQueueWrap.h"

#define APP_MESSAGE_QUEUE_UTEST_HASH_VALUE         0x1234
#define APP_MESSAGE_QUEUE_UTEST_INVALID_HASH_VALUE 0x1235

typedef struct {
    PConnectionMsgQ pConnectionMsgQ;
} AppMessageQueue, *PAppMessageQueue;

static AppMessageQueue mAppMessageQueue;
static memCalloc BackGlobalMemCalloc;

/* Called before each test method. */
void setUp()
{
}

/* Called after each test method. */
void tearDown()
{
}

static PAppMessageQueue getAppMessageQueue(void)
{
    return &mAppMessageQueue;
}

static STATUS appQueueCreate_pic_callback(PStackQueue* ppStackQueue)
{
    return stackQueueCreate(ppStackQueue);
}

static STATUS appQueueGetCount_pic_callback(PStackQueue pStackQueue, PUINT32 pCount)
{
    return stackQueueGetCount(pStackQueue, pCount);
}

static STATUS appQueueIsEmpty_pic_callback(PStackQueue pStackQueue, PBOOL pIsEmpty)
{
    return stackQueueIsEmpty(pStackQueue, pIsEmpty);
}

static STATUS appQueueEnqueue_pic_callback(PStackQueue pStackQueue, UINT64 item)
{
    return stackQueueEnqueue(pStackQueue, item);
}

static STATUS appQueueDequeue_pic_callback(PStackQueue pStackQueue, PUINT64 pItem)
{
    return stackQueueDequeue(pStackQueue, pItem);
}

static STATUS appQueueGetIterator_pic_callback(PStackQueue pStackQueue, PStackQueueIterator pIterator)
{
    return stackQueueGetIterator(pStackQueue, pIterator);
}

static STATUS appQueueIteratorGetItem_pic_callback(StackQueueIterator iterator, PUINT64 pData)
{
    return stackQueueIteratorGetItem(iterator, pData);
}

static STATUS appQueueIteratorNext_pic_callback(PStackQueueIterator pIterator)
{
    return stackQueueIteratorNext(pIterator);
}

static STATUS appQueueRemoveItem_pic_callback(PStackQueue pStackQueue, UINT64 item)
{
    return stackQueueRemoveItem(pStackQueue, item);
}

static STATUS appQueueClear_pic_callback(PStackQueue pStackQueue, BOOL freeData)
{
    return stackQueueClear(pStackQueue, freeData);
}

static STATUS appQueueFree_pic_callback(PStackQueue pStackQueue)
{
    return stackQueueFree(pStackQueue);
}

static STATUS msgHandleHook_success_callback(PVOID udata, PSignalingMessage pSignalingMessage)
{
    UNUSED_PARAM(udata);

    return STATUS_SUCCESS;
}

static STATUS msgHandleHook_fail_callback(PVOID udata, PSignalingMessage pSignalingMessage)
{
    return STATUS_APP_MSGQ_HANDLE_PENDING_MSQ;
}

static PVOID null_memCalloc(SIZE_T num, SIZE_T size)
{
    return NULL;
}

void test_createConnectionMsqQ(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();

    retStatus = createConnectionMsqQ(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_CREATE_CONN_MSQ, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueClear_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_createConnectionMsqQ_null(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);

    BackGlobalMemCalloc = globalMemCalloc;
    globalMemCalloc = null_memCalloc;

    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NOT_ENOUGH_MEMORY, retStatus);

    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);

    globalMemCalloc = BackGlobalMemCalloc;
}

void test_createPendingMsgQ(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMessageQueue;

    retStatus = createPendingMsgQ(NULL, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    retStatus = createPendingMsgQ(&pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    retStatus = createPendingMsgQ(&pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueCreate_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_CREATE_PENDING_MSQ, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    appQueueEnqueue_IgnoreAndReturn(STATUS_NULL_ARG);
    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueClear_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_PUSH_CONN_MSQ, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_createPendingMsgQ_null(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMessageQueue;

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    BackGlobalMemCalloc = globalMemCalloc;
    globalMemCalloc = null_memCalloc;

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NOT_ENOUGH_MEMORY, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);

    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);

    globalMemCalloc = BackGlobalMemCalloc;
}

void test_getPendingMsgQByHashVal(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMessageQueue;

    retStatus = getPendingMsgQByHashVal(NULL, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, FALSE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, FALSE, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueGetIterator_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, FALSE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueIteratorGetItem_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, FALSE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, FALSE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, FALSE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueRemoveItem_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueRemoveItem_StubWithCallback(appQueueRemoveItem_pic_callback);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_INVALID_HASH_VALUE, TRUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_pushMsqIntoPendingMsgQ(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMessageQueue;
    ReceivedSignalingMessage msg;

    retStatus = pushMsqIntoPendingMsgQ(NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = pushMsqIntoPendingMsgQ(pPendingMessageQueue, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueEnqueue_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMessageQueue, &msg);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_PUSH_PENDING_MSQ, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMessageQueue, &msg);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_pushMsqIntoPendingMsgQ_null(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMessageQueue;
    ReceivedSignalingMessage msg;

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMessageQueue);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    BackGlobalMemCalloc = globalMemCalloc;
    globalMemCalloc = null_memCalloc;

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMessageQueue, &msg);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NOT_ENOUGH_MEMORY, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);

    globalMemCalloc = BackGlobalMemCalloc;
}

void test_handlePendingMsgQ(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMsgQ;
    ReceivedSignalingMessage msg;

    retStatus = handlePendingMsgQ(NULL, msgHandleHook_success_callback, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMsgQ, &msg);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    appQueueRemoveItem_StubWithCallback(appQueueRemoveItem_pic_callback);
    appQueueGetCount_StubWithCallback(appQueueGetCount_pic_callback);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueIsEmpty_IgnoreAndReturn(STATUS_NULL_ARG);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueClear_pic_callback);
    retStatus = handlePendingMsgQ(pPendingMsgQ, msgHandleHook_success_callback, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_EMPTY_PENDING_MSQ, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMsgQ, &msg);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueIsEmpty_StubWithCallback(appQueueIsEmpty_pic_callback);
    appQueueDequeue_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = handlePendingMsgQ(pPendingMsgQ, msgHandleHook_success_callback, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_POP_PENDING_MSQ, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMsgQ, &msg);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueDequeue_StubWithCallback(appQueueDequeue_pic_callback);
    retStatus = handlePendingMsgQ(pPendingMsgQ, msgHandleHook_success_callback, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMsgQ, &msg);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = handlePendingMsgQ(pPendingMsgQ, msgHandleHook_fail_callback, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_HANDLE_PENDING_MSQ, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = pushMsqIntoPendingMsgQ(pPendingMsgQ, &msg);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = getPendingMsgQByHashVal(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, TRUE, &pPendingMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = handlePendingMsgQ(pPendingMsgQ, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_removeExpiredPendingMsgQ_case1(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMsgQ0, pPendingMsgQ1, pPendingMsgQ2;
    UINT64 startTime;
    ;

    retStatus = removeExpiredPendingMsgQ(NULL, 0);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ0);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE + 1, &pPendingMsgQ1);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE + 2, &pPendingMsgQ2);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    startTime = GETTIME();
    pPendingMsgQ0->createTime = startTime - 40 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pPendingMsgQ1->createTime = startTime - 2 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pPendingMsgQ2->createTime = startTime;
    appQueueGetCount_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = removeExpiredPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueGetCount_StubWithCallback(appQueueGetCount_pic_callback);
    appQueueDequeue_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = removeExpiredPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueDequeue_StubWithCallback(appQueueDequeue_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    appQueueEnqueue_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = removeExpiredPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_removeExpiredPendingMsgQ_case2(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppMessageQueue pAppMessageQueue = getAppMessageQueue();
    PPendingMessageQueue pPendingMsgQ0, pPendingMsgQ1, pPendingMsgQ2;
    UINT64 startTime;
    ;

    retStatus = removeExpiredPendingMsgQ(NULL, 0);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);

    appQueueCreate_StubWithCallback(appQueueCreate_pic_callback);
    retStatus = createConnectionMsqQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueEnqueue_StubWithCallback(appQueueEnqueue_pic_callback);
    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE, &pPendingMsgQ0);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE + 1, &pPendingMsgQ1);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = createPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, APP_MESSAGE_QUEUE_UTEST_HASH_VALUE + 2, &pPendingMsgQ2);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    startTime = GETTIME();
    pPendingMsgQ0->createTime = startTime - 40 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pPendingMsgQ1->createTime = startTime - 2 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    pPendingMsgQ2->createTime = startTime;

    appQueueGetCount_StubWithCallback(appQueueGetCount_pic_callback);
    appQueueDequeue_StubWithCallback(appQueueDequeue_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    retStatus = removeExpiredPendingMsgQ(pAppMessageQueue->pConnectionMsgQ, 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_pic_callback);
    appQueueIteratorGetItem_StubWithCallback(appQueueIteratorGetItem_pic_callback);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_pic_callback);
    appQueueClear_StubWithCallback(appQueueClear_pic_callback);
    appQueueFree_StubWithCallback(appQueueFree_pic_callback);
    retStatus = freeConnectionMsgQ(&pAppMessageQueue->pConnectionMsgQ);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pAppMessageQueue->pConnectionMsgQ);
}

void test_freeConnectionMsgQ(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    retStatus = freeConnectionMsgQ(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_MSGQ_NULL_ARG, retStatus);
}
