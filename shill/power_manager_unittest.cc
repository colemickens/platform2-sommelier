// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager.h"

#include <string>

#include <base/bind.h>
#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_event_dispatcher.h"
#include "shill/mock_power_manager_proxy.h"
#include "shill/power_manager_proxy_interface.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::map;
using std::string;
using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::SetArgumentPointee;
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
  static const char kDescription1[];
  static const char kDescription2[];
  static const int kSuspendId1 = 123;
  static const int kSuspendId2 = 456;
  static const int kDelayId1 = 4;
  static const int kDelayId2 = 16;

  PowerManagerTest()
      : kTimeout(base::TimeDelta::FromSeconds(3)),
        power_manager_(&dispatcher_, &factory_),
        proxy_(factory_.proxy()),
        delegate_(factory_.delegate()) {
    suspend_imminent_callback1_ =
        Bind(&PowerManagerTest::SuspendImminentAction1, Unretained(this));
    suspend_imminent_callback2_ =
        Bind(&PowerManagerTest::SuspendImminentAction2, Unretained(this));
    suspend_done_callback1_ =
        Bind(&PowerManagerTest::SuspendDoneAction1, Unretained(this));
    suspend_done_callback2_ =
        Bind(&PowerManagerTest::SuspendDoneAction2, Unretained(this));
  }

  MOCK_METHOD1(SuspendImminentAction1, void(int));
  MOCK_METHOD1(SuspendImminentAction2, void(int));
  MOCK_METHOD1(SuspendDoneAction1, void(int));
  MOCK_METHOD1(SuspendDoneAction2, void(int));

 protected:
  void AddProxyRegisterSuspendDelayExpectation(
      base::TimeDelta timeout,
      const std::string &description,
      int delay_id,
      bool return_value) {
    EXPECT_CALL(*proxy_, RegisterSuspendDelay(timeout, description, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(delay_id),
                        Return(return_value)));
  }

  void AddProxyUnregisterSuspendDelayExpectation(int delay_id,
                                                 bool return_value) {
    EXPECT_CALL(*proxy_, UnregisterSuspendDelay(delay_id))
        .WillOnce(Return(return_value));
  }

  void AddProxyReportSuspendReadinessExpectation(int delay_id,
                                                 int suspend_id,
                                                 bool return_value) {
    EXPECT_CALL(*proxy_, ReportSuspendReadiness(delay_id, suspend_id))
        .WillOnce(Return(return_value));
  }

  void OnSuspendImminent(int suspend_id) {
    EXPECT_CALL(dispatcher_,
                PostDelayedTask(_, PowerManager::kSuspendTimeoutMilliseconds));
    factory_.delegate()->OnSuspendImminent(suspend_id);
    EXPECT_TRUE(power_manager_.suspending());
  }

  void OnSuspendDone(int suspend_id) {
    factory_.delegate()->OnSuspendDone(suspend_id);
    EXPECT_FALSE(power_manager_.suspending());
  }

  void OnSuspendTimeout(int suspend_id) {
    power_manager_.OnSuspendTimeout(suspend_id);
  }

  // This is non-static since it's a non-POD type.
  const base::TimeDelta kTimeout;

  MockEventDispatcher dispatcher_;
  FakeProxyFactory factory_;
  PowerManager power_manager_;
  MockPowerManagerProxy *const proxy_;
  PowerManagerProxyDelegate *const delegate_;
  PowerManager::SuspendImminentCallback suspend_imminent_callback1_;
  PowerManager::SuspendImminentCallback suspend_imminent_callback2_;
  PowerManager::SuspendDoneCallback suspend_done_callback1_;
  PowerManager::SuspendDoneCallback suspend_done_callback2_;
};

const char PowerManagerTest::kKey1[] = "Zaphod";
const char PowerManagerTest::kKey2[] = "Beeblebrox";
const char PowerManagerTest::kDescription1[] = "Foo";
const char PowerManagerTest::kDescription2[] = "Bar";

TEST_F(PowerManagerTest, SuspendingState) {
  const int kSuspendId = 3;
  EXPECT_FALSE(power_manager_.suspending());
  OnSuspendImminent(kSuspendId);
  EXPECT_TRUE(power_manager_.suspending());
  OnSuspendDone(kSuspendId);
  EXPECT_FALSE(power_manager_.suspending());
}

TEST_F(PowerManagerTest, MultipleSuspendDelays) {
  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription1, kDelayId1, true);
  EXPECT_TRUE(power_manager_.AddSuspendDelay(
                  kKey1, kDescription1, kTimeout,
                  suspend_imminent_callback1_, suspend_done_callback1_));
  Mock::VerifyAndClearExpectations(proxy_);

  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription2, kDelayId2, true);
  EXPECT_TRUE(power_manager_.AddSuspendDelay(
                  kKey2, kDescription2, kTimeout,
                  suspend_imminent_callback2_, suspend_done_callback2_));
  Mock::VerifyAndClearExpectations(proxy_);

  EXPECT_CALL(*this, SuspendImminentAction1(kSuspendId1));
  EXPECT_CALL(*this, SuspendImminentAction2(kSuspendId1));
  OnSuspendImminent(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);

  AddProxyReportSuspendReadinessExpectation(kDelayId1, kSuspendId1, true);
  EXPECT_TRUE(power_manager_.ReportSuspendReadiness(kKey1, kSuspendId1));
  Mock::VerifyAndClearExpectations(proxy_);

  AddProxyReportSuspendReadinessExpectation(kDelayId2, kSuspendId2, true);
  EXPECT_TRUE(power_manager_.ReportSuspendReadiness(kKey2, kSuspendId2));
  Mock::VerifyAndClearExpectations(proxy_);

  EXPECT_CALL(*this, SuspendDoneAction1(kSuspendId1));
  EXPECT_CALL(*this, SuspendDoneAction2(kSuspendId1));
  OnSuspendDone(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);

  AddProxyUnregisterSuspendDelayExpectation(kDelayId1, true);
  EXPECT_TRUE(power_manager_.RemoveSuspendDelay(kKey1));
  Mock::VerifyAndClearExpectations(proxy_);

  AddProxyUnregisterSuspendDelayExpectation(kDelayId2, true);
  EXPECT_TRUE(power_manager_.RemoveSuspendDelay(kKey2));
  Mock::VerifyAndClearExpectations(proxy_);

  // Start a second suspend attempt with no registered delays.
  OnSuspendImminent(kSuspendId2);
  OnSuspendDone(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(PowerManagerTest, RegisterSuspendDelayFailure) {
  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription1, kDelayId1, false);
  EXPECT_FALSE(power_manager_.AddSuspendDelay(
                   kKey1, kDescription1, kTimeout,
                   suspend_imminent_callback1_, suspend_done_callback1_));
  Mock::VerifyAndClearExpectations(proxy_);

  // No callbacks should be invoked.
  OnSuspendImminent(kSuspendId1);
  OnSuspendDone(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(PowerManagerTest, ReportSuspendReadinessFailure) {
  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription1, kDelayId1, true);
  EXPECT_TRUE(power_manager_.AddSuspendDelay(
                  kKey1, kDescription1, kTimeout,
                  suspend_imminent_callback1_, suspend_done_callback1_));
  Mock::VerifyAndClearExpectations(proxy_);

  AddProxyReportSuspendReadinessExpectation(kDelayId1, kSuspendId1, false);
  EXPECT_FALSE(power_manager_.ReportSuspendReadiness(kKey1, kSuspendId1));
  Mock::VerifyAndClearExpectations(proxy_);
}

TEST_F(PowerManagerTest, UnregisterSuspendDelayFailure) {
  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription1, kDelayId1, true);
  EXPECT_TRUE(power_manager_.AddSuspendDelay(
                  kKey1, kDescription1, kTimeout,
                  suspend_imminent_callback1_, suspend_done_callback1_));
  Mock::VerifyAndClearExpectations(proxy_);

  // Make the proxy report failure for unregistering.
  AddProxyUnregisterSuspendDelayExpectation(kDelayId1, false);
  EXPECT_FALSE(power_manager_.RemoveSuspendDelay(kKey1));
  Mock::VerifyAndClearExpectations(proxy_);

  // As a result, the callback should still be invoked.
  EXPECT_CALL(*this, SuspendImminentAction1(kSuspendId1));
  OnSuspendImminent(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);

  // Let the unregister request complete successfully now.
  AddProxyUnregisterSuspendDelayExpectation(kDelayId1, true);
  EXPECT_TRUE(power_manager_.RemoveSuspendDelay(kKey1));
  Mock::VerifyAndClearExpectations(proxy_);
}

TEST_F(PowerManagerTest, DuplicateKey) {
  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription1, kDelayId1, true);
  EXPECT_TRUE(power_manager_.AddSuspendDelay(
                  kKey1, kDescription1, kTimeout,
                  suspend_imminent_callback1_, suspend_done_callback1_));
  Mock::VerifyAndClearExpectations(proxy_);

  // The new request should be ignored.
  EXPECT_FALSE(power_manager_.AddSuspendDelay(
                   kKey1, kDescription2, kTimeout,
                   suspend_imminent_callback2_, suspend_done_callback2_));

  // The first delay's callback should be invoked.
  EXPECT_CALL(*this, SuspendImminentAction1(kSuspendId1));
  OnSuspendImminent(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(PowerManagerTest, UnknownKey) {
  EXPECT_FALSE(power_manager_.RemoveSuspendDelay(kKey1));
  Mock::VerifyAndClearExpectations(proxy_);
}

TEST_F(PowerManagerTest, OnSuspendTimeout) {
  AddProxyRegisterSuspendDelayExpectation(
      kTimeout, kDescription1, kDelayId1, true);
  EXPECT_TRUE(power_manager_.AddSuspendDelay(
                  kKey1, kDescription1, kTimeout,
                  suspend_imminent_callback1_, suspend_done_callback1_));
  Mock::VerifyAndClearExpectations(proxy_);

  EXPECT_CALL(*this, SuspendImminentAction1(kSuspendId1));
  OnSuspendImminent(kSuspendId1);
  Mock::VerifyAndClearExpectations(this);

  // After the timeout, the "done" callback should be run.
  EXPECT_CALL(*this, SuspendDoneAction1(kSuspendId1));
  OnSuspendTimeout(kSuspendId1);
  EXPECT_FALSE(power_manager_.suspending());
  Mock::VerifyAndClearExpectations(this);
}

}  // namespace shill
