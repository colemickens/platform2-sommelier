// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager.h"

#include <string>

#include <base/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_callback.h"
#include "shill/mock_power_manager_proxy.h"
#include "shill/power_manager_proxy_interface.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::_;
using testing::Test;

namespace shill {

typedef MockCallback<void(PowerManager::SuspendState)> MyMockCallback;

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

 protected:
  FakeProxyFactory factory_;
  PowerManager power_manager_;
  PowerManagerProxyDelegate *const delegate_;
};

TEST_F(PowerManagerTest, Add) {
  const string kKey = "Zaphod";
  MyMockCallback *mock_callback = NewMockCallback();
  EXPECT_CALL(*mock_callback, OnRun(PowerManagerProxyDelegate::kOn));
  power_manager_.AddStateChangeCallback(kKey, mock_callback);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
}

TEST_F(PowerManagerTest, AddMultipleRunMultiple) {
  const string kKey1 = "Zaphod";
  MyMockCallback *mock_callback1 = NewMockCallback();
  EXPECT_CALL(*mock_callback1, OnRun(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*mock_callback1, OnRun(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(kKey1, mock_callback1);

  const string kKey2 = "Beeblebrox";
  MyMockCallback *mock_callback2 = NewMockCallback();
  EXPECT_CALL(*mock_callback2, OnRun(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*mock_callback2, OnRun(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(kKey2, mock_callback2);

  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kMem);
}

TEST_F(PowerManagerTest, Remove) {
  const string kKey1 = "Zaphod";
  MyMockCallback *mock_callback1 = NewMockCallback();
  EXPECT_CALL(*mock_callback1, OnRun(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*mock_callback1, OnRun(PowerManagerProxyDelegate::kMem));
  power_manager_.AddStateChangeCallback(kKey1, mock_callback1);

  const string kKey2 = "Beeblebrox";
  MyMockCallback *mock_callback2 = NewMockCallback();
  EXPECT_CALL(*mock_callback2, OnRun(PowerManagerProxyDelegate::kOn));
  EXPECT_CALL(*mock_callback2, OnRun(PowerManagerProxyDelegate::kMem)).Times(0);
  power_manager_.AddStateChangeCallback(kKey2, mock_callback2);

  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);

  power_manager_.RemoveStateChangeCallback(kKey2);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kMem);

  power_manager_.RemoveStateChangeCallback(kKey1);
  factory_.delegate()->OnPowerStateChanged(PowerManagerProxyDelegate::kOn);
}

TEST_F(PowerManagerTest, OnSuspendDelayIgnored) {
  const string kKey = "Zaphod";
  MyMockCallback *mock_callback = NewMockCallback();
  EXPECT_CALL(*mock_callback, OnRun(_)).Times(0);
  power_manager_.AddStateChangeCallback(kKey, mock_callback);
  factory_.delegate()->OnSuspendDelay(99);
}

typedef PowerManagerTest PowerManagerDeathTest;

TEST_F(PowerManagerDeathTest, AddDuplicateKey) {
  const string kKey1 = "Zaphod";
  MyMockCallback *mock_callback1 = NewMockCallback();
  MyMockCallback *mock_callback2 = NewMockCallback();
  power_manager_.AddStateChangeCallback(kKey1, mock_callback1);

  // Adding another callback with the same key is an error.
  EXPECT_DEATH(power_manager_.AddStateChangeCallback(kKey1, mock_callback2),
               "Inserting duplicate key");
}

TEST_F(PowerManagerDeathTest, RemoveUnknownKey) {
  const string kKey1 = "Zaphod";
  const string kKey2 = "Beeblebrox";
  MyMockCallback *mock_callback1 = NewMockCallback();
  power_manager_.AddStateChangeCallback(kKey1, mock_callback1);

  // Attempting to remove a callback key that was not added is an error.
  EXPECT_DEATH(power_manager_.RemoveStateChangeCallback(kKey2),
               "Removing unknown key");
}

}  // namespace shill
