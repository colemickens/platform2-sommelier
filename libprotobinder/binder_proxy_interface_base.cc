// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_proxy_interface_base.h"

#include "libprotobinder/binder_manager.h"

namespace protobinder {

BinderProxyInterfaceBase::BinderProxyInterfaceBase(BinderProxy* remote)
    : remote_(remote) {
}

BinderProxyInterfaceBase::~BinderProxyInterfaceBase() {
}

}  // namespace protobinder
