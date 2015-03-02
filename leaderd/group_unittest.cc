// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/group.h"

#include <string>

#include <base/run_loop.h>
#include <dbus/bus.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

using dbus::MockBus;
using dbus::MockExportedObject;
using dbus::ObjectPath;
using chromeos::dbus_utils::ExportedObjectManager;
using testing::AnyNumber;
using testing::ReturnRef;
using testing::_;

namespace leaderd {

namespace {
const char kGroupName[] = "ABC";
const std::string kMyUUID = "012";
const char kScore = 25;

}  // namespace

class GroupTest : public testing::Test {
 public:
  void SetUp() override {
    // Ignore threading concerns.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    group_.reset(new Group{kGroupName, bus_, nullptr,
                           dbus::ObjectPath("/manager/object/path"), nullptr});
  }
  scoped_refptr<MockBus> bus_{new MockBus{dbus::Bus::Options{}}};
  std::unique_ptr<Group> group_;
};

TEST_F(GroupTest, LeaveGroup) {
  chromeos::ErrorPtr error;
  EXPECT_TRUE(group_->LeaveGroup(&error));
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(GroupTest, SetScore) {
  chromeos::ErrorPtr error;
  EXPECT_TRUE(group_->SetScore(&error, kScore));
  EXPECT_EQ(nullptr, error.get());
}

}  // namespace leaderd
