// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/proxy_service.h"

#include <gtest/gtest.h>

#include "arc/vm/vsock_proxy/proxy_base.h"

namespace arc {
namespace {

class FakeProxy;
// Track the instance.
FakeProxy* instance = nullptr;

// Fake implementation of ProxyBase to track the state.
class FakeProxy : public ProxyBase {
 public:
  FakeProxy() {
    EXPECT_FALSE(instance);
    instance = this;
  }

  ~FakeProxy() override {
    EXPECT_EQ(instance, this);
    instance = nullptr;
  }

  void Initialize() override { initialized_ = true; }

  bool is_initialized() const { return initialized_; }

  VSockProxy* GetVSockProxy() override { return nullptr; }

 private:
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeProxy);
};

// Fake implementation of ProxyFactory to inject FakeProxy.
class FakeProxyFactory : public ProxyService::ProxyFactory {
 public:
  FakeProxyFactory() = default;
  ~FakeProxyFactory() override = default;

  std::unique_ptr<ProxyBase> Create() override {
    return std::make_unique<FakeProxy>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeProxyFactory);
};

TEST(ProxyServiceTest, Run) {
  {
    ProxyService service(std::make_unique<FakeProxyFactory>());
    ASSERT_TRUE(service.Start());
    ASSERT_TRUE(instance);
    EXPECT_TRUE(instance->is_initialized());
  }
  // Destroying the |service| should destroy the proxy and stop the dedicated
  // thread. Then unblocked.
  EXPECT_FALSE(instance);
}

}  // namespace
}  // namespace arc
