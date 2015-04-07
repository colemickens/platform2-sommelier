// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_COMMON_BINDER_TEST_BASE_H_
#define PSYCHE_COMMON_BINDER_TEST_BASE_H_

#include <memory>

#include <base/macros.h>
#include <gtest/gtest.h>

namespace protobinder {
class BinderManagerStub;
class BinderProxy;
}

namespace psyche {

// Base class for unit tests that exercise code that uses libprotobinder.
// TODO(derat): Move this to a libprotobinder shared testing library.
class BinderTestBase : public testing::Test {
 public:
  BinderTestBase();
  ~BinderTestBase() override;

 protected:
  // Returns a new BinderProxy with a unique handle.
  std::unique_ptr<protobinder::BinderProxy> CreateBinderProxy();

  protobinder::BinderManagerStub* binder_manager_;  // Not owned.

  // Next handle for CreateBinderProxy() to use.
  uint32_t next_proxy_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BinderTestBase);
};

}  // namespace psyche

#endif  // PSYCHE_COMMON_BINDER_TEST_BASE_H_
