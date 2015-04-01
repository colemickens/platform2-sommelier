// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_proxy.h"

#include "libprotobinder/binder_manager.h"

namespace protobinder {

BinderProxy::BinderProxy(uint32_t handle) : handle_(handle) {
  BinderManagerInterface* manager = BinderManagerInterface::Get();
  manager->IncWeakHandle(handle);
  if (handle != 0) {
    manager->RequestDeathNotification(this);
  }
}

BinderProxy::~BinderProxy() {
  BinderManagerInterface* manager = BinderManagerInterface::Get();
  if (handle_ != 0) {
    manager->ClearDeathNotification(this);
  }
  manager->DecWeakHandle(handle_);
}

int BinderProxy::Transact(uint32_t code,
                          Parcel* data,
                          Parcel* reply,
                          uint32_t flags) {
  return BinderManagerInterface::Get()->Transact(
      handle_, code, *data, reply, flags);
}

void BinderProxy::SetDeathCallback(const base::Closure& closure) {
  if (handle_ == 0) {
    LOG(DFATAL) << "Cannot get death notifications for context manager.";
  }
  death_callback_ = closure;
}

void BinderProxy::HandleDeathNotification() {
  if (handle_ == 0) {
    NOTREACHED();
  }
  if (!death_callback_.is_null())
    death_callback_.Run();
}

const BinderProxy* BinderProxy::GetBinderProxy() const {
  return this;
}

}  // namespace protobinder
