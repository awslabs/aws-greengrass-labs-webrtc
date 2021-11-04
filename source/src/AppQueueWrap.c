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
#define LOG_CLASS "AppQueueWrap"
#include "AppQueueWrap.h"

STATUS appQueueCreate(PStackQueue* ppStackQueue)
{
    return stackQueueCreate(ppStackQueue);
}

STATUS appQueueGetCount(PStackQueue pStackQueue, PUINT32 pCount)
{
    return stackQueueGetCount(pStackQueue, pCount);
}

STATUS appQueueIsEmpty(PStackQueue pStackQueue, PBOOL pIsEmpty)
{
    return stackQueueIsEmpty(pStackQueue, pIsEmpty);
}

STATUS appQueueEnqueue(PStackQueue pStackQueue, UINT64 item)
{
    return stackQueueEnqueue(pStackQueue, item);
}

STATUS appQueueDequeue(PStackQueue pStackQueue, PUINT64 pItem)
{
    return stackQueueDequeue(pStackQueue, pItem);
}

STATUS appQueueGetIterator(PStackQueue pStackQueue, PStackQueueIterator pIterator)
{
    return stackQueueGetIterator(pStackQueue, pIterator);
}

STATUS appQueueIteratorGetItem(StackQueueIterator iterator, PUINT64 pData)
{
    return stackQueueIteratorGetItem(iterator, pData);
}

STATUS appQueueIteratorNext(PStackQueueIterator pIterator)
{
    return stackQueueIteratorNext(pIterator);
}

STATUS appQueueRemoveItem(PStackQueue pStackQueue, UINT64 item)
{
    return stackQueueRemoveItem(pStackQueue, item);
}

STATUS appQueueClear(PStackQueue pStackQueue, BOOL freeData)
{
    return stackQueueClear(pStackQueue, freeData);
}

STATUS appQueueFree(PStackQueue pStackQueue)
{
    return stackQueueFree(pStackQueue);
}
