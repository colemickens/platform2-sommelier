// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_manager.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "libprotobinder/binder_driver.h"
#include "libprotobinder/binder_host.h"
#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/iinterface.h"
#include "libprotobinder/util.h"

// Generated IDL
#include "libprotobinder/binder.pb.h"

namespace protobinder {

namespace {

BinderManagerInterface* g_binder_manager = nullptr;

}  // namespace

// Information about a binder object that was created by this process.
struct BinderManager::HostInfo {
  explicit HostInfo(BinderHost* host) : host(host), remote_refs(0) {}
  ~HostInfo() = default;

  // Weak pointer to the object that handles transactions.
  BinderHost* host;

  // Number of references to this host held by remote processes, as reported by
  // BR_ACQUIRE and BR_RELEASE messages.
  int remote_refs;
};

// TODO(derat): Reorder these methods to match the header once the changes here
// have settled down.

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

void BinderManager::ReleaseBinderBuffer(const uint8_t* data) {
  VLOG(2) << "Binder free of " << data;
  out_commands_.WriteInt32(BC_FREE_BUFFER);
  out_commands_.WritePointer(reinterpret_cast<uintptr_t>(data));
}

void BinderManager::ReleaseParcel(Parcel* parcel) {
  // TODO(leecam): Close FDs in Parcel.
  ReleaseBinderBuffer(reinterpret_cast<const uint8_t*>(parcel->Data()));
}

// TODO(leecam): Remove the DoBinderReadWriteIoctl
// to reduce number of calls to driver
void BinderManager::IncWeakHandle(uint32_t handle) {
  VLOG(1) << "Incrementing reference count for handle " << handle;
  out_commands_.WriteInt32(BC_INCREFS);
  out_commands_.WriteInt32(handle);
  DoBinderReadWriteIoctl(false);
}

void BinderManager::DecWeakHandle(uint32_t handle) {
  VLOG(1) << "Decrementing reference count for handle " << handle;
  out_commands_.WriteInt32(BC_DECREFS);
  out_commands_.WriteInt32(handle);
  DoBinderReadWriteIoctl(false);
}

void BinderManager::RequestDeathNotification(uint32_t handle) {
  VLOG(1) << "Requesting death notifications for handle " << handle;
  out_commands_.WriteInt32(BC_REQUEST_DEATH_NOTIFICATION);
  out_commands_.WriteInt32(handle);
  out_commands_.WritePointer(handle);
  DoBinderReadWriteIoctl(false);
}

void BinderManager::ClearDeathNotification(uint32_t handle) {
  VLOG(1) << "Clearing death notifications for handle " << handle;
  out_commands_.WriteInt32(BC_CLEAR_DEATH_NOTIFICATION);
  out_commands_.WriteInt32(handle);
  out_commands_.WritePointer(handle);
  DoBinderReadWriteIoctl(false);
}

binder_uintptr_t BinderManager::GetNextBinderHostCookie() {
  const binder_uintptr_t cookie = next_host_cookie_++;
  // Avoid handing out 0 -- that's the context manager.
  CHECK(cookie) << "Host cookie counter wrapped";
  CHECK(!hosts_.count(cookie)) << "Host cookie " << cookie << " already in use";
  return cookie;
}

void BinderManager::RegisterBinderHost(BinderHost* host) {
  DCHECK(host);
  VLOG(1) << "Registering host " << host << " with cookie " << host->cookie();
  CHECK(hosts_.emplace(host->cookie(), HostInfo(host)).second)
      << "Got request to reregister host cookie " << host->cookie();
}

void BinderManager::UnregisterBinderHost(BinderHost* host) {
  DCHECK(host);
  VLOG(1) << "Unregistering host " << host << " with cookie " << host->cookie();

  auto it = hosts_.find(host->cookie());
  CHECK(it != hosts_.end())
      << "Got request to unregister unknown host cookie " << host->cookie();
  it->second.host = nullptr;
  if (!it->second.remote_refs)
    hosts_.erase(it);
}

void BinderManager::RegisterBinderProxy(BinderProxy* proxy) {
  DCHECK(proxy);
  VLOG(1) << "Registering proxy " << proxy << " with handle "
          << proxy->handle();

  if (!proxy->handle())
    return;

  DCHECK(!internal::EraseMultimapEntries(&proxies_, proxy->handle(), proxy))
      << "Got request to reregister proxy " << proxy << " for handle "
      << proxy->handle();
  proxies_.insert(std::make_pair(proxy->handle(), proxy));

  // If this is the first proxy for the handle, add a reference and start
  // listening for death notifications.
  if (proxies_.count(proxy->handle()) == 1) {
    IncWeakHandle(proxy->handle());
    RequestDeathNotification(proxy->handle());
  }
}

void BinderManager::UnregisterBinderProxy(BinderProxy* proxy) {
  DCHECK(proxy);
  VLOG(1) << "Unregistering proxy " << proxy << " with handle "
          << proxy->handle();

  if (!proxy->handle())
    return;

  const size_t num_erased =
      internal::EraseMultimapEntries(&proxies_, proxy->handle(), proxy);
  CHECK_EQ(num_erased, 1U)
      << "Expected exactly one copy of proxy " << proxy << " for handle "
      << proxy->handle() << " when unregistering it";

  // If this was the only proxy for the handle, drop the reference and stop
  // listening for death notifications.
  if (!proxies_.count(proxy->handle())) {
    ClearDeathNotification(proxy->handle());
    DecWeakHandle(proxy->handle());
  }
}

IInterface* BinderManager::CreateTestInterface(const BinderProxy* binder) {
  return nullptr;
}

void BinderManager::AddHostReference(binder_uintptr_t cookie) {
  auto it = hosts_.find(cookie);
  if (it == hosts_.end()) {
    LOG(ERROR) << "Not adding ref for unknown host cookie " << cookie;
    return;
  }
  it->second.remote_refs++;
}

void BinderManager::RemoveHostReference(binder_uintptr_t cookie) {
  auto it = hosts_.find(cookie);
  if (it == hosts_.end()) {
    LOG(ERROR) << "Not removing ref for unknown host cookie " << cookie;
    return;
  }
  it->second.remote_refs--;
  DCHECK_GE(it->second.remote_refs, 0)
      << "Negative refcount for host cookie " << cookie;
  if (!it->second.host && !it->second.remote_refs)
    hosts_.erase(it);
}

Status BinderManager::SendReply(const Parcel& reply, const Status& status) {
  if (!status) {
    Parcel status_reply;
    status.AddToParcel(&status_reply);
    SetUpTransaction(true, -1, 0, status_reply, TF_STATUS_CODE);
  } else {
    SetUpTransaction(true, -1, 0, reply, 0);
  }

  return WaitAndActionReply(NULL);
}

// Process a single command from binder.
void BinderManager::ProcessCommand(uint32_t cmd) {
  uintptr_t ptr = 0;
  uintptr_t cookie = 0;
  switch (cmd) {
    case BR_NOOP:
      break;
    case BR_INCREFS:
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&cookie);
      VLOG(1) << "BR_INCREFS: ptr=" << ptr << " cookie=" << cookie;
      break;
    case BR_DECREFS:
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&cookie);
      VLOG(1) << "BR_DECREFS: ptr=" << ptr << " cookie=" << cookie;
      break;
    case BR_ACQUIRE:
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&cookie);
      VLOG(1) << "BR_ACQUIRE: ptr=" << ptr << " cookie=" << cookie;
      AddHostReference(cookie);
      break;
    case BR_RELEASE:
      in_commands_.ReadPointer(&ptr);
      in_commands_.ReadPointer(&cookie);
      VLOG(1) << "BR_RELEASE: ptr=" << ptr << " cookie=" << cookie;
      RemoveHostReference(cookie);
      break;
    case BR_DEAD_BINDER:
      in_commands_.ReadPointer(&ptr);
      VLOG(1) << "BR_DEAD_BINDER: ptr=" << ptr;
      NotifyProxiesAboutBinderDeath(static_cast<uint32_t>(ptr));
      break;
    case BR_CLEAR_DEATH_NOTIFICATION_DONE:
      in_commands_.ReadPointer(&ptr);
      VLOG(1) << "BR_CLEAR_DEATH_NOTIFICATION_DONE: ptr=" << ptr;
      break;
    case BR_OK:
      VLOG(1) << "BR_OK";
      break;
    case BR_ERROR: {
      VLOG(1) << "BR_ERROR";
      uint32_t error_code = 0;
      in_commands_.ReadUInt32(&error_code);
      LOG(ERROR) << "Received BR_ERROR code " << error_code;
    } break;
    case BR_TRANSACTION: {
      VLOG(1) << "BR_TRANSACTION";
      binder_transaction_data tr;
      if (!in_commands_.Read(&tr, sizeof(tr)))
        LOG(FATAL) << "Binder Transaction contains no data";

      Parcel data;
      if (!data.InitFromBinderTransaction(
              reinterpret_cast<void*>(tr.data.ptr.buffer), tr.data_size,
              reinterpret_cast<binder_size_t*>(tr.data.ptr.offsets),
              tr.offsets_size, base::Bind(&BinderManager::ReleaseParcel,
                                          weak_ptr_factory_.GetWeakPtr()))) {
        LOG(ERROR) << "Failed to create parcel from Transaction";
        break;
      }

      Parcel reply;
      Status status = STATUS_OK();
      if (tr.target.ptr) {
        auto it = hosts_.find(static_cast<binder_uintptr_t>(tr.cookie));
        if (it != hosts_.end() && it->second.host)
          status = it->second.host->Transact(tr.code, &data, &reply, tr.flags);
        else
          status = STATUS_BINDER_ERROR(Status::DRIVER_ERROR);
      }
      if ((tr.flags & TF_ONE_WAY) == 0)
        SendReply(reply, status);
    } break;
    default:
      LOG(FATAL) << "Unknown binder command " << cmd;
  }
}

Status BinderManager::HandleReply(const binder_transaction_data& tr,
                                  Parcel* reply) {
  DCHECK(reply);

  if ((tr.flags & TF_STATUS_CODE) == 0) {
    // This is a successful reply.
    if (!reply->InitFromBinderTransaction(
            reinterpret_cast<void*>(tr.data.ptr.buffer), tr.data_size,
            reinterpret_cast<binder_size_t*>(tr.data.ptr.offsets),
            tr.offsets_size, base::Bind(&BinderManager::ReleaseParcel,
                                        weak_ptr_factory_.GetWeakPtr())))
      return STATUS_BINDER_ERROR(Status::BAD_PARCEL);
    return STATUS_OK();
  }
  // Else this reply contains a Status.
  Parcel status_parcel;
  if (!status_parcel.InitFromBinderTransaction(
          reinterpret_cast<void*>(tr.data.ptr.buffer), tr.data_size,
          reinterpret_cast<binder_size_t*>(tr.data.ptr.offsets),
          tr.offsets_size, base::Bind(&BinderManager::ReleaseParcel,
                                      weak_ptr_factory_.GetWeakPtr())))
    return STATUS_BINDER_ERROR(Status::BAD_PARCEL);

  return Status(&status_parcel);
}

void BinderManager::NotifyProxiesAboutBinderDeath(uint32_t handle) {
  if (!handle) {
    LOG(ERROR) << "Ignoring notification about death of binder " << handle;
    return;
  }
  const auto range = proxies_.equal_range(handle);
  for (auto it = range.first; it != range.second; ++it)
    it->second->HandleDeathNotification();
}

Status BinderManager::WaitAndActionReply(Parcel* reply) {
  // Loop until:
  // * On error and bail.
  // * On Transaction Complete if we don't require a reply.
  // * On reply.
  while (1) {
    DoBinderReadWriteIoctl(true);
    uint32_t cmd = 0;
    if (!in_commands_.ReadUInt32(&cmd)) {
      LOG(FATAL) << "Binder Cmd contains no data";
    }
    switch (cmd) {
      case BR_TRANSACTION_COMPLETE:
        VLOG(1) << "BR_TRANSACTION_COMPLETE";
        if (!reply)
          return STATUS_OK();
        break;
      case BR_DEAD_REPLY:
        VLOG(1) << "BR_DEAD_REPLY";
        return STATUS_BINDER_ERROR(Status::DEAD_ENDPOINT);
      case BR_FAILED_REPLY:
        VLOG(1) << "BR_FAILED_REPLY";
        return STATUS_BINDER_ERROR(Status::FAILED_TRANSACTION);
      case BR_REPLY: {
        VLOG(1) << "BR_REPLY";
        binder_transaction_data tr;
        if (!in_commands_.Read(&tr, sizeof(tr)))
          LOG(FATAL) << "Binder Reply cmd contains no data";
        if (!reply) {
          // We received an unexpected reply. This could be a reply
          // left over from one-way call, where a reply was actually
          // returned. Need to free it and continue to loop looking
          // for a Transaction Complete.
          ReleaseBinderBuffer(
              reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer));
          LOG(WARNING) << "Received unexpected reply";
          break;
        }
        return HandleReply(tr, reply);
      }
      default:
        ProcessCommand(cmd);
    }
  }
  NOTREACHED();
}

void BinderManager::SetUpTransaction(bool is_reply,
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
    LOG(FATAL) << "binder cmd parcel failure";
  if (!out_commands_.Write(&tr, sizeof(tr)))
    LOG(FATAL) << "binder cmd parcel failure";
}

Status BinderManager::Transact(uint32_t handle,
                               uint32_t code,
                               const Parcel& data,
                               Parcel* reply,
                               bool one_way) {
  int flags = TF_ACCEPT_FDS;
  if (one_way) {
    flags |= TF_ONE_WAY;
    reply = nullptr;
  }
  SetUpTransaction(false, handle, code, data, flags);
  return WaitAndActionReply(reply);
}

void BinderManager::DoBinderReadWriteIoctl(bool do_read) {
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
    // Nothing to do.
    return;
  }
  if (driver_->ReadWrite(&bwr) < 0) {
    LOG(FATAL) << "Driver ReadWrite failed";
  }
  VLOG(2) << base::StringPrintf("Binder data R:%lld/%lld W:%lld/%lld",
                                bwr.read_consumed, bwr.read_size,
                                bwr.write_consumed, bwr.write_size);
  if (bwr.read_consumed > 0) {
    in_commands_.SetLen(bwr.read_consumed);
    in_commands_.SetPos(0);
  }
  if (bwr.write_consumed > 0) {
    if (bwr.write_consumed < out_commands_.Len()) {
      // Didn't take all of our data
      LOG(FATAL) << "Binder did not consume all data";
    } else {
      out_commands_.SetLen(0);
      out_commands_.SetPos(0);
    }
    in_commands_.SetLen(bwr.read_consumed);
    in_commands_.SetPos(0);
  }
}

bool BinderManager::GetFdForPolling(int* fd) {
  driver_->SetMaxThreads(0);
  out_commands_.WriteInt32(BC_ENTER_LOOPER);
  *fd = driver_->GetFdForPolling();
  return true;
}

void BinderManager::GetNextCommandAndProcess() {
  DoBinderReadWriteIoctl(true);
  uint32_t cmd = 0;
  if (!in_commands_.ReadUInt32(&cmd))
    LOG(FATAL) << "Binder driver failed to provide a cmd code";
  ProcessCommand(cmd);
}

void BinderManager::HandleEvent() {
  // Process all the commands
  do {
    GetNextCommandAndProcess();
  } while (!in_commands_.IsEmpty());

  DoBinderReadWriteIoctl(false);
}

BinderManager::BinderManager(std::unique_ptr<BinderDriverInterface> driver)
    : driver_(std::move(driver)),
      next_host_cookie_(1),
      weak_ptr_factory_(this) {
  VLOG(1) << "BinderManager created";
  in_commands_.SetCapacity(256);
  out_commands_.SetCapacity(256);
}

BinderManager::~BinderManager() {
  DoBinderReadWriteIoctl(false);
}

}  // namespace protobinder
