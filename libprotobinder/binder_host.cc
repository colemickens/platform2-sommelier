// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_host.h"

#include <base/logging.h>

#include "libprotobinder/parcel.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

BinderHost::BinderHost() {}

int BinderHost::Transact(uint32_t code,
                         Parcel* data,
                         Parcel* reply,
                         uint32_t flags) {
  const int ret = OnTransact(code, data, reply, flags);
  if (reply)
    reply->SetPos(0);
  return ret;
}

const BinderHost* BinderHost::GetBinderHost() const {
  return this;
}

BinderHost::~BinderHost() {}

int BinderHost::OnTransact(uint32_t code,
                           Parcel* data,
                           Parcel* reply,
                           uint32_t flags) {
  LOG(WARNING) << "OnTransact: Unknown code " << code;
  return ERROR_UNKNOWN_CODE;
}

}  // namespace protobinder
