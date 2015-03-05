// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/manager.h"

#include <string>

#include <base/run_loop.h>
#include <base/message_loop/message_loop.h>
#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_manager.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>
#include "leaderd/mock_peerd_client.h"

using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::ObjectPath;
using chromeos::dbus_utils::ExportedObjectManager;
using testing::AnyNumber;
using testing::Property;
using testing::Return;
using testing::_;

namespace leaderd {

namespace {
const char kGroupName[] = "ABC";
const char kUUIDLessThanMyUUID[] = "123";
const char kMyUUID[] = "456";
const char kUUIDGreaterThanMyUUID[] = "567";
const char kScore = 25;
const char kDbusSource[] = "a.dbus.connection.id";

}  // namespace

class ManagerTest : public testing::Test {
 public:
  void SetUp() override {
    LOG(INFO) << "Start setup";
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_.get(), GetDBusTaskRunner())
        .WillRepeatedly(Return(message_loop_.message_loop_proxy().get()));

    ObjectPath group_path("/org/chromium/leaderd/groups/1");
    group_object_ = new dbus::MockExportedObject(mock_bus_.get(), group_path);
    EXPECT_CALL(*mock_bus_, GetExportedObject(group_path))
        .Times(AnyNumber())
        .WillRepeatedly(Return(group_object_.get()));

    EXPECT_CALL(*group_object_, ExportMethod(_, _, _, _)).Times(AnyNumber());

    LOG(INFO) << "Before create peerd";
    std::unique_ptr<MockPeerdClient> peerd_client(new MockPeerdClient());
    peerd_manager_.reset(new MockManagerInterface());

    EXPECT_CALL(*peerd_client, GetManagerProxy())
        .WillRepeatedly(Return(peerd_manager_.get()));
    EXPECT_CALL(*peerd_manager_, StartMonitoring(_, _, _, _, _))
        .WillRepeatedly(Return(true));

    LOG(INFO) << "Before create manager";
    manager_.reset(
        new Manager{mock_bus_, object_manager_.get(), std::move(peerd_client)});
    LOG(INFO) << "after create manager";
    manager_->uuid_ = kMyUUID;
  }

  bool JoinGroup(chromeos::ErrorPtr* error, const std::string& dbus_client,
                 const std::string& in_group_id,
                 const std::map<std::string, chromeos::Any>& options,
                 dbus::ObjectPath* object_path) {
    LOG(INFO) << "Call join";
    dbus::MethodCall method_call{"org.chromium.leaderd.Manager", "JoinGroup"};
    method_call.SetSender(dbus_client);
    LOG(INFO) << "Before call " << manager_.get();
    return manager_->JoinGroup(error, &method_call, in_group_id, options,
                               object_path);
  }

  base::MessageLoop message_loop_;
  scoped_refptr<MockBus> mock_bus_{new MockBus{dbus::Bus::Options{}}};
  std::unique_ptr<ExportedObjectManager> object_manager_;
  std::unique_ptr<MockManagerInterface> peerd_manager_;
  scoped_refptr<dbus::MockExportedObject> group_object_;
  std::unique_ptr<Manager> manager_;
};

TEST_F(ManagerTest, JoinGroup_RejectEmptyGroup) {
  chromeos::ErrorPtr error;
  dbus::ObjectPath group_path;
  EXPECT_FALSE(JoinGroup(&error, kDbusSource, "", {}, &group_path));
  EXPECT_FALSE(group_path.IsValid());
  EXPECT_NE(nullptr, error.get());
}

TEST_F(ManagerTest, JoinGroup_ShouldAcceptName) {
  chromeos::ErrorPtr error;
  dbus::ObjectPath group_path;
  EXPECT_TRUE(JoinGroup(&error, kDbusSource, kGroupName, {}, &group_path));
  EXPECT_TRUE(group_path.IsValid());
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(ManagerTest, JoinGroup_HandlesMultipleJoins) {
  chromeos::ErrorPtr error;
  dbus::ObjectPath group_path;
  dbus::ObjectPath group_path_second;
  EXPECT_TRUE(JoinGroup(&error, kDbusSource, kGroupName, {}, &group_path));
  EXPECT_TRUE(group_path.IsValid());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(
      JoinGroup(&error, kDbusSource, kGroupName, {}, &group_path_second));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(group_path, group_path_second);
}

TEST_F(ManagerTest, JoinGroup_ChallengeLeaderUnknownGroup) {
  std::string out_leader;
  std::string out_uuid;
  EXPECT_FALSE(manager_->ChallengeLeader(kUUIDLessThanMyUUID, kGroupName,
                                         kScore, &out_leader, &out_uuid));
}

TEST_F(ManagerTest, JoinGroup_ChallengeLeaderKnownGroupDefaultScore) {
  chromeos::ErrorPtr error;
  dbus::ObjectPath group_path;
  EXPECT_TRUE(JoinGroup(&error, kDbusSource, kGroupName, {}, &group_path));
  EXPECT_TRUE(group_path.IsValid());
  EXPECT_EQ(nullptr, error.get());
  std::string out_leader;
  std::string out_uuid;
  // Default score is 0.
  EXPECT_TRUE(manager_->ChallengeLeader(kUUIDLessThanMyUUID, kGroupName, kScore,
                                        &out_leader, &out_uuid));
  EXPECT_EQ(kUUIDLessThanMyUUID, out_leader);
  EXPECT_FALSE(out_uuid.empty());
}

TEST_F(ManagerTest, JoinGroup_ChallengeLeaderKnownGroupDefaultScoreUUIDLess) {
  chromeos::ErrorPtr error;
  dbus::ObjectPath group_path;
  EXPECT_TRUE(JoinGroup(&error, kDbusSource, kGroupName, {}, &group_path));
  EXPECT_TRUE(group_path.IsValid());
  EXPECT_EQ(nullptr, error.get());
  std::string out_leader;
  std::string out_uuid;
  // Default score is 0.
  EXPECT_TRUE(manager_->ChallengeLeader(kUUIDLessThanMyUUID, kGroupName, 0,
                                        &out_leader, &out_uuid));
  EXPECT_EQ(kMyUUID, out_leader);
  EXPECT_FALSE(out_uuid.empty());
}

TEST_F(ManagerTest,
       JoinGroup_ChallengeLeaderKnownGroupDefaultScoreUUIDGreater) {
  chromeos::ErrorPtr error;
  dbus::ObjectPath group_path;
  EXPECT_TRUE(JoinGroup(&error, kDbusSource, kGroupName, {}, &group_path));
  EXPECT_TRUE(group_path.IsValid());
  EXPECT_EQ(nullptr, error.get());
  std::string out_leader;
  std::string out_uuid;
  // Default score is 0.
  EXPECT_TRUE(manager_->ChallengeLeader(kUUIDGreaterThanMyUUID, kGroupName, 0,
                                        &out_leader, &out_uuid));
  EXPECT_EQ(kUUIDGreaterThanMyUUID, out_leader);
  EXPECT_FALSE(out_uuid.empty());
}

}  // namespace leaderd
