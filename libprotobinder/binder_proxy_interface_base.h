// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_PROXY_INTERFACE_BASE_H_
#define LIBPROTOBINDER_BINDER_PROXY_INTERFACE_BASE_H_

#include <stdint.h>

#include "libprotobinder/ibinder.h"

#define BINDER_EXPORT __attribute__((visibility("default")))

namespace protobinder {

class BINDER_EXPORT BinderProxyInterfaceBase {
 public:
  explicit BinderProxyInterfaceBase(IBinder* binder);
  ~BinderProxyInterfaceBase();

  inline IBinder* Remote() { return remote_; }

 private:
  IBinder* remote_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_PROXY_INTERFACE_BASE_H_
