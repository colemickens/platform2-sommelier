// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_MANAGER_STUB_H_
#define LIBPROTOBINDER_BINDER_MANAGER_STUB_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>

#include <base/macros.h>

#include "binder_manager.h"  // NOLINT(build/include)

namespace protobinder {

class BinderHost;
class IInterface;

// Stub implementation of BinderManagerInterface for testing.
class BINDER_EXPORT BinderManagerStub : public BinderManagerInterface {
 public:
  BinderManagerStub();
  ~BinderManagerStub() override;

  // If death notifications have been requested for |proxy_handle|, sends a
  // notification.
  void ReportBinderDeath(uint32_t proxy_handle);

  // Ensures that the next CreateTestInterface() call for a BinderProxy
  // identified by |proxy_handle| will return |interface|, allowing tests to set
  // their own interfaces for handles that they've created. 0 may be passed to
  // set the interface that will be returned if a null proxy is passed.
  void SetTestInterface(uint32_t proxy_handle,
                        std::unique_ptr<IInterface> interface);

  // Returns the BinderHost registered in |hosts_| for |cookie|, or null if a
  // host wasn't registered.
  BinderHost* GetHostForCookie(binder_uintptr_t cookie);

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
  std::map<uint64_t, BinderHost*> hosts_;
  std::multimap<uint32_t, BinderProxy*> proxies_;

  // Maps from BinderProxy handles to test interface objects that should be
  // released and returned in response to CreateTestInterface() calls.
  std::map<uint32_t, std::unique_ptr<IInterface>> test_interfaces_;

  // Test interface object that will be released and returned in response to a
  // CreateTestInterface() call with a null BinderProxy argument.
  std::unique_ptr<IInterface> test_interface_for_null_proxy_;

  // Value to be returned for next call to GetNextBinderHostCookie().
  binder_uintptr_t next_binder_host_cookie_;

  DISALLOW_COPY_AND_ASSIGN(BinderManagerStub);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_MANAGER_STUB_H_
