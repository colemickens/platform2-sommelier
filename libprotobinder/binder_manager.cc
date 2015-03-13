// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// Out of order due to this bad header requiring sys/types.h
#include <linux/android/binder.h>

#include "libprotobinder/binder_host.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

namespace {

const size_t kBinderMappedSize = (1 * 1024 * 1024) - (4096 * 2);

BinderManager* g_binder_manager = NULL;

}  // namespace

BinderManager* BinderManager::GetBinderManager() {
  if (!g_binder_manager)
    g_binder_manager = new BinderManager();
  return g_binder_manager;
}

void BinderManager::ReleaseBinderBuffer(Parcel* parcel,
                                        const uint8_t* data,
                                        size_t data_size,
                                        const binder_size_t* objects,
                                        size_t objects_size,
                                        void* cookie) {
  printf("Binder Free\n");
  // TODO(leecam): Close FDs in Parcel
  GetBinderManager()->in_commands_.WriteInt32(BC_FREE_BUFFER);
  GetBinderManager()->in_commands_.WritePointer((uintptr_t)data);
}

void BinderManager::IncWeakHandle(uint32_t handle) {
  in_commands_.WriteInt32(BC_INCREFS);
  in_commands_.WriteInt32(handle);
}

void BinderManager::DecWeakHandle(uint32_t handle) {
  in_commands_.WriteInt32(BC_DECREFS);
  in_commands_.WriteInt32(handle);
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
  switch (cmd) {
    case BR_NOOP:
      break;
    case BR_INCREFS:
      printf("BR_INCREFS\n");
      in_commands_.ReadPointer();
      in_commands_.ReadPointer();
      break;
    case BR_DECREFS:
      printf("BR_DECREFS\n");
      in_commands_.ReadPointer();
      in_commands_.ReadPointer();
      break;
    case BR_ACQUIRE:
      printf("BR_ACQUIRE\n");
      in_commands_.ReadPointer();
      in_commands_.ReadPointer();
      break;
    case BR_RELEASE:
      printf("BR_RELEASE\n");
      in_commands_.ReadPointer();
      in_commands_.ReadPointer();
      break;
    case BR_OK:
      printf("BR_OK\n");
      break;
    case BR_ERROR:
      printf("BR_ERROR\n");
      in_commands_.ReadInt32();
      return false;
      break;
    case BR_TRANSACTION: {
      printf("BR_TRANSACTION\n");
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
        err = binder->Transact(tr.code, data, &reply, tr.flags);
      }
      if ((tr.flags & TF_ONE_WAY) == 0)
        SendReply(reply, err);
    } break;
    default:
      printf("Unknown Command\n");
  }
  return true;
}

int BinderManager::WaitAndActionReply(Parcel* reply) {
  while (1) {
    if (!DoBinderReadWriteIoctl(true))
      break;
    const uint32_t cmd = in_commands_.ReadInt32();
    printf("Got reply command %x\n", cmd);
    switch (cmd) {
      case BR_TRANSACTION_COMPLETE:
        printf("Cmd BR_TRANSACTION_COMPLETE\n");
        if (!reply)
          return SUCCESS;
        break;
      case BR_DEAD_REPLY:
        printf("Cmd BR_DEAD_REPLY\n");
        return ERROR_DEAD_ENDPOINT;
      case BR_FAILED_REPLY:
        printf("Cmd BR_FAILED_REPLY\n");
        return ERROR_FAILED_TRANSACTION;
      case BR_REPLY: {
        printf("Cmd BR_REPLY\n");
        binder_transaction_data tr;
        if (!in_commands_.Read(&tr, sizeof(tr)))
          return ERROR_REPLY_PARCEL;
        int err = SUCCESS;
        if (reply) {
          if ((tr.flags & TF_STATUS_CODE) == 0) {
            printf("Status code4\n");
            if (!reply->InitFromBinderTransaction(
                    reinterpret_cast<void*>(tr.data.ptr.buffer), tr.data_size,
                    reinterpret_cast<binder_size_t*>(tr.data.ptr.offsets),
                    tr.offsets_size, ReleaseBinderBuffer))
              err = ERROR_REPLY_PARCEL;
          } else {
            printf("Status code1\n");
            err = *reinterpret_cast<const int*>(tr.data.ptr.buffer);
            printf("Status code2\n");
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
                            uint32_t flags) {
  flags |= TF_ACCEPT_FDS;
  const int ret = SetUpTransaction(false, handle, code, data, flags);
  if (ret < 0)
    return ret;
  return WaitAndActionReply(reply);
}

bool BinderManager::WriteCmd(void* data, size_t len) {
  struct binder_write_read bwr;

  bwr.write_size = len;
  bwr.write_consumed = 0;
  bwr.write_buffer = reinterpret_cast<uintptr_t>(data);
  bwr.read_size = 0;
  bwr.read_consumed = 0;
  bwr.read_buffer = 0;

  if (ioctl(binder_fd_, BINDER_WRITE_READ, &bwr) == 0)
    return true;
  return false;
}

bool BinderManager::DoBinderReadWriteIoctl(bool do_read) {
  if (binder_fd_ < 0)
    return false;
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
  printf("Doing ioctl\n");
  if (ioctl(binder_fd_, BINDER_WRITE_READ, &bwr) < 0) {
    printf("Binder Failed\n");
    return false;
  }
  printf("Binder Data R:%lld/%lld W:%lld/%lld\n", bwr.read_consumed,
         bwr.read_size, bwr.write_consumed, bwr.write_size);
  if (bwr.read_consumed > 0) {
    in_commands_.SetLen(bwr.read_consumed);
    in_commands_.SetPos(0);
  }
  if (bwr.write_consumed > 0) {
    if (bwr.write_consumed < out_commands_.Len()) {
      // Didnt take all of our data
      printf("BAD BAD BAD!\n");
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
  if (binder_fd_ < 0)
    return false;
  in_commands_.WriteInt32(BC_ENTER_LOOPER);
  *fd = binder_fd_;
  return true;
}

bool BinderManager::GetNextCommandAndProcess() {
  DoBinderReadWriteIoctl(true);
  uint32_t cmd = in_commands_.ReadInt32();
  return ProcessCommand(cmd);
}

bool BinderManager::HandleEvent() {
  // Process all the commands
  do {
    GetNextCommandAndProcess();
  } while (!in_commands_.IsEmpty());

  return DoBinderReadWriteIoctl(false);
}

void BinderManager::EnterLoop() {
  printf("Entering loop\n");
  struct binder_write_read bwr;
  uint32_t readbuf[32];

  bwr.write_size = 0;
  bwr.write_consumed = 0;
  bwr.write_buffer = 0;

  readbuf[0] = BC_ENTER_LOOPER;
  WriteCmd(readbuf, sizeof(readbuf[0]));

  while (true) {
    bwr.read_size = sizeof(readbuf);
    bwr.read_consumed = 0;
    bwr.read_buffer = reinterpret_cast<uintptr_t>(readbuf);

    // wait or a reply
    printf("Waiting for message\n");
    const int ret = ioctl(binder_fd_, BINDER_WRITE_READ, &bwr);
    if (ret < 0) {
      printf("Loop ioctl failed %d\n", ret);
      break;
    }
    printf("Got a reply!\n");
  }
}

BinderManager::BinderManager() {
  printf("Binder Manager Created\n");

  binder_fd_ = open("/dev/binder", O_RDWR);
  if (binder_fd_ < 0) {
    printf("Failed to open binder\n");
    // TODO(leecam): Could die here?
    return;
  }
  // Check the version
  struct binder_version vers;
  if ((ioctl(binder_fd_, BINDER_VERSION, &vers) == -1) ||
      (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
    printf("binder driver miss match\n");
    close(binder_fd_);
    binder_fd_ = -1;
    return;
  }

  // mmap the user buffer
  binder_mapped_address_ = mmap(NULL, kBinderMappedSize, PROT_READ,
                                MAP_PRIVATE | MAP_NORESERVE, binder_fd_, 0);
  if (binder_mapped_address_ == MAP_FAILED) {
    printf("Failed to mmap binder\n");
    close(binder_fd_);
    binder_fd_ = -1;
    return;
  }

  // TODO(leecam): Check these and set binder_fd_
  // to -1 on failure.
  in_commands_.SetCapacity(256);
  out_commands_.SetCapacity(256);
}

}  // namespace protobinder
