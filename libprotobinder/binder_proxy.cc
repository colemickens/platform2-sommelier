// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_proxy.h"

#include <base/logging.h>

#include "libprotobinder/binder.pb.h"
#include "libprotobinder/binder_manager.h"

namespace protobinder {

BinderProxy::BinderProxy(uint32_t handle) : handle_(handle) {
  BinderManagerInterface::Get()->RegisterBinderProxy(this);
}

BinderProxy::~BinderProxy() {
  BinderManagerInterface::Get()->UnregisterBinderProxy(this);
}

void BinderProxy::CopyToProtocolBuffer(StrongBinder* proto) const {
  DCHECK(proto);
  proto->Clear();
  proto->set_proxy_handle(handle_);
}

Status BinderProxy::Transact(uint32_t code,
                             Parcel* data,
                             Parcel* reply,
                             bool one_way) {
  return BinderManagerInterface::Get()->Transact(handle_, code, *data, reply,
                                                 one_way);
}

void BinderProxy::SetDeathCallback(const base::Closure& closure) {
  if (handle_ == 0)
    LOG(DFATAL) << "Cannot get death notifications for context manager";
  death_callback_ = closure;
}

void BinderProxy::HandleDeathNotification() {
  if (handle_ == 0)
    LOG(DFATAL) << "Got death notification for context manager";
  if (!death_callback_.is_null())
    death_callback_.Run();
}

const BinderProxy* BinderProxy::GetBinderProxy() const {
  return this;
}

BinderProxy* BinderProxy::GetBinderProxy() {
  return this;
}

}  // namespace protobinder
