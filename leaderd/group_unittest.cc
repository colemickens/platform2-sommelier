// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "leaderd/group.h"
#include "leaderd/manager.h"

using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::ObjectPath;
using chromeos::dbus_utils::ExportedObjectManager;
using testing::AnyNumber;
using testing::ReturnRef;
using testing::_;

namespace leaderd {

namespace {
const char kObjectManagerPath[] = "/objman";
const char kGroupPath[] = "/objman/group";
const char kGroupName[] = "ABC";
const std::string kMyUUID = "012";
const char kScore = 25;
const char kTestDBusSource[] = "TestDBusSource";

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
  MOCK_CONST_METHOD0(GetUUID, const std::string&());
  MOCK_METHOD1(RemoveGroup, void(const std::string&));
  MOCK_CONST_METHOD1(GetIPInfo,
                     std::vector<std::tuple<std::vector<uint8_t>, uint16_t>>(
                         const std::string&));
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

    EXPECT_CALL(group_delegate_, GetUUID())
        .WillRepeatedly(ReturnRef(self_uuid_));

    group_.reset(new Group{kGroupName,
                           bus_,
                           object_manager_.get(),
                           dbus::ObjectPath(kGroupPath),
                           kTestDBusSource,
                           {},
                           &group_delegate_});
  }

  // The delegate makes us return a reference, not a value.
  const std::string self_uuid_{"this-is-my-own-uuid"};

  base::MessageLoop message_loop_;
  scoped_refptr<EspeciallyMockedBus> bus_{
      new EspeciallyMockedBus{dbus::Bus::Options{}}};
  std::unique_ptr<ExportedObjectManager> object_manager_{
      new ExportedObjectManager(bus_,
                                dbus::ObjectPath(kObjectManagerPath))};
  MockGroupDelegate group_delegate_;
  std::unique_ptr<Group> group_;
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

}  // namespace leaderd
