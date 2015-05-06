// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_manager_stub.h"

#include <base/logging.h>

#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/ibinder.h"
#include "libprotobinder/iinterface.h"
#include "libprotobinder/util.h"

namespace protobinder {

BinderManagerStub::BinderManagerStub() : next_binder_host_cookie_(1) {}

BinderManagerStub::~BinderManagerStub() {}

void BinderManagerStub::ReportBinderDeath(uint32_t proxy_handle) {
  CHECK(proxy_handle);
  const auto range = proxies_.equal_range(proxy_handle);
  for (auto it = range.first; it != range.second; ++it)
    it->second->HandleDeathNotification();
}

void BinderManagerStub::SetTestInterface(
    uint32_t proxy_handle,
    std::unique_ptr<IInterface> interface) {
  if (proxy_handle)
    test_interfaces_[proxy_handle] = std::move(interface);
  else
    test_interface_for_null_proxy_ = std::move(interface);
}

BinderHost* BinderManagerStub::GetHostForCookie(binder_uintptr_t cookie) {
  const auto it = hosts_.find(cookie);
  return it != hosts_.end() ? it->second : nullptr;
}

Status BinderManagerStub::Transact(uint32_t handle,
             uint32_t code,
             const Parcel& data,
             Parcel* reply,
             bool one_way) {
  return STATUS_OK();
}

binder_uintptr_t BinderManagerStub::GetNextBinderHostCookie() {
  return next_binder_host_cookie_++;
}

bool BinderManagerStub::GetFdForPolling(int* fd) {
  // TODO(derat): Return an FD that can actually be used here.
  return true;
}

void BinderManagerStub::HandleEvent() {
}

void BinderManagerStub::RegisterBinderHost(BinderHost* host) {
  CHECK(host);
  CHECK(!hosts_.count(host->cookie()))
      << "Host with cookie " << host->cookie() << " already registered";
  hosts_[host->cookie()] = host;
}

void BinderManagerStub::UnregisterBinderHost(BinderHost* host) {
  CHECK(host);
  CHECK(hosts_.count(host->cookie()))
      << "Host with cookie " << host->cookie() << " not registered";
  hosts_.erase(host->cookie());
}

void BinderManagerStub::RegisterBinderProxy(BinderProxy* proxy) {
  CHECK(proxy);
  CHECK(!internal::EraseMultimapEntries(&proxies_, proxy->handle(), proxy))
      << "Got request to reregister proxy " << proxy << " for handle "
      << proxy->handle();
  proxies_.insert(std::make_pair(proxy->handle(), proxy));
}

void BinderManagerStub::UnregisterBinderProxy(BinderProxy* proxy) {
  CHECK(proxy);
  CHECK_EQ(internal::EraseMultimapEntries(&proxies_, proxy->handle(), proxy),
           1U)
      << "Expected exactly one copy of proxy " << proxy << " for handle "
      << proxy->handle() << " when unregistering it";
}

IInterface* BinderManagerStub::CreateTestInterface(const BinderProxy* binder) {
  if (!binder)
    return test_interface_for_null_proxy_.release();

  const BinderProxy* proxy = binder->GetBinderProxy();
  if (!proxy)
    return nullptr;

  auto it = test_interfaces_.find(proxy->handle());
  if (it == test_interfaces_.end())
    return nullptr;

  std::unique_ptr<IInterface> interface = std::move(it->second);
  test_interfaces_.erase(it);
  return interface.release();
}

}  // namespace protobinder
