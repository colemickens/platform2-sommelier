// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_MANAGER_H_
#define LIBPROTOBINDER_BINDER_MANAGER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>

#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>

// binder.h requires types.h to be included first.
#include <sys/types.h>
#include <linux/android/binder.h>  // NOLINT(build/include_alpha)

#include "binder_driver.h"  // NOLINT(build/include)
#include "binder_export.h"  // NOLINT(build/include)
#include "parcel.h"         // NOLINT(build/include)
#include "status.h"         // NOLINT(build/include)

namespace protobinder {

class BinderHost;
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
  virtual bool GetFdForPolling(int* fd) = 0;
  virtual void HandleEvent() = 0;

  // Returns the cookie that should be used to identify a new BinderHost().
  virtual binder_uintptr_t GetNextBinderHostCookie() = 0;

  // Registers or unregisters a mapping from a cookie to a host. Called by
  // BinderHost's constructor and destructor, respectively.
  virtual void RegisterBinderHost(BinderHost* host) = 0;
  virtual void UnregisterBinderHost(BinderHost* host) = 0;

  // Registers or unregisters a proxy. Called by BinderProxy's constructor and
  // destructor, respectively.
  virtual void RegisterBinderProxy(BinderProxy* proxy) = 0;
  virtual void UnregisterBinderProxy(BinderProxy* proxy) = 0;

  // If a test IInterface has been registered for |proxy|, returns it.
  // Otherwise, returns null.
  virtual std::unique_ptr<IInterface> CreateTestInterface(
      const BinderProxy* proxy) = 0;
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
  bool GetFdForPolling(int* fd) override;
  void HandleEvent() override;
  binder_uintptr_t GetNextBinderHostCookie() override;
  void RegisterBinderHost(BinderHost* host) override;
  void UnregisterBinderHost(BinderHost* host) override;
  void RegisterBinderProxy(BinderProxy* proxy) override;
  void UnregisterBinderProxy(BinderProxy* proxy) override;
  std::unique_ptr<IInterface> CreateTestInterface(
      const BinderProxy* proxy) override;

 private:
  struct HostInfo;

  // Adds or removes a remote process's reference from the HostInfo struct
  // identified by |cookie| in |hosts_|. If the reference count reaches zero and
  // the underlying BinderHost object has already been destroyed,
  // RemoveHostReference() removes the struct from |hosts_|.
  void AddHostReference(binder_uintptr_t cookie);
  void RemoveHostReference(binder_uintptr_t cookie);

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

  // Increments or decrements the kernel's reference count for the binder
  // identified by |handle|.
  void IncWeakHandle(uint32_t handle);
  void DecWeakHandle(uint32_t handle);

  // Registers or unregisters a request for death notifications for the binder
  // identified by |handle|.
  void RequestDeathNotification(uint32_t handle);
  void ClearDeathNotification(uint32_t handle);

  // Notifies all of the BinderProxy objects keyed by |handle| in |proxies_|
  // that the host side of their binder has died.
  void NotifyProxiesAboutBinderDeath(uint32_t handle);

  std::unique_ptr<BinderDriverInterface> driver_;

  // These parcels are used to pass binder ioctl commands to binder.
  // They carry binder command buffers, not to be confused with Parcels
  // used in Transactions which carry user data.
  Parcel out_commands_;
  Parcel in_commands_;

  // Value to be returned for next call to GetNextBinderHostCookie().
  binder_uintptr_t next_host_cookie_;

  // Associates cookies with hosts.
  std::map<binder_uintptr_t, HostInfo> hosts_;

  // Associates handles with BinderProxy objects. Note that multiple proxies may
  // be created for a single binder handle.
  std::multimap<uint32_t, BinderProxy*> proxies_;

  // BinderProxy objects that NotifyProxiesAboutBinderDeath() is in the process
  // of notifying. Stored in a member so that UnregisterBinderProxy() can update
  // it if one of the death callbacks happens to destroy a still-scheduled
  // proxy.
  std::set<BinderProxy*> proxies_to_notify_about_death_;

  // Keep this member last.
  base::WeakPtrFactory<BinderManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BinderManager);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_MANAGER_H_
