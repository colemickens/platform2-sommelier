// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager.h"

#include <string>

#include <base/bind.h>
#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_power_manager_proxy.h"
#include "shill/power_manager_proxy_interface.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::_;
using testing::Test;

namespace shill {

namespace {

class FakeProxyFactory : public ProxyFactory {
 public:
  FakeProxyFactory() : delegate_(NULL) {}

  virtual PowerManagerProxyInterface *CreatePowerManagerProxy(
      PowerManagerProxyDelegate *delegate) {
    delegate_ = delegate;
    return new MockPowerManagerProxy;
  }
  PowerManagerProxyDelegate *delegate() const { return delegate_; }

 private:
  PowerManagerProxyDelegate *delegate_;
};

}  // namespace

class PowerManagerTest : public Test {
 public:
  PowerManagerTest()
      : power_manager_(&factory_),
        delegate_(factory_.delegate()) { }

  MOCK_METHOD1(TestCallback1, void(PowerManager::SuspendState));
  MOCK_METHOD1(TestCallback2, void(PowerManager::SuspendState));

 protected:
  FakeProxyFactory factory_;
  PowerManager power_manager_;
  PowerManagerProxyDelegate *const delegate_;
};

TEST_F(PowerManagerTest, OnPowerStateChanged) {
  EXPECT_EQ(PowerManagerProxyDelegate::kUnknown, power_manager_.power_state());
  power_manager_.OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  EXPECT_EQ(PowerManagerProxyDelegate::kOn, power_manager_.power_state());
}

TEST_F(PowerManagerTest, Add) {
  const string kKey = "Zaphod";
  EXPECT_CALL(*this, TestCallback1(PowerManagerProxyDelegate::kOn));
  power_manager_.AddStateChangeCallback(
      kKey, Bind(&PowerManagerTest::TestCallback1, Unretained(this)));
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  power_manager_.RemoveStateChangeCallback(kKey);
}

TEST_F(PowerManagerTest, AddMultipleRunMultiple) {
  const string kKey1 = "Zaphod";
  EXPECT_CALL(*this, TestCallback1(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, TestCallback1(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::TestCallback1, Unretained(this)));

  const string kKey2 = "Beeblebrox";
  EXPECT_CALL(*this, TestCallback2(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, TestCallback2(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(
      kKey2, Bind(&PowerManagerTest::TestCallback2, Unretained(this)));

  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kMem);
}

TEST_F(PowerManagerTest, Remove) {
  const string kKey1 = "Zaphod";
  EXPECT_CALL(*this, TestCallback1(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, TestCallback1(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::TestCallback1, Unretained(this)));

  const string kKey2 = "Beeblebrox";
  EXPECT_CALL(*this, TestCallback2(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, TestCallback2(PowerManagerProxyDelegate::kMem)).Times(0);
  power_manager_.AddStateChangeCallback(
      kKey2, Bind(&PowerManagerTest::TestCallback2, Unretained(this)));

  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);

  power_manager_.RemoveStateChangeCallback(kKey2);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kMem);

  power_manager_.RemoveStateChangeCallback(kKey1);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
}

TEST_F(PowerManagerTest, OnSuspendDelayIgnored) {
  const string kKey = "Zaphod";
  EXPECT_CALL(*this, TestCallback1(_)).Times(0);
  power_manager_.AddStateChangeCallback(
      kKey, Bind(&PowerManagerTest::TestCallback1, Unretained(this)));
  factory_.delegate()->OnSuspendDelay(99);
}

typedef PowerManagerTest PowerManagerDeathTest;

TEST_F(PowerManagerDeathTest, AddDuplicateKey) {
  const string kKey1 = "Zaphod";
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::TestCallback1, Unretained(this)));

#ifndef NDEBUG
  // Adding another callback with the same key is an error and causes a crash in
  // debug mode.
  EXPECT_DEATH(power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::TestCallback2, Unretained(this))),
               "Inserting duplicate key");
#else  // NDEBUG
  EXPECT_CALL(*this, TestCallback2(PowerManagerProxyDelegate::kOn));
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::TestCallback2, Unretained(this)));
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
#endif  // NDEBUG
}

TEST_F(PowerManagerDeathTest, RemoveUnknownKey) {
  const string kKey1 = "Zaphod";
  const string kKey2 = "Beeblebrox";
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::TestCallback1, Unretained(this)));

#ifndef NDEBUG
  // Attempting to remove a callback key that was not added is an error and
  // crashes in debug mode.
  EXPECT_DEATH(power_manager_.RemoveStateChangeCallback(kKey2),
               "Removing unknown key");
#else  // NDEBUG
  EXPECT_CALL(*this, TestCallback1(PowerManagerProxyDelegate::kOn));

  // In non-debug mode, removing an unknown key does nothing.
  power_manager_.RemoveStateChangeCallback(kKey2);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
#endif  // NDEBUG
}

}  // namespace shill
