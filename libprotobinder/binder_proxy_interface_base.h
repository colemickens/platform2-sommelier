// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_PROXY_INTERFACE_BASE_H_
#define LIBPROTOBINDER_BINDER_PROXY_INTERFACE_BASE_H_

#include "binder_export.h"  // NOLINT(build/include)
#include "ibinder.h"  // NOLINT(build/include)

namespace protobinder {

class BINDER_EXPORT BinderProxyInterfaceBase {
 public:
  explicit BinderProxyInterfaceBase(BinderProxy* remote);
  ~BinderProxyInterfaceBase();

  inline BinderProxy* Remote() { return remote_; }

 private:
  BinderProxy* remote_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_PROXY_INTERFACE_BASE_H_
