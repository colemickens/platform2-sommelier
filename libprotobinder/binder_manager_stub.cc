// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_manager_stub.h"

#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/ibinder.h"
#include "libprotobinder/iinterface.h"

namespace protobinder {

BinderManagerStub::BinderManagerStub() {}

BinderManagerStub::~BinderManagerStub() {}

void BinderManagerStub::ReportBinderDeath(BinderProxy* proxy) {
  CHECK(proxy);
  if (handles_requesting_death_notifications_.count(proxy->handle()))
    proxy->HandleDeathNotification();
}

void BinderManagerStub::SetTestInterface(BinderProxy* proxy,
                                         scoped_ptr<IInterface> interface) {
  CHECK(proxy);
  test_interfaces_[proxy->handle()] = make_linked_ptr(interface.release());
}

int BinderManagerStub::Transact(uint32_t handle,
             uint32_t code,
             const Parcel& data,
             Parcel* reply,
             uint32_t flags) {
  return 0;
}

void BinderManagerStub::IncWeakHandle(uint32_t handle) {}

void BinderManagerStub::DecWeakHandle(uint32_t handle) {}

void BinderManagerStub::EnterLoop() {}

bool BinderManagerStub::GetFdForPolling(int* fd) {
  // TODO(derat): Return an FD that can actually be used here.
  return true;
}

bool BinderManagerStub::HandleEvent() {
  return true;
}

void BinderManagerStub::RequestDeathNotification(BinderProxy* proxy) {
  CHECK(proxy);
  handles_requesting_death_notifications_.insert(proxy->handle());
}

void BinderManagerStub::ClearDeathNotification(BinderProxy* proxy) {
  CHECK(proxy);
  handles_requesting_death_notifications_.erase(proxy->handle());
}

IInterface* BinderManagerStub::CreateTestInterface(const IBinder* binder) {
  const BinderProxy* proxy = binder->GetBinderProxy();
  if (!proxy)
    return nullptr;

  auto it = test_interfaces_.find(proxy->handle());
  if (it == test_interfaces_.end())
    return nullptr;

  linked_ptr<IInterface> interface = it->second;
  test_interfaces_.erase(it);
  return interface.release();
}

}  // namespace protobinder
