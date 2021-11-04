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
#ifndef __KINESIS_VIDEO_WEBRTC_APP_QUEUE_WRAP_INCLUDE__
#define __KINESIS_VIDEO_WEBRTC_APP_QUEUE_WRAP_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "AppConfig.h"
#include "AppError.h"

STATUS appQueueCreate(PStackQueue* ppStackQueue);
STATUS appQueueGetCount(PStackQueue pStackQueue, PUINT32 pCount);
STATUS appQueueIsEmpty(PStackQueue pStackQueue, PBOOL pIsEmpty);
STATUS appQueueEnqueue(PStackQueue pStackQueue, UINT64 item);
STATUS appQueueDequeue(PStackQueue pStackQueue, PUINT64 pItem);
STATUS appQueueGetIterator(PStackQueue pStackQueue, PStackQueueIterator pIterator);
STATUS appQueueIteratorGetItem(StackQueueIterator iterator, PUINT64 pData);
STATUS appQueueIteratorNext(PStackQueueIterator pIterator);
STATUS appQueueRemoveItem(PStackQueue pStackQueue, UINT64 item);
STATUS appQueueClear(PStackQueue pStackQueue, BOOL freeData);
STATUS appQueueFree(PStackQueue pStackQueue);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_APP_QUEUE_WRAP_INCLUDE__ */
