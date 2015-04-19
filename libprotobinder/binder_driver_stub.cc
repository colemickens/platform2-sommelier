// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_driver_stub.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/logging.h>

#include "libprotobinder/status.h"

namespace protobinder {

const int BinderDriverStub::kReplyVal = 0xDEAD;
const char BinderDriverStub::kReplyString[] = "TEST";

BinderDriverStub::BinderDriverStub() : max_threads_(0) {
}

BinderDriverStub::~BinderDriverStub() {
  if (!user_buffers_.empty())
    LOG(FATAL) << "Not all binder buffers were released";
  for (auto it : handle_refs_) {
    if (it.second != 0)
      LOG(FATAL) << "Not all binder refs were released";
  }
  if (!death_notifications_.empty())
    LOG(FATAL) << "Not all binder death notifications were released";
}

int BinderDriverStub::GetFdForPolling() {
  return 0;
}

int BinderDriverStub::ReadWrite(binder_write_read* buffers) {
  if (buffers->write_size > 0) {
    CHECK(buffers->write_buffer) << "Bad binder write buffer";
    char* buffer = reinterpret_cast<char*>(buffers->write_buffer);

    char* ptr = buffer + buffers->write_consumed;
    char* end = buffer + buffers->write_size;

    while (ptr < end) {
      if ((size_t)(end - ptr) < sizeof(uint32_t))
        LOG(FATAL) << "Not enough data in command buffer";
      uint32_t cmd = *(reinterpret_cast<uint32_t*>(ptr));
      ptr += sizeof(cmd);
      switch (cmd) {
        case BC_TRANSACTION: {
          struct binder_transaction_data* tr = nullptr;
          if ((ptr < end) && (size_t)(end - ptr) < sizeof(*tr))
            LOG(FATAL)
                << "Not enough data in command buffer for BC_TRANSACTION";
          tr = reinterpret_cast<struct binder_transaction_data*>(ptr);
          ptr += sizeof(*tr);
          ProcessTransaction(tr);
          break;
        }
        case BC_REPLY: {
          struct binder_transaction_data* tr = nullptr;
          if ((ptr < end) && (size_t)(end - ptr) < sizeof(*tr))
            LOG(FATAL) << "Not enough data in command buffer for BC_REPLY";
          tr = (struct binder_transaction_data*)ptr;
          ptr += sizeof(*tr);
          memcpy(&last_transaction_data_, tr, sizeof(last_transaction_data_));
          return_cmds_.WriteInt32(BR_TRANSACTION_COMPLETE);
          break;
        }
        case BC_FREE_BUFFER: {
          if ((ptr < end) && (size_t)(end - ptr) < sizeof(void*)) {
            LOG(FATAL)
                << "Not enough data in command buffer for BC_FREE_BUFFER";
          }
          void** buf = reinterpret_cast<void**>(ptr);
          ptr += sizeof(*buf);
          auto it = user_buffers_.find(*buf);
          if (it == user_buffers_.end())
            LOG(FATAL) << "Freeing invalid buffer";
          user_buffers_.erase(it);
          break;
        }
        case BC_INCREFS: {
          if ((ptr < end) && (size_t)(end - ptr) < sizeof(uint32_t))
            LOG(FATAL) << "Not enough data in command buffer for BC_INCREFS";
          uint32_t handle = *(reinterpret_cast<uint32_t*>(ptr));
          ptr += sizeof(handle);
          handle_refs_[handle]++;
          break;
        }
        case BC_DECREFS: {
          if ((ptr < end) && (size_t)(end - ptr) < sizeof(uint32_t))
            LOG(FATAL) << "Not enough data in command buffer for BC_DECREFS";
          uint32_t handle = *(reinterpret_cast<uint32_t*>(ptr));
          ptr += sizeof(uint32_t);
          if (handle_refs_[handle] == 0)
            LOG(FATAL) << "Calling BC_DECREFS with zero refs";
          handle_refs_[handle]--;
          break;
        }
        case BC_REQUEST_DEATH_NOTIFICATION: {
          if ((ptr < end) &&
              (size_t)(end - ptr) < (sizeof(uint32_t) + sizeof(uintptr_t))) {
            LOG(FATAL) << "Not enough data in command buffer for "
                          "BC_REQUEST_DEATH_NOTIFICATION";
          }
          uint32_t handle = *(reinterpret_cast<uint32_t*>(ptr));
          ptr += sizeof(handle);
          uintptr_t cookie = *(reinterpret_cast<uintptr_t*>(ptr));
          ptr += sizeof(cookie);
          death_notifications_[cookie] = handle;
          break;
        }
        case BC_CLEAR_DEATH_NOTIFICATION: {
          if ((ptr < end) &&
              (size_t)(end - ptr) < (sizeof(uint32_t) + sizeof(uintptr_t)))
            LOG(FATAL) << "Not enough data in command buffer for "
                          "BC_CLEAR_DEATH_NOTIFICATION";
          uint32_t handle = *(reinterpret_cast<uint32_t*>(ptr));
          ptr += sizeof(handle);
          uintptr_t cookie = *(reinterpret_cast<uintptr_t*>(ptr));
          ptr += sizeof(cookie);
          auto it = death_notifications_.find(cookie);
          if (it == death_notifications_.end())
            LOG(FATAL) << "BC_CLEAR_DEATH_NOTIFICATION without registering";
          if (it->second != handle)
            LOG(FATAL) << "BC_CLEAR_DEATH_NOTIFICATION bad cookie";
          death_notifications_.erase(it);
          break;
        }
        default:
          LOG(FATAL) << "protobinder sent unknown command " << cmd;
      }
    }

    buffers->write_consumed = ptr - buffer;
  }

  if (buffers->read_size > 0) {
    CHECK(buffers->read_buffer) << "Bad binder read buffer";

    size_t len = buffers->read_size;
    if (return_cmds_.Len() <= buffers->read_size)
      len = return_cmds_.Len();
    else
      LOG(FATAL) << "Return commands did not fit in user buffer";

    memcpy(reinterpret_cast<void*>(buffers->read_buffer), return_cmds_.Data(),
           len);
    buffers->read_consumed = len;
    return_cmds_.SetLen(0);
    return_cmds_.SetPos(0);
  }

  return 0;
}
void BinderDriverStub::SetMaxThreads(int max_threads) {
  max_threads_ = max_threads;
}

struct binder_transaction_data* BinderDriverStub::LastTransactionData() {
  return &last_transaction_data_;
}

int BinderDriverStub::GetRefCount(uint32_t handle) {
  return handle_refs_[handle];
}

bool BinderDriverStub::IsDeathRegistered(uintptr_t cookie, uint32_t handle) {
  auto it = death_notifications_.find(cookie);
  if (it == death_notifications_.end())
    return false;
  if (it->second != handle)
    return false;
  return true;
}

void BinderDriverStub::InjectDeathNotification(uintptr_t cookie) {
  return_cmds_.WriteInt32(BR_DEAD_BINDER);
  return_cmds_.WritePointer(cookie);
}

void BinderDriverStub::InjectTransaction(uintptr_t cookie,
                                         uint32_t code,
                                         const Parcel& data,
                                         bool one_way) {
  // Need to take a copy of the Parcel and add it to list so it can be freed.
  std::unique_ptr<Parcel> transact_parcel(new Parcel);
  transact_parcel->Write(data.Data(), data.Len());

  struct binder_transaction_data transact_data;
  memset(&transact_data, 0, sizeof(transact_data));

  transact_data.data.ptr.buffer = (binder_uintptr_t)transact_parcel->Data();
  transact_data.data_size = transact_parcel->Len();

  transact_data.target.ptr = cookie;
  transact_data.cookie = cookie;
  transact_data.code = code;

  transact_data.flags |= one_way ? TF_ONE_WAY : 0;

  user_buffers_[transact_parcel->Data()] = std::move(transact_parcel);
  return_cmds_.WriteInt32(BR_TRANSACTION);
  return_cmds_.Write(&transact_data, sizeof(transact_data));
}

void BinderDriverStub::ProcessTransaction(struct binder_transaction_data* tr) {
  memcpy(&last_transaction_data_, tr, sizeof(last_transaction_data_));

  if (tr->target.handle == BAD_ENDPOINT) {
    return_cmds_.WriteInt32(BR_DEAD_REPLY);
    return;
  }

  return_cmds_.WriteInt32(BR_TRANSACTION_COMPLETE);

  if ((tr->target.handle == GOOD_ENDPOINT ||
       tr->target.handle == STATUS_ENDPOINT) &&
      ((tr->flags & TF_ONE_WAY) == 0)) {
    std::unique_ptr<Parcel> reply_parcel(new Parcel);
    struct binder_transaction_data reply_data;
    memset(&reply_data, 0, sizeof(reply_data));

    if (tr->target.handle == GOOD_ENDPOINT) {
      reply_parcel->WriteInt32(kReplyVal);
      reply_parcel->WriteString(kReplyString);
    } else {
      reply_data.flags |= TF_STATUS_CODE;
      Status status = STATUS_APP_ERROR(kReplyVal, kReplyString);
      status.AddToParcel(reply_parcel.get());
    }

    reply_data.data.ptr.buffer = (binder_uintptr_t)reply_parcel->Data();
    reply_data.data_size = reply_parcel->Len();

    user_buffers_[reply_parcel->Data()] = std::move(reply_parcel);

    return_cmds_.WriteInt32(BR_REPLY);
    return_cmds_.Write(&reply_data, sizeof(reply_data));
  }
}

}  // namespace protobinder
