// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binder_proxy.h"

#include <stdint.h>
#include <stdio.h>

#include "binder_manager.h"

namespace brillobinder {

BinderProxy::BinderProxy(uint32_t handle) : handle_(handle) {
  BinderManager::GetBinderManager()->IncWeakHandle(handle);
}

BinderProxy::~BinderProxy() {
  BinderManager::GetBinderManager()->DecWeakHandle(handle_);
}

int BinderProxy::Transact(uint32_t code,
                          Parcel& data,
                          Parcel* reply,
                          uint32_t flags) {
  return BinderManager::GetBinderManager()->Transact(handle_, code, data, reply,
                                                     flags);
}

BinderProxy* BinderProxy::GetBinderProxy() {
  return this;
}
}