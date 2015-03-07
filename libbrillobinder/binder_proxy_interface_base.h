// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLOBINDER_BINDER_PROXY_INTERFACE_BASE_H_
#define LIBBRILLOBINDER_BINDER_PROXY_INTERFACE_BASE_H_

#include <stdint.h>

#include "ibinder.h"

#define BINDER_EXPORT __attribute__((visibility("default")))

namespace brillobinder {

class BINDER_EXPORT BinderProxyInterfaceBase {
 public:
  BinderProxyInterfaceBase(IBinder* binder);
  ~BinderProxyInterfaceBase();

  inline IBinder* Remote() { return remote_; }

 private:
  IBinder* remote_;
};
}

#endif