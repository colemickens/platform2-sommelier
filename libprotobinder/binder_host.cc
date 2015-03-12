// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_host.h"

#include <stdint.h>
#include <stdio.h>

#include "libprotobinder/parcel.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

BinderHost::BinderHost() {
}

BinderHost::~BinderHost() {
}

int BinderHost::Transact(uint32_t code,
                         const Parcel& data,
                         Parcel* reply,
                         uint32_t flags) {
  int ret;
  ret = OnTransact(code, data, reply, flags);
  if (reply)
    reply->SetPos(0);
  return ret;
}

// Is called by BinderManager...
int BinderHost::OnTransact(uint32_t code,
                           const Parcel& data,
                           Parcel* reply,
                           uint32_t flags) {
  printf("OnTransact: Unknown code%d\n", code);
  return ERROR_UNKNOWN_CODE;
}

BinderHost* BinderHost::GetBinderHost() {
  return this;
}

}  // namespace protobinder
