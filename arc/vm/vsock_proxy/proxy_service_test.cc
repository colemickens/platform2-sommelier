// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/proxy_service.h"

#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/proxy_base.h"

namespace arc {
namespace {

// Fake implementation of ProxyBase to track the state.
class FakeProxy : public ProxyBase {
 public:
  FakeProxy() = default;
  ~FakeProxy() override = default;
  FakeProxy(const FakeProxy&) = delete;
  FakeProxy& operator=(const FakeProxy&) = delete;

  bool Initialize() override {
    initialized_ = true;
    return true;
  }

  bool is_initialized() const { return initialized_; }

  VSockProxy* GetVSockProxy() override { return nullptr; }

 private:
  bool initialized_ = false;
};

TEST(ProxyServiceTest, Run) {
  auto instance = new FakeProxy;
  ProxyService service{std::unique_ptr<FakeProxy>(instance)};
  ASSERT_TRUE(service.Start());
  EXPECT_TRUE(instance->is_initialized());
}

}  // namespace
}  // namespace arc
