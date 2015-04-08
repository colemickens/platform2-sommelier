// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_MANAGER_H_
#define LIBPROTOBINDER_BINDER_MANAGER_H_

#include <cstdint>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

#include "binder_export.h"  // NOLINT(build/include)
#include "parcel.h"  // NOLINT(build/include)

namespace protobinder {

class BinderProxy;
class IInterface;

// Interface for a singleton class for communicating using the binder protocol.
class BINDER_EXPORT BinderManagerInterface {
 public:
  // Returns the singleton instance of BinderManagerInterface, creating it if
  // necessary.
  static BinderManagerInterface* Get();

  // Overrides the automatically-created instance returned by Get(). Can be
  // called by tests to install their own BinderManagerStub.
  static void SetForTesting(scoped_ptr<BinderManagerInterface> manager);

  virtual ~BinderManagerInterface() = default;

  virtual int Transact(uint32_t handle,
                       uint32_t code,
                       const Parcel& data,
                       Parcel* reply,
                       bool one_way) = 0;
  virtual void IncWeakHandle(uint32_t handle) = 0;
  virtual void DecWeakHandle(uint32_t handle) = 0;
  virtual bool GetFdForPolling(int* fd) = 0;
  virtual bool HandleEvent() = 0;

  // Creates or clears a request for binder death notifications.
  // End-users should use BinderProxy::SetDeathCallback() instead of calling
  // these methods directly.
  virtual void RequestDeathNotification(BinderProxy* proxy) = 0;
  virtual void ClearDeathNotification(BinderProxy* proxy) = 0;

  // If a test IInterface has been registered for |binder|, returns it.
  // Otherwise, returns nullptr.
  virtual IInterface* CreateTestInterface(const IBinder* binder) = 0;
};

// Real implementation of BinderManagerInterface that communicates with the
// kernel via /dev/binder.
class BINDER_EXPORT BinderManager : public BinderManagerInterface {
 public:
  BinderManager();
  ~BinderManager() override;

  // BinderManagerInterface:
  int Transact(uint32_t handle,
               uint32_t code,
               const Parcel& data,
               Parcel* reply,
               bool one_way) override;
  void IncWeakHandle(uint32_t handle) override;
  void DecWeakHandle(uint32_t handle) override;
  bool GetFdForPolling(int* fd) override;
  bool HandleEvent() override;
  void RequestDeathNotification(BinderProxy* proxy) override;
  void ClearDeathNotification(BinderProxy* proxy) override;
  IInterface* CreateTestInterface(const IBinder* binder) override;

 private:
  int WaitAndActionReply(Parcel* reply);

  static void ReleaseBinderBuffer(Parcel* parcel,
                                  const uint8_t* data,
                                  size_t data_size,
                                  const binder_size_t* objects,
                                  size_t objects_size,
                                  void* cookie);
  bool DoBinderReadWriteIoctl(bool do_read);
  int SetUpTransaction(bool is_reply,
                       uint32_t handle,
                       uint32_t code,
                       const Parcel& data,
                       uint32_t flags);
  // Process a single command from binder.
  int ProcessCommand(uint32_t cmd);
  bool GetNextCommandAndProcess();
  int SendReply(const Parcel& reply, int error_code);

  int binder_fd_;
  void* binder_mapped_address_;

  // These parcels are used to pass binder ioctl commands to binder.
  // They carry binder command buffers, not to be confused with Parcels
  // used in Transactions which carry user data.
  Parcel out_commands_;
  Parcel in_commands_;

  DISALLOW_COPY_AND_ASSIGN(BinderManager);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_MANAGER_H_
