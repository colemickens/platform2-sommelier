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

class IInterface;

// Stub implementation of BinderManagerInterface for testing.
class BINDER_EXPORT BinderManagerStub : public BinderManagerInterface {
 public:
  BinderManagerStub();
  ~BinderManagerStub() override;

  // If death notifications have been requested for |proxy|, sends a
  // notification.
  void ReportBinderDeath(BinderProxy* proxy);

  // Ensures that the next CreateTestInterface() call for |proxy| will return
  // |interface|, allowing tests to set their own interfaces for proxies that
  // they've created.
  void SetTestInterface(BinderProxy* proxy,
                        std::unique_ptr<IInterface> interface);

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
  // Handles of BinderProxy objects that have requested death notifications.
  std::set<uint32_t> handles_requesting_death_notifications_;

  // Maps from BinderProxy handles to test interface objects that should be
  // released and returned in response to CreateTestInterface() calls.
  std::map<uint32_t, std::unique_ptr<IInterface>> test_interfaces_;

  // Test interface object that will be released and returned in response to a
  // CreateTestInterface() call with a null IBinder argument.
  std::unique_ptr<IInterface> test_interface_for_null_proxy_;

  DISALLOW_COPY_AND_ASSIGN(BinderManagerStub);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_MANAGER_STUB_H_
