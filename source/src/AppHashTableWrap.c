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
#define LOG_CLASS "AppHashTableWrap"
#include "AppHashTableWrap.h"

STATUS appHashTableCreateWithParams(UINT32 bucketCount, UINT32 bucketLength, PHashTable* ppHashTable)
{
    return hashTableCreateWithParams(bucketCount, bucketLength, ppHashTable);
}

STATUS appHashTableContains(PHashTable pHashTable, UINT64 key, PBOOL pContains)
{
    return hashTableContains(pHashTable, key, pContains);
}

STATUS appHashTableGet(PHashTable pHashTable, UINT64 key, PUINT64 pValue)
{
    return hashTableGet(pHashTable, key, pValue);
}

STATUS appHashTablePut(PHashTable pHashTable, UINT64 key, UINT64 value)
{
    return hashTablePut(pHashTable, key, value);
}

STATUS appHashTableRemove(PHashTable pHashTable, UINT64 key)
{
    return hashTableRemove(pHashTable, key);
}

STATUS appHashTableClear(PHashTable pHashTable)
{
    return hashTableClear(pHashTable);
}

STATUS appHashTableFree(PHashTable pHashTable)
{
    return hashTableFree(pHashTable);
}
