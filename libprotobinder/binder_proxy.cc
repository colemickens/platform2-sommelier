// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_proxy.h"

#include "libprotobinder/binder_manager.h"

namespace protobinder {

BinderProxy::BinderProxy(uint32_t handle) : handle_(handle) {
  BinderManager* manager = BinderManager::GetBinderManager();
  manager->IncWeakHandle(handle);
  manager->RequestDeathNotification(this);
}

BinderProxy::~BinderProxy() {
  BinderManager* manager = BinderManager::GetBinderManager();
  manager->ClearDeathNotification(this);
  manager->DecWeakHandle(handle_);
}

int BinderProxy::Transact(uint32_t code,
                          Parcel* data,
                          Parcel* reply,
                          uint32_t flags) {
  return BinderManager::GetBinderManager()->Transact(handle_, code, *data,
                                                     reply, flags);
}

void BinderProxy::SetDeathCallback(const base::Closure& closure) {
  death_callback_ = closure;
}

void BinderProxy::HandleDeathNotification() {
  if (!death_callback_.is_null())
    death_callback_.Run();
}

const BinderProxy* BinderProxy::GetBinderProxy() const {
  return this;
}

}  // namespace protobinder
