// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "binder_proxy_interface_base.h"

#include <stdint.h>
#include <stdio.h>

#include "binder_manager.h"

namespace brillobinder {

BinderProxyInterfaceBase::BinderProxyInterfaceBase(IBinder* binder) {
  remote_ = binder;
}

BinderProxyInterfaceBase::~BinderProxyInterfaceBase() {
}
}