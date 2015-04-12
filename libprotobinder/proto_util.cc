// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/proto_util.h"

#include <base/logging.h>

// IDL definition
#include "libprotobinder/binder.pb.h"

namespace protobinder {

std::unique_ptr<BinderProxy> ExtractBinderFromProto(StrongBinder* proto) {
  // Maybe the proxy already got pulled out of the message.
  if (!proto->ibinder())
    LOG(WARNING) << "ibinder field in proto message is empty";
  std::unique_ptr<BinderProxy> proxy(
      reinterpret_cast<BinderProxy*>(proto->ibinder()));
  proto->set_ibinder(0);
  return std::move(proxy);
}

bool StoreBinderInProto(const IBinder& binder, StrongBinder* proto) {
  proto->set_ibinder(reinterpret_cast<uint64_t>(&binder));
  return true;
}

}  // namespace protobinder
