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
using std::map;
using std::string;
using testing::_;
using testing::Test;

namespace shill {

namespace {

class FakeProxyFactory : public ProxyFactory {
 public:
  FakeProxyFactory()
      : delegate_(NULL),
        proxy_(new MockPowerManagerProxy) {}

  virtual PowerManagerProxyInterface *CreatePowerManagerProxy(
      PowerManagerProxyDelegate *delegate) {
    delegate_ = delegate;
    return proxy_;
  }
  PowerManagerProxyDelegate *delegate() const { return delegate_; }
  MockPowerManagerProxy *proxy() const { return proxy_; }

 private:
  PowerManagerProxyDelegate *delegate_;
  MockPowerManagerProxy *const proxy_;
};

}  // namespace

class PowerManagerTest : public Test {
 public:
  static const char kKey1[];
  static const char kKey2[];
  static const uint32 kSequence1 = 123;
  static const uint32 kSequence2 = 456;

  PowerManagerTest()
      : power_manager_(&factory_),
        delegate_(factory_.delegate()) {
    state_change_callback1_ =
        Bind(&PowerManagerTest::StateChangeAction1, Unretained(this));
    state_change_callback2_ =
        Bind(&PowerManagerTest::StateChangeAction2, Unretained(this));
    suspend_delay_callback1_ =
        Bind(&PowerManagerTest::SuspendDelayAction1, Unretained(this));
    suspend_delay_callback2_ =
        Bind(&PowerManagerTest::SuspendDelayAction2, Unretained(this));
  }

  MOCK_METHOD1(StateChangeAction1, void(PowerManager::SuspendState));
  MOCK_METHOD1(StateChangeAction2, void(PowerManager::SuspendState));
  MOCK_METHOD1(SuspendDelayAction1, void(uint32));
  MOCK_METHOD1(SuspendDelayAction2, void(uint32));

 protected:
  FakeProxyFactory factory_;
  PowerManager power_manager_;
  PowerManagerProxyDelegate *const delegate_;
  PowerManager::PowerStateCallback state_change_callback1_;
  PowerManager::PowerStateCallback state_change_callback2_;
  PowerManager::SuspendDelayCallback suspend_delay_callback1_;
  PowerManager::SuspendDelayCallback suspend_delay_callback2_;
};

const char PowerManagerTest::kKey1[] = "Zaphod";
const char PowerManagerTest::kKey2[] = "Beeblebrox";

TEST_F(PowerManagerTest, OnPowerStateChanged) {
  EXPECT_EQ(PowerManagerProxyDelegate::kUnknown, power_manager_.power_state());
  power_manager_.OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  EXPECT_EQ(PowerManagerProxyDelegate::kOn, power_manager_.power_state());
}

TEST_F(PowerManagerTest, AddStateChangeCallback) {
  EXPECT_CALL(*this, StateChangeAction1(PowerManagerProxyDelegate::kOn));
  power_manager_.AddStateChangeCallback(kKey1, state_change_callback1_);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  power_manager_.RemoveStateChangeCallback(kKey1);
}

TEST_F(PowerManagerTest, AddSuspendDelayCallback) {
  EXPECT_CALL(*this, SuspendDelayAction1(kSequence1));
  power_manager_.AddSuspendDelayCallback(kKey1, suspend_delay_callback1_);
  factory_.delegate()->OnSuspendDelay(kSequence1);
  power_manager_.RemoveSuspendDelayCallback(kKey1);
}

TEST_F(PowerManagerTest, AddMultipleStateChangeRunMultiple) {
  EXPECT_CALL(*this, StateChangeAction1(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, StateChangeAction1(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(kKey1, state_change_callback1_);

  EXPECT_CALL(*this, StateChangeAction2(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, StateChangeAction2(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(kKey2, state_change_callback2_);

  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kMem);
}

TEST_F(PowerManagerTest, AddMultipleSuspendDelayRunMultiple) {
  EXPECT_CALL(*this, SuspendDelayAction1(kSequence1));
  EXPECT_CALL(*this, SuspendDelayAction1(kSequence2));
  power_manager_.AddSuspendDelayCallback(kKey1, suspend_delay_callback1_);

  EXPECT_CALL(*this, SuspendDelayAction2(kSequence1));
  EXPECT_CALL(*this, SuspendDelayAction2(kSequence2));
  power_manager_.AddSuspendDelayCallback(kKey2, suspend_delay_callback2_);

  factory_.delegate()->OnSuspendDelay(kSequence1);
  factory_.delegate()->OnSuspendDelay(kSequence2);
}

TEST_F(PowerManagerTest, RemoveStateChangeCallback) {
  EXPECT_CALL(*this, StateChangeAction1(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, StateChangeAction1(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(kKey1, state_change_callback1_);

  EXPECT_CALL(*this, StateChangeAction2(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*this, StateChangeAction2(PowerManagerProxyDelegate::kMem))
      .Times(0);
  power_manager_.AddStateChangeCallback(kKey2, state_change_callback2_);

  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);

  power_manager_.RemoveStateChangeCallback(kKey2);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kMem);

  power_manager_.RemoveStateChangeCallback(kKey1);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
}

TEST_F(PowerManagerTest, RemoveSuspendDelayCallback) {
  EXPECT_CALL(*this, SuspendDelayAction1(kSequence1));
  EXPECT_CALL(*this, SuspendDelayAction1(kSequence2));
  power_manager_.AddSuspendDelayCallback(kKey1, suspend_delay_callback1_);

  EXPECT_CALL(*this, SuspendDelayAction2(kSequence1));
  EXPECT_CALL(*this, SuspendDelayAction2(kSequence2)).Times(0);
  power_manager_.AddSuspendDelayCallback(kKey2, suspend_delay_callback2_);

  factory_.delegate()->OnSuspendDelay(kSequence1);

  power_manager_.RemoveSuspendDelayCallback(kKey2);
  factory_.delegate()->OnSuspendDelay(kSequence2);

  power_manager_.RemoveSuspendDelayCallback(kKey1);
  factory_.delegate()->OnSuspendDelay(kSequence1);
}

typedef PowerManagerTest PowerManagerDeathTest;

TEST_F(PowerManagerDeathTest, AddStateChangeCallbackDuplicateKey) {
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::StateChangeAction1, Unretained(this)));

#ifndef NDEBUG
  // Adding another callback with the same key is an error and causes a crash in
  // debug mode.
  EXPECT_DEATH(power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::StateChangeAction2, Unretained(this))),
               "Inserting duplicate key");
#else  // NDEBUG
  EXPECT_CALL(*this, StateChangeAction2(PowerManagerProxyDelegate::kOn));
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::StateChangeAction2, Unretained(this)));
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
#endif  // NDEBUG
}

TEST_F(PowerManagerDeathTest, RemoveStateChangeCallbackUnknownKey) {
  power_manager_.AddStateChangeCallback(
      kKey1, Bind(&PowerManagerTest::StateChangeAction1, Unretained(this)));

#ifndef NDEBUG
  // Attempting to remove a callback key that was not added is an error and
  // crashes in debug mode.
  EXPECT_DEATH(power_manager_.RemoveStateChangeCallback(kKey2),
               "Removing unknown key");
#else  // NDEBUG
  EXPECT_CALL(*this, StateChangeAction1(PowerManagerProxyDelegate::kOn));

  // In non-debug mode, removing an unknown key does nothing.
  power_manager_.RemoveStateChangeCallback(kKey2);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
#endif  // NDEBUG
}

TEST_F(PowerManagerTest, RegisterSuspendDelay) {
  const int kDelay = 100;
  EXPECT_CALL(*factory_.proxy(), RegisterSuspendDelay(kDelay));
  power_manager_.RegisterSuspendDelay(kDelay);
}


}  // namespace shill
