// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_host.h"

#include <base/logging.h>

#include "libprotobinder/parcel.h"

namespace protobinder {

BinderHost::BinderHost() {}

Status BinderHost::Transact(uint32_t code,
                         Parcel* data,
                         Parcel* reply,
                         bool one_way) {
  const Status ret = OnTransact(code, data, reply, one_way);
  if (reply)
    reply->SetPos(0);
  return ret;
}

const BinderHost* BinderHost::GetBinderHost() const {
  return this;
}

BinderHost::~BinderHost() {}

Status BinderHost::OnTransact(uint32_t code,
                           Parcel* data,
                           Parcel* reply,
                           bool one_way) {
  NOTIMPLEMENTED();
  return STATUS_BINDER_ERROR(Status::UNKNOWN_CODE);
}

}  // namespace protobinder
