// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_proxy.h"

#include "libprotobinder/binder_manager.h"

namespace protobinder {

BinderProxy::BinderProxy(uint32_t handle) : handle_(handle) {
  BinderManager::GetBinderManager()->IncWeakHandle(handle);
}

BinderProxy::~BinderProxy() {
  BinderManager::GetBinderManager()->DecWeakHandle(handle_);
}

int BinderProxy::Transact(uint32_t code,
                          Parcel* data,
                          Parcel* reply,
                          uint32_t flags) {
  return BinderManager::GetBinderManager()->Transact(handle_, code, *data,
                                                     reply, flags);
}

const BinderProxy* BinderProxy::GetBinderProxy() const {
  return this;
}

}  // namespace protobinder
