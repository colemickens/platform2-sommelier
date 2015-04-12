// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_PROTO_UTIL_H_
#define LIBPROTOBINDER_PROTO_UTIL_H_

#include <memory>

#include "binder_export.h"  // NOLINT(build/include)
#include "binder_proxy.h"   // NOLINT(build/include)

namespace protobinder {

class BinderFd;
class IBinder;
class StrongBinder;

// Use these methods when dealing with the special protos.
// Never extract data from protos directly.
BINDER_EXPORT std::unique_ptr<BinderProxy> ExtractBinderFromProto(
    StrongBinder* proto);

BINDER_EXPORT bool StoreBinderInProto(const IBinder& binder,
                                      StrongBinder* proto);

// TODO(leecam): Add support for FDs

}  // namespace protobinder

#endif  // LIBPROTOBINDER_PROTO_UTIL_H_
