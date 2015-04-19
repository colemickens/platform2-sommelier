// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_MANAGER_H_
#define LIBPROTOBINDER_BINDER_MANAGER_H_

#include <cstdint>
#include <memory>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>

#include "binder_driver.h"  // NOLINT(build/include)
#include "binder_export.h"  // NOLINT(build/include)
#include "parcel.h"         // NOLINT(build/include)
#include "status.h"         // NOLINT(build/include)

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

  virtual Status Transact(uint32_t handle,
                          uint32_t code,
                          const Parcel& data,
                          Parcel* reply,
                          bool one_way) = 0;
  virtual void IncWeakHandle(uint32_t handle) = 0;
  virtual void DecWeakHandle(uint32_t handle) = 0;
  virtual bool GetFdForPolling(int* fd) = 0;
  virtual void HandleEvent() = 0;

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
  explicit BinderManager(std::unique_ptr<BinderDriverInterface> driver);
  ~BinderManager() override;

  // BinderManagerInterface:
  Status Transact(uint32_t handle,
                  uint32_t code,
                  const Parcel& data,
                  Parcel* reply,
                  bool one_way) override;
  void IncWeakHandle(uint32_t handle) override;
  void DecWeakHandle(uint32_t handle) override;
  bool GetFdForPolling(int* fd) override;
  void HandleEvent() override;
  void RequestDeathNotification(BinderProxy* proxy) override;
  void ClearDeathNotification(BinderProxy* proxy) override;
  IInterface* CreateTestInterface(const IBinder* binder) override;

 private:
  Status WaitAndActionReply(Parcel* reply);

  // Writes a command freeing |data|.
  void ReleaseBinderBuffer(const uint8_t* data);

  // Passes |parcel|'s data to ReleaseBinderBuffer().
  void ReleaseParcel(Parcel* parcel);

  void DoBinderReadWriteIoctl(bool do_read);
  void SetUpTransaction(bool is_reply,
                        uint32_t handle,
                        uint32_t code,
                        const Parcel& data,
                        uint32_t flags);
  // Process a single command from binder.
  void ProcessCommand(uint32_t cmd);
  void GetNextCommandAndProcess();
  Status SendReply(const Parcel& reply, const Status& status);
  Status HandleReply(const binder_transaction_data& tr, Parcel* reply);

  // These parcels are used to pass binder ioctl commands to binder.
  // They carry binder command buffers, not to be confused with Parcels
  // used in Transactions which carry user data.
  Parcel out_commands_;
  Parcel in_commands_;

  std::unique_ptr<BinderDriverInterface> driver_;

  // Keep this member last.
  base::WeakPtrFactory<BinderManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BinderManager);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_MANAGER_H_
