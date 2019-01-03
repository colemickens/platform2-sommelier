/*
 * Copyright (C) 2014-2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SharedItemPool.h"

namespace cros {
namespace intel {

template <class ItemType>
SharedItemPool<ItemType>::SharedItemPool(const char*name):
    mAllocated(nullptr),
    mCapacity(0),
    mDeleter(this),
    mTraceReturns(false),
    mName(name),
    mResetter(nullptr)
{}

template <class ItemType>
SharedItemPool<ItemType>::~SharedItemPool()
{
    deInit();
}

template <class ItemType>
status_t SharedItemPool<ItemType>::init(int32_t capacity,  void (*resetter)(ItemType*))
{
    if (mCapacity != 0) {
        LOGE("trying to initialize pool twice ?");
        return INVALID_OPERATION;
    }
    std::lock_guard<std::mutex> l(mMutex);
    mResetter = resetter;
    mCapacity = capacity;
    mAvailable.reserve(capacity);
    mAllocated = new ItemType[capacity];

    for (int32_t i = 0; i < capacity; i++) {
        mAvailable.push_back(&mAllocated[i]);
    }
    LOG1("Shared pool %s init with %d items", mName, capacity);
    return OK;
}

template <class ItemType>
bool SharedItemPool<ItemType>::isFull()
{
    std::lock_guard<std::mutex> l(mMutex);
    bool ret = (mAvailable.size() == mCapacity);
    return ret;
}

template <class ItemType>
status_t SharedItemPool<ItemType>::deInit()
{
    std::lock_guard<std::mutex> l(mMutex);
    if (mCapacity == 0) {
        LOG1("Shared pool %s isn't initialized or already de-initialized",
                mName);
        return OK;
    }
    if (mAvailable.size() != mCapacity) {
        LOGE("Not all items are returned when destroying pool %s (%zu/%zu)!",
                mName, mAvailable.size(), mCapacity);
    }
    delete [] mAllocated;
    mAllocated = nullptr;
    mAvailable.clear();
    mCapacity = 0;
    LOG1("Shared pool %s deinit done.", mName);
    return OK;
}

template <class ItemType>
status_t SharedItemPool<ItemType>::acquireItem(std::shared_ptr<ItemType> &item)
{
    item.reset();
    std::lock_guard<std::mutex> l(mMutex);
    if (mAvailable.empty()) {
        return INVALID_OPERATION;
    }
    std::shared_ptr<ItemType> sh(mAvailable[0], mDeleter);
    mAvailable.erase(mAvailable.begin());
    item = sh;
    LOGP("shared pool %s acquire items %p", mName, sh.get());
    return OK;
}

template <class ItemType>
size_t SharedItemPool<ItemType>::availableItems()
{
    std::lock_guard<std::mutex> l(mMutex);
    size_t ret = mAvailable.size();
    return ret;
}

template <class ItemType>
status_t SharedItemPool<ItemType>::_releaseItem(ItemType *item)
{
    std::lock_guard<std::mutex> l(mMutex);
    if (mResetter)
        mResetter(item);

    LOGP("shared pool %s returning item %p", mName, item);
    if (mTraceReturns) {
    PRINT_BACKTRACE_LINUX();
    }

    mAvailable.push_back(item);
    return OK;
}

} /* namespace intel */
} /* namespace cros */
