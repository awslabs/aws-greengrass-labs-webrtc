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
#ifndef __KINESIS_VIDEO_WEBRTC_APP_HASHTABLE_WRAP_INCLUDE__
#define __KINESIS_VIDEO_WEBRTC_APP_HASHTABLE_WRAP_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "AppConfig.h"
#include "AppError.h"

STATUS appHashTableCreateWithParams(UINT32 bucketCount, UINT32 bucketLength, PHashTable* ppHashTable);
STATUS appHashTableContains(PHashTable pHashTable, UINT64 key, PBOOL pContains);
STATUS appHashTableGet(PHashTable pHashTable, UINT64 key, PUINT64 pValue);
STATUS appHashTablePut(PHashTable pHashTable, UINT64 key, UINT64 value);
STATUS appHashTableRemove(PHashTable pHashTable, UINT64 key);
STATUS appHashTableClear(PHashTable pHashTable);
STATUS appHashTableFree(PHashTable pHashTable);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_APP_HASHTABLE_WRAP_INCLUDE__ */
