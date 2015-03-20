// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/timer/mock_timer.h>
#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "leaderd/group.h"
#include "leaderd/manager.h"

using base::MockTimer;
using base::Timer;
using chromeos::dbus_utils::ExportedObjectManager;
using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::ObjectPath;
using testing::AnyNumber;
using testing::ReturnRef;
using testing::_;

namespace leaderd {

namespace {
const char kObjectManagerPath[] = "/objman";
const char kGroupPath[] = "/objman/group";
const char kGroupName[] = "ABC";
const char kScore = 25;
const char kTestDBusSource[] = "TestDBusSource";
const char kSelfUUID[] = "this is my own uuid";

// Chrome doesn't bother mocking out their objects completely.
class EspeciallyMockedBus : public dbus::MockBus {
 public:
  using dbus::MockBus::MockBus;

  MOCK_METHOD2(GetServiceOwner, void(const std::string& service_name,
                                     const GetServiceOwnerCallback& callback));

  MOCK_METHOD2(ListenForServiceOwnerChange,
               void(const std::string& service_name,
                    const GetServiceOwnerCallback& callback));

  MOCK_METHOD2(UnlistenForServiceOwnerChange,
               void(const std::string& service_name,
                    const GetServiceOwnerCallback& callback));

 protected:
  virtual ~EspeciallyMockedBus() = default;
};

class MockGroupDelegate : public Group::Delegate {
 public:
  MockGroupDelegate() {
    EXPECT_CALL(*this, GetUUID()).WillRepeatedly(ReturnRef(uuid_));
  }

  MOCK_CONST_METHOD0(GetUUID, const std::string&());
  MOCK_METHOD1(RemoveGroup, void(const std::string&));
  MOCK_CONST_METHOD1(GetIPInfo,
                     std::vector<std::tuple<std::vector<uint8_t>, uint16_t>>(
                         const std::string&));

  // The delegate makes us return a reference, not a value.
  const std::string uuid_{kSelfUUID};
};

}  // namespace

class GroupTest : public testing::Test {
 public:
  void SetUp() override {
    // Ignore threading concerns.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, ListenForServiceOwnerChange(kTestDBusSource, _));
    EXPECT_CALL(*bus_, GetServiceOwner(kTestDBusSource, _));

    group_.reset(new Group{kGroupName,
                           bus_,
                           object_manager_.get(),
                           dbus::ObjectPath(kGroupPath),
                           kTestDBusSource,
                           {},
                           &group_delegate_});
    wanderer_timer_ = new MockTimer{false, false};
    heartbeat_timer_ = new MockTimer{true, true};
    group_->ReplaceTimersWithMocksForTest(
        std::unique_ptr<Timer>{wanderer_timer_},
        std::unique_ptr<Timer>{heartbeat_timer_});
  }

  void AssertState(Group::State state, const std::string& leader_id) {
    EXPECT_EQ(group_->state_, state);
    EXPECT_EQ(group_->leader_, leader_id);
  }

  void SetRole(Group::State state, const std::string& leader_id) {
    group_->SetRole(state, leader_id);
  }

  base::MessageLoop message_loop_;
  scoped_refptr<EspeciallyMockedBus> bus_{
      new EspeciallyMockedBus{dbus::Bus::Options{}}};
  std::unique_ptr<ExportedObjectManager> object_manager_{
      new ExportedObjectManager(bus_,
                                dbus::ObjectPath(kObjectManagerPath))};
  MockGroupDelegate group_delegate_;
  std::unique_ptr<Group> group_;
  // We'll fill these in at SetUp time, but the timers are owned by the Group.
  MockTimer* wanderer_timer_{nullptr};
  MockTimer* heartbeat_timer_{nullptr};
};

TEST_F(GroupTest, LeaveGroup) {
  chromeos::ErrorPtr error;
  EXPECT_TRUE(group_->LeaveGroup(&error));
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(GroupTest, SetScore) {
  chromeos::ErrorPtr error;
  LOG(INFO) << "Set score";
  EXPECT_TRUE(group_->SetScore(&error, kScore));
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(GroupTest, ShouldBecomeLeaderWithoutPeers) {
  SetRole(Group::State::WANDERER, "");
  // A couple heartbearts in wanderer state should change
  // nothing if we don't have any peers.
  heartbeat_timer_->Fire();
  AssertState(Group::State::WANDERER, "");
  heartbeat_timer_->Fire();
  AssertState(Group::State::WANDERER, "");
  // And if nothing happens for long enough, we should promote
  // ourselves to leader.
  wanderer_timer_->Fire();
  AssertState(Group::State::LEADER, kSelfUUID);
}

}  // namespace leaderd
