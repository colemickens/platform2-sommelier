// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_manager.h"

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "libprotobinder/binder_driver.h"
#include "libprotobinder/binder_host.h"
#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/iinterface.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

namespace {

BinderManagerInterface* g_binder_manager = nullptr;

}  // namespace

// static
BinderManagerInterface* BinderManagerInterface::Get() {
  if (!g_binder_manager) {
    std::unique_ptr<BinderDriver> driver(new BinderDriver());
    driver->Init();
    g_binder_manager = new BinderManager(std::move(driver));
  }
  return g_binder_manager;
}

// static
void BinderManagerInterface::SetForTesting(
    scoped_ptr<BinderManagerInterface> manager) {
  if (g_binder_manager)
    delete g_binder_manager;
  g_binder_manager = manager.release();
}

void BinderManager::ReleaseBinderBuffer(Parcel* parcel,
                                        const uint8_t* data,
                                        size_t data_size,
                                        const binder_size_t* objects,
                                        size_t objects_size,
                                        void* cookie) {
  VLOG(1) << "Binder free";
  // TODO(leecam): Close FDs in Parcel
  BinderManager* manager =
      static_cast<BinderManager*>(BinderManagerInterface::Get());
  manager->out_commands_.WriteInt32(BC_FREE_BUFFER);
  manager->out_commands_.WritePointer((uintptr_t)data);
}

// TODO(leecam): Remove the DoBinderReadWriteIoctl
// to reduce number of calls to driver
void BinderManager::IncWeakHandle(uint32_t handle) {
  out_commands_.WriteInt32(BC_INCREFS);
  out_commands_.WriteInt32(handle);
  DoBinderReadWriteIoctl(false);
}

void BinderManager::DecWeakHandle(uint32_t handle) {
  out_commands_.WriteInt32(BC_DECREFS);
  out_commands_.WriteInt32(handle);
  DoBinderReadWriteIoctl(false);
}

void BinderManager::RequestDeathNotification(BinderProxy* proxy) {
  DCHECK(proxy);
  out_commands_.WriteInt32(BC_REQUEST_DEATH_NOTIFICATION);
  out_commands_.WriteInt32(proxy->handle());
  out_commands_.WritePointer(reinterpret_cast<uintptr_t>(proxy));
  DoBinderReadWriteIoctl(false);
}

void BinderManager::ClearDeathNotification(BinderProxy* proxy) {
  DCHECK(proxy);
  out_commands_.WriteInt32(BC_CLEAR_DEATH_NOTIFICATION);
  out_commands_.WriteInt32(proxy->handle());
  out_commands_.WritePointer(reinterpret_cast<uintptr_t>(proxy));
  DoBinderReadWriteIoctl(false);
}

IInterface* BinderManager::CreateTestInterface(const IBinder* binder) {
  return nullptr;
}

int BinderManager::SendReply(const Parcel& reply, int error_code) {
  Parcel err_reply;
  int ret = -1;
  if (error_code < 0) {
    err_reply.WriteInt32(error_code);
    ret = SetUpTransaction(true, -1, 0, err_reply, TF_STATUS_CODE);
  } else {
    ret = SetUpTransaction(true, -1, 0, reply, 0);
  }
  if (ret < 0)
    return ret;

  return WaitAndActionReply(NULL);
}

// Process a single command from binder.
int BinderManager::ProcessCommand(uint32_t cmd) {
  uintptr_t ptr = 0;
  switch (cmd) {
    case BR_NOOP:
      break;
    case BR_INCREFS:
      VLOG(1) << "BR_INCREFS";
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&ptr);
      break;
    case BR_DECREFS:
      VLOG(1) << "BR_DECREFS";
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&ptr);
      break;
    case BR_ACQUIRE:
      VLOG(1) << "BR_ACQUIRE";
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&ptr);
      break;
    case BR_RELEASE:
      VLOG(1) << "BR_RELEASE";
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&ptr);
      break;
    case BR_DEAD_BINDER:
      VLOG(1) << "BR_DEAD_BINDER";
      in_commands_.ReadPointer(&ptr);
      if (ptr)
        reinterpret_cast<BinderProxy*>(ptr)->HandleDeathNotification();
      break;
    case BR_CLEAR_DEATH_NOTIFICATION_DONE:
      VLOG(1) << "BR_CLEAR_DEATH_NOTIFICATION_DONE";
      in_commands_.ReadPointer(&ptr);
      break;
    case BR_OK:
      VLOG(1) << "BR_OK";
      break;
    case BR_ERROR: {
      VLOG(1) << "BR_ERROR";
      uint32_t error_code = 0;
      in_commands_.ReadUInt32(&error_code);
      return false;
    } break;
    case BR_TRANSACTION: {
      VLOG(1) << "BR_TRANSACTION";
      binder_transaction_data tr;
      if (!in_commands_.Read(&tr, sizeof(tr)))
        return false;

      Parcel data;
      if (!data.InitFromBinderTransaction(
              reinterpret_cast<void*>(tr.data.ptr.buffer), tr.data_size,
              reinterpret_cast<binder_size_t*>(tr.data.ptr.offsets),
              tr.offsets_size, ReleaseBinderBuffer))
        return false;

      Parcel reply;
      int err = SUCCESS;
      if (tr.target.ptr) {
        BinderHost* binder(reinterpret_cast<BinderHost*>(tr.cookie));
        err = binder->Transact(tr.code, &data, &reply, tr.flags);
      }
      if ((tr.flags & TF_ONE_WAY) == 0)
        SendReply(reply, err);
    } break;
    default:
      LOG(ERROR) << "Unknown command " << cmd;
  }
  return true;
}

int BinderManager::WaitAndActionReply(Parcel* reply) {
  while (1) {
    if (!DoBinderReadWriteIoctl(true))
      break;
    uint32_t cmd = 0;
    if (!in_commands_.ReadUInt32(&cmd)) {
      return ERROR_CMD_PARCEL;
    }
    VLOG(1) << "Got reply command " << cmd;
    switch (cmd) {
      case BR_TRANSACTION_COMPLETE:
        VLOG(1) << "Cmd BR_TRANSACTION_COMPLETE";
        if (!reply)
          return SUCCESS;
        break;
      case BR_DEAD_REPLY:
        VLOG(1) << "Cmd BR_DEAD_REPLY";
        return ERROR_DEAD_ENDPOINT;
      case BR_FAILED_REPLY:
        VLOG(1) << "Cmd BR_FAILED_REPLY";
        return ERROR_FAILED_TRANSACTION;
      case BR_REPLY: {
        VLOG(1) << "Cmd BR_REPLY";
        binder_transaction_data tr;
        if (!in_commands_.Read(&tr, sizeof(tr)))
          return ERROR_REPLY_PARCEL;
        int err = SUCCESS;
        if (reply) {
          if ((tr.flags & TF_STATUS_CODE) == 0) {
            VLOG(1) << "Status code 4";
            if (!reply->InitFromBinderTransaction(
                    reinterpret_cast<void*>(tr.data.ptr.buffer), tr.data_size,
                    reinterpret_cast<binder_size_t*>(tr.data.ptr.offsets),
                    tr.offsets_size, ReleaseBinderBuffer))
              err = ERROR_REPLY_PARCEL;
          } else {
            VLOG(1) << "Status code 1";
            err = *reinterpret_cast<const int*>(tr.data.ptr.buffer);
            VLOG(1) << "Status code 2";
            ReleaseBinderBuffer(
                NULL, reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size / sizeof(binder_size_t), NULL);
          }
        } else {
          ReleaseBinderBuffer(
              NULL, reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
              tr.data_size,
              reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
              tr.offsets_size / sizeof(binder_size_t), NULL);
          continue;
        }
        return err;
      } break;
      default:
        ProcessCommand(cmd);
        break;
    }
  }
  return true;
}

int BinderManager::SetUpTransaction(bool is_reply,
                                    uint32_t handle,
                                    uint32_t code,
                                    const Parcel& data,
                                    uint32_t flags) {
  binder_transaction_data tr;

  tr.target.ptr = 0;
  tr.target.handle = handle;
  tr.code = code;
  tr.flags = flags;
  tr.cookie = 0;

  tr.sender_pid = 0;
  tr.sender_euid = 0;

  tr.data_size = data.Len();
  tr.data.ptr.buffer = reinterpret_cast<binder_uintptr_t>(data.Data());
  tr.offsets_size = data.ObjectCount() * sizeof(binder_size_t);
  tr.data.ptr.offsets = reinterpret_cast<binder_uintptr_t>(data.ObjectData());

  const uint32_t command = is_reply ? BC_REPLY : BC_TRANSACTION;
  if (!out_commands_.WriteInt32(command))
    return ERROR_CMD_PARCEL;
  if (!out_commands_.Write(&tr, sizeof(tr)))
    return ERROR_CMD_PARCEL;
  return SUCCESS;
}

int BinderManager::Transact(uint32_t handle,
                            uint32_t code,
                            const Parcel& data,
                            Parcel* reply,
                            bool one_way) {
  int flags = TF_ACCEPT_FDS;
  if (one_way) {
    flags |= TF_ONE_WAY;
    reply = nullptr;
  }
  const int ret = SetUpTransaction(false, handle, code, data, flags);
  if (ret < 0)
    return ret;
  return WaitAndActionReply(reply);
}

bool BinderManager::DoBinderReadWriteIoctl(bool do_read) {
  // Create a binder_write_read command and send it to binder
  struct binder_write_read bwr;

  // Are there commands from binder we still havent processed?
  const bool outstanding_unprocessed_cmds = !in_commands_.IsEmpty();

  // Setup read params
  if (do_read && !outstanding_unprocessed_cmds) {
    // Caller requested a read and there is no outstanding data.
    bwr.read_size = in_commands_.Capacity();
    bwr.read_buffer = reinterpret_cast<uintptr_t>(in_commands_.Data());
  } else {
    // If there are unprocessed commands, dont get anymore.
    bwr.read_size = 0;
    bwr.read_buffer = 0;
  }

  // Setup write params
  if (do_read && outstanding_unprocessed_cmds) {
    bwr.write_size = 0;
  } else {
    bwr.write_size = out_commands_.Len();
  }
  bwr.write_buffer = reinterpret_cast<uintptr_t>(out_commands_.Data());

  bwr.write_consumed = 0;
  bwr.read_consumed = 0;

  if ((bwr.write_size == 0) && (bwr.read_size == 0)) {
    return true;
  }
  VLOG(1) << "Doing ioctl";
  if (driver_->ReadWrite(&bwr) < 0) {
    LOG(ERROR) << "driver ReadWrite failed";
    return false;
  }
  VLOG(1) << base::StringPrintf("Binder data R:%lld/%lld W:%lld/%lld",
                                bwr.read_consumed, bwr.read_size,
                                bwr.write_consumed, bwr.write_size);
  if (bwr.read_consumed > 0) {
    in_commands_.SetLen(bwr.read_consumed);
    in_commands_.SetPos(0);
  }
  if (bwr.write_consumed > 0) {
    if (bwr.write_consumed < out_commands_.Len()) {
      // Didn't take all of our data
      LOG(ERROR) << "Binder did not consume all data";
      exit(0);
    } else {
      out_commands_.SetLen(0);
      out_commands_.SetPos(0);
    }
    in_commands_.SetLen(bwr.read_consumed);
    in_commands_.SetPos(0);
  }

  return true;
}

bool BinderManager::GetFdForPolling(int* fd) {
  driver_->SetMaxThreads(0);
  out_commands_.WriteInt32(BC_ENTER_LOOPER);
  *fd = driver_->GetFdForPolling();
  return true;
}

bool BinderManager::GetNextCommandAndProcess() {
  DoBinderReadWriteIoctl(true);
  uint32_t cmd = 0;
  if (!in_commands_.ReadUInt32(&cmd))
    return false;
  return ProcessCommand(cmd);
}

bool BinderManager::HandleEvent() {
  // Process all the commands
  do {
    GetNextCommandAndProcess();
  } while (!in_commands_.IsEmpty());

  return DoBinderReadWriteIoctl(false);
}

BinderManager::BinderManager(std::unique_ptr<BinderDriverInterface> driver) {
  VLOG(1) << "BinderManager created";

  driver_ = std::move(driver);

  in_commands_.SetCapacity(256);
  out_commands_.SetCapacity(256);
}

BinderManager::~BinderManager() {
  DoBinderReadWriteIoctl(false);
}

}  // namespace protobinder
