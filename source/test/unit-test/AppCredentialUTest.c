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
#include <fcntl.h>
#include "unity.h"
#include "AppCredential.h"
#include "mock_Include.h"
#include "mock_AppCredentialWrap.h"
#include "mock_AppQueueWrap.h"

#define APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR          "."
#define APP_CREDENTIAL_UTEST_INVALID_CACERT_PATH_ENV_VAR  "./12345"
#define APP_CREDENTIAL_UTEST_CACERT_NAME                  "test.pem"
#define APP_CREDENTIAL_UTEST_NULL_CACERT_NAME             ""
#define APP_CREDENTIAL_UTEST_ACCESS_KEY_ENV_VAR           "access_key_id"
#define APP_CREDENTIAL_UTEST_SECRET_KEY_ENV_VAR           "access_secret_key"
#define APP_CREDENTIAL_UTEST_SESSION_TOKEN_ENV_VAR        "session_token"
#define APP_CREDENTIAL_UTEST_IOT_CORE_THING_NAME          "thing_name"
#define APP_CREDENTIAL_UTEST_IOT_CORE_CREDENTIAL_ENDPOINT "credential_point"
#define APP_CREDENTIAL_UTEST_IOT_CORE_CERT                "cert"
#define APP_CREDENTIAL_UTEST_IOT_CORE_PRIVATE_KEY         "private_key"
#define APP_CREDENTIAL_UTEST_IOT_CORE_ROLE_ALIAS          "role_alias"
#define APP_CREDENTIAL_UTEST_ECS_AUTH_TOKEN               "auth_token"
#define APP_CREDENTIAL_UTEST_ECS_CREDENTIALS_FULL_URI     "credential_uri"

typedef struct {
    PAppCredential pAppCredential;
    PStackQueue pStackQueue;
    PRtcCertificate pRtcCertificate;
    StackQueueIterator pIterator;
} AppCredentialMock, *PAppCredentialMock;

static AppCredentialMock mAppCredentialMock;
static AppCredential mAppCredential;
static StackQueue mStackQueue;
static RtcCertificate mRtcCertificate;
static SingleListNode mIterator;
static createMutex BackGlobalCreateMutex;

static PAppCredentialMock getAppCredentialMock(void)
{
    return &mAppCredentialMock;
}

static STATUS appQueueCreate_success_callback(PStackQueue* ppStackQueue)
{
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    *ppStackQueue = pAppCredentialMock->pStackQueue;
    return STATUS_SUCCESS;
}

static STATUS appQueueCreate_fail_callback(PStackQueue* ppStackQueue)
{
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    *ppStackQueue = NULL;
    return STATUS_NULL_ARG;
}

static STATUS appQueueGetCount_overflow_callback(PStackQueue pStackQueue, PUINT32 pCount)
{
    *pCount = MAX_RTCCONFIGURATION_CERTIFICATES + 1;
    return STATUS_SUCCESS;
}

static STATUS appQueueGetCount_one_callback(PStackQueue pStackQueue, PUINT32 pCount)
{
    *pCount = 1;
    return STATUS_SUCCESS;
}

static STATUS createRtcCertificate_callback(PRtcCertificate* ppRtcCertificate)
{
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    *ppRtcCertificate = pAppCredentialMock->pAppCredential;
    return STATUS_SUCCESS;
}

static STATUS appQueueGetIterator_callback(PStackQueue pStackQueue, PStackQueueIterator pIterator, int NumCalls)
{
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();

    *pIterator = NULL;

    if (NumCalls <= 1) {
        *pIterator = pAppCredentialMock->pIterator;
    }

    return STATUS_SUCCESS;
}

static STATUS appQueueIteratorNext_callback(PStackQueueIterator pIterator, int NumCalls)
{
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();

    *pIterator = NULL;

    if (NumCalls <= 1) {
        *pIterator = pAppCredentialMock->pIterator;
    }

    return STATUS_SUCCESS;
}

static MUTEX null_createMutex(BOOL reentrant)
{
    return NULL;
}

/* Called before each test method. */
void setUp()
{
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    pAppCredentialMock->pAppCredential = &mAppCredential;
    memset(pAppCredentialMock->pAppCredential, 0, sizeof(AppCredential));
    pAppCredentialMock->pStackQueue = &mStackQueue;
    pAppCredentialMock->pRtcCertificate = &mRtcCertificate;
    pAppCredentialMock->pIterator = &mIterator;
}

/* Called after each test method. */
void tearDown()
{
}

void test_searchSslCert(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    PAppCredential pAppCredential = pAppCredentialMock->pAppCredential;

    remove(APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR "/" APP_CREDENTIAL_UTEST_CACERT_NAME);
    unsetenv(CACERT_PATH_ENV_VAR);

    retStatus = searchSslCert(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    retStatus = searchSslCert(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_MISS_CACERT_PATH, retStatus);

    setenv(CACERT_PATH_ENV_VAR, APP_CREDENTIAL_UTEST_INVALID_CACERT_PATH_ENV_VAR, 1);
    retStatus = searchSslCert(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_INVALID_CACERT_PATH, retStatus);

    setenv(CACERT_PATH_ENV_VAR, APP_CREDENTIAL_UTEST_NULL_CACERT_NAME, 1);
    retStatus = searchSslCert(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_INVALID_CACERT_PATH, retStatus);

    setenv(CACERT_PATH_ENV_VAR, APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR, 1);
    retStatus = searchSslCert(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_CACERT_NOT_FOUND, retStatus);

    int fd2 = open(APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR "/" APP_CREDENTIAL_UTEST_CACERT_NAME, O_RDWR | O_CREAT);
    if (fd2 != -1) {
        close(fd2);
    }
    retStatus = searchSslCert(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
}

void test_generatedCert(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    PAppCredential pAppCredential = pAppCredentialMock->pAppCredential;
    PRtcCertificate pRtcCertificate;

    setenv(ACCESS_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_ACCESS_KEY_ENV_VAR, 1);
    setenv(SECRET_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_SECRET_KEY_ENV_VAR, 1);
    setenv(SESSION_TOKEN_ENV_VAR, APP_CREDENTIAL_UTEST_SESSION_TOKEN_ENV_VAR, 1);

    createAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueCreate_StubWithCallback(appQueueCreate_success_callback);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = generateCertRoutine(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    appQueueGetCount_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = generateCertRoutine(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueGetCount_StubWithCallback(appQueueGetCount_overflow_callback);
    retStatus = generateCertRoutine(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetCount_StubWithCallback(appQueueGetCount_one_callback);
    createRtcCertificate_IgnoreAndReturn(STATUS_APP_CREDENTIAL_CERT_CREATE);
    retStatus = generateCertRoutine(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_CERT_CREATE, retStatus);

    createRtcCertificate_StubWithCallback(createRtcCertificate_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueEnqueue_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = generateCertRoutine(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_CERT_STACK, retStatus);

    appQueueEnqueue_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = generateCertRoutine(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_callback);
    appQueueIteratorGetItem_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueClear_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    freeAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    unsetenv(ACCESS_KEY_ENV_VAR);
    unsetenv(SECRET_KEY_ENV_VAR);
    unsetenv(SESSION_TOKEN_ENV_VAR);
}

void test_popGeneratedCert(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    PAppCredential pAppCredential = pAppCredentialMock->pAppCredential;
    PRtcCertificate pRtcCertificate;

    setenv(ACCESS_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_ACCESS_KEY_ENV_VAR, 1);
    setenv(SECRET_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_SECRET_KEY_ENV_VAR, 1);
    setenv(SESSION_TOKEN_ENV_VAR, APP_CREDENTIAL_UTEST_SESSION_TOKEN_ENV_VAR, 1);

    createAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueCreate_StubWithCallback(appQueueCreate_success_callback);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = popGeneratedCert(NULL, &pRtcCertificate);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    retStatus = popGeneratedCert(pAppCredential, NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    appQueueDequeue_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = popGeneratedCert(pAppCredential, &pRtcCertificate);
    TEST_ASSERT_EQUAL(STATUS_NULL_ARG, retStatus);

    appQueueDequeue_IgnoreAndReturn(STATUS_NOT_FOUND);
    retStatus = popGeneratedCert(pAppCredential, &pRtcCertificate);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueDequeue_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = popGeneratedCert(pAppCredential, &pRtcCertificate);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_callback);
    appQueueIteratorGetItem_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueClear_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    freeAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    unsetenv(ACCESS_KEY_ENV_VAR);
    unsetenv(SECRET_KEY_ENV_VAR);
    unsetenv(SESSION_TOKEN_ENV_VAR);
}

void test_createCredential(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    PAppCredential pAppCredential = pAppCredentialMock->pAppCredential;

    unsetenv(CACERT_PATH_ENV_VAR);
    retStatus = createCredential(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_MISS_CACERT_PATH, retStatus);

    setenv(CACERT_PATH_ENV_VAR, APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(ACCESS_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_ACCESS_KEY_ENV_VAR, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(SECRET_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_SECRET_KEY_ENV_VAR, 1);
    setenv(SESSION_TOKEN_ENV_VAR, APP_CREDENTIAL_UTEST_SESSION_TOKEN_ENV_VAR, 1);
    createAppStaticCredentialProvider_IgnoreAndReturn(STATUS_APP_CREDENTIAL_ALLOCATE_STATIC);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_STATIC, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_callback);
    appQueueIteratorGetItem_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueClear_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_NA, retStatus);

    createAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueCreate_StubWithCallback(appQueueCreate_success_callback);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppStaticCredentialProvider_IgnoreAndReturn(STATUS_APP_CREDENTIAL_DESTROY_STATIC);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_STATIC, retStatus);

    unsetenv(ACCESS_KEY_ENV_VAR);
    unsetenv(SECRET_KEY_ENV_VAR);
    unsetenv(SESSION_TOKEN_ENV_VAR);

    setenv(APP_IOT_CORE_THING_NAME, APP_CREDENTIAL_UTEST_IOT_CORE_THING_NAME, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(APP_IOT_CORE_CREDENTIAL_ENDPOINT, APP_CREDENTIAL_UTEST_IOT_CORE_CREDENTIAL_ENDPOINT, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(APP_IOT_CORE_CERT, APP_CREDENTIAL_UTEST_IOT_CORE_CERT, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(APP_IOT_CORE_PRIVATE_KEY, APP_CREDENTIAL_UTEST_IOT_CORE_PRIVATE_KEY, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(APP_IOT_CORE_ROLE_ALIAS, APP_CREDENTIAL_UTEST_IOT_CORE_ROLE_ALIAS, 1);
    createAppIotCredentialProvider_IgnoreAndReturn(STATUS_APP_CREDENTIAL_ALLOCATE_IOT);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_IOT, retStatus);

    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_NA, retStatus);

    createAppIotCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppIotCredentialProvider_IgnoreAndReturn(STATUS_APP_CREDENTIAL_DESTROY_IOT);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_IOT, retStatus);

    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppIotCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    unsetenv(APP_IOT_CORE_THING_NAME);
    unsetenv(APP_IOT_CORE_CREDENTIAL_ENDPOINT);
    unsetenv(APP_IOT_CORE_CERT);
    unsetenv(APP_IOT_CORE_PRIVATE_KEY);
    unsetenv(APP_IOT_CORE_ROLE_ALIAS);

    setenv(APP_ECS_AUTH_TOKEN, APP_CREDENTIAL_UTEST_ECS_AUTH_TOKEN, 1);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_NA, retStatus);

    setenv(APP_ECS_CREDENTIALS_FULL_URI, APP_CREDENTIAL_UTEST_ECS_CREDENTIALS_FULL_URI, 1);
    createAppEcsCredentialProvider_IgnoreAndReturn(STATUS_APP_CREDENTIAL_ALLOCATE_ECS);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_ALLOCATE_ECS, retStatus);

    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_NA, retStatus);

    createAppEcsCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppEcsCredentialProvider_IgnoreAndReturn(STATUS_APP_CREDENTIAL_DESTROY_ECS);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_ECS, retStatus);

    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    freeAppEcsCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueCreate_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_PREGENERATED_CERT_QUEUE, retStatus);

    unsetenv(APP_ECS_AUTH_TOKEN);
    unsetenv(APP_ECS_CREDENTIALS_FULL_URI);
}

void test_createCredential_null_mutex(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    PAppCredential pAppCredential = pAppCredentialMock->pAppCredential;

    setenv(CACERT_PATH_ENV_VAR, APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR, 1);
    setenv(ACCESS_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_ACCESS_KEY_ENV_VAR, 1);
    setenv(SECRET_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_SECRET_KEY_ENV_VAR, 1);
    setenv(SESSION_TOKEN_ENV_VAR, APP_CREDENTIAL_UTEST_SESSION_TOKEN_ENV_VAR, 1);

    createAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    freeAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    BackGlobalCreateMutex = globalCreateMutex;
    globalCreateMutex = null_createMutex;

    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_INVALID_MUTEX, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_callback);
    appQueueIteratorGetItem_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueClear_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueFree_IgnoreAndReturn(STATUS_SUCCESS);

    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_DESTROY_NA, retStatus);

    globalCreateMutex = BackGlobalCreateMutex;
}

void test_destroyCredential(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppCredentialMock pAppCredentialMock = getAppCredentialMock();
    PAppCredential pAppCredential = pAppCredentialMock->pAppCredential;

    retStatus = destroyCredential(NULL);
    TEST_ASSERT_EQUAL(STATUS_APP_CREDENTIAL_NULL_ARG, retStatus);

    setenv(CACERT_PATH_ENV_VAR, APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR, 1);
    setenv(ACCESS_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_ACCESS_KEY_ENV_VAR, 1);
    setenv(SECRET_KEY_ENV_VAR, APP_CREDENTIAL_UTEST_SECRET_KEY_ENV_VAR, 1);
    setenv(SESSION_TOKEN_ENV_VAR, APP_CREDENTIAL_UTEST_SESSION_TOKEN_ENV_VAR, 1);

    createAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueCreate_StubWithCallback(appQueueCreate_success_callback);
    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueGetIterator_StubWithCallback(appQueueGetIterator_callback);
    appQueueIteratorGetItem_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueIteratorNext_StubWithCallback(appQueueIteratorNext_callback);
    freeRtcCertificate_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueClear_IgnoreAndReturn(STATUS_NULL_ARG);
    appQueueFree_IgnoreAndReturn(STATUS_SUCCESS);
    freeAppStaticCredentialProvider_IgnoreAndReturn(STATUS_SUCCESS);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = createCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    appQueueClear_IgnoreAndReturn(STATUS_SUCCESS);
    appQueueFree_IgnoreAndReturn(STATUS_NULL_ARG);
    retStatus = destroyCredential(pAppCredential);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    remove(APP_CREDENTIAL_UTEST_CACERT_PATH_ENV_VAR "/" APP_CREDENTIAL_UTEST_CACERT_NAME);
}
