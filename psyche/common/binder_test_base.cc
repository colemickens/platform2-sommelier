// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/common/binder_test_base.h"

#include <base/memory/scoped_ptr.h>
#include <protobinder/binder_manager.h>
#include <protobinder/binder_manager_stub.h>
#include <protobinder/binder_proxy.h>

using protobinder::BinderManagerInterface;
using protobinder::BinderManagerStub;
using protobinder::BinderProxy;

namespace psyche {

BinderTestBase::BinderTestBase()
    : binder_manager_(new BinderManagerStub),
      next_proxy_handle_(1) {
  BinderManagerInterface::SetForTesting(
      scoped_ptr<BinderManagerInterface>(binder_manager_));
}

BinderTestBase::~BinderTestBase() {
  BinderManagerInterface::SetForTesting(
      scoped_ptr<BinderManagerInterface>());
}

std::unique_ptr<BinderProxy> BinderTestBase::CreateBinderProxy() {
  return std::unique_ptr<BinderProxy>(new BinderProxy(next_proxy_handle_++));
}

}  // namespace psyche
