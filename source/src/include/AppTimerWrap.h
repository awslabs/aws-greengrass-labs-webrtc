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
#ifndef __KINESIS_VIDEO_WEBRTC_APP_TIMER_WRAP_INCLUDE__
#define __KINESIS_VIDEO_WEBRTC_APP_TIMER_WRAP_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "AppConfig.h"
#include "AppError.h"

STATUS appTimerQueueCreate(PTIMER_QUEUE_HANDLE pHandle);
STATUS appTimeQueueAdd(TIMER_QUEUE_HANDLE handle, UINT64 start, UINT64 period, TimerCallbackFunc timerCallbackFn, UINT64 customData, PUINT32 pIndex);
STATUS appTimerQueueCancel(TIMER_QUEUE_HANDLE handle, UINT32 timerId, UINT64 customData);
STATUS appTimerQueueFree(PTIMER_QUEUE_HANDLE pHandle);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_APP_TIMER_WRAP_INCLUDE__ */
