// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/common/util.h"

#include <utility>

#include <protobinder/binder_proxy.h>
#include <protobinder/ibinder.h>

#include "psyche/proto_bindings/psyche.pb.h"

using protobinder::BinderProxy;
using protobinder::IBinder;
using protobinder::StrongBinder;

namespace psyche {
namespace util {

std::unique_ptr<BinderProxy> ExtractBinderProxyFromProto(StrongBinder* proto) {
  // Maybe the proxy already got pulled out of the message.
  if (!proto->ibinder())
    LOG(WARNING) << "ibinder field in proto message is empty";
  std::unique_ptr<BinderProxy> proxy(
      reinterpret_cast<BinderProxy*>(proto->ibinder()));
  proto->set_ibinder(0);
  return std::move(proxy);
}

void CopyBinderToProto(const IBinder& binder, StrongBinder* proto) {
  proto->set_ibinder(reinterpret_cast<uint64_t>(&binder));
  // The |offset| field is required, but it's set by libprotobinder before
  // serialization.
}

}  // namespace util
}  // namespace psyche
