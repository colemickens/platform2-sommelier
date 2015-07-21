// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/dbus_command_proxy.h"

#include <functional>
#include <memory>

#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/property.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/dbus_object_test_helpers.h>
#include <gtest/gtest.h>

#include "buffet/dbus_constants.h"
#include "libweave/src/commands/command_dictionary.h"
#include "libweave/src/commands/command_instance.h"
#include "libweave/src/commands/unittest_utils.h"

namespace weave {

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

using chromeos::VariantDictionary;
using chromeos::dbus_utils::AsyncEventSequencer;
using unittests::CreateDictionaryValue;

namespace {

const char kTestCommandCategoty[] = "test_command_category";
const char kTestCommandId[] = "cmd_1";

}  // namespace

class DBusCommandProxyTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Set up a mock DBus bus object.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());

    // Command instance.
    // TODO(antonm): Test results.
    auto json = CreateDictionaryValue(R"({
      'robot': {
        'jump': {
          'parameters': {
            'height': {
              'type': 'integer',
              'minimum': 0,
              'maximum': 100
            },
            '_jumpType': {
              'type': 'string',
              'enum': ['_withAirFlip', '_withSpin', '_withKick']
            }
          },
          'results': {
            'foo': {
              'type': 'integer'
            },
            'bar': {
              'type': 'string'
            }
          },
          'progress': {
            'progress': {
              'type': 'integer',
              'minimum': 0,
              'maximum': 100
            }
          }
        }
      }
    })");
    CHECK(dict_.LoadCommands(*json, kTestCommandCategoty, nullptr, nullptr))
        << "Failed to parse test command dictionary";

    json = CreateDictionaryValue(R"({
      'name': 'robot.jump',
      'parameters': {
        'height': 53,
        '_jumpType': '_withKick'
      }
    })");
    command_instance_ =
        CommandInstance::FromJson(json.get(), "local", dict_, nullptr, nullptr);
    command_instance_->SetID(kTestCommandId);

    // Set up a mock ExportedObject to be used with the DBus command proxy.
    std::string cmd_path = buffet::kCommandServicePathPrefix;
    cmd_path += kTestCommandId;
    const dbus::ObjectPath kCmdObjPath(cmd_path);
    // Use a mock exported object for the exported object manager.
    mock_exported_object_command_ =
        new dbus::MockExportedObject(bus_.get(), kCmdObjPath);
    EXPECT_CALL(*bus_, GetExportedObject(kCmdObjPath))
        .Times(AnyNumber())
        .WillRepeatedly(Return(mock_exported_object_command_.get()));
    EXPECT_CALL(*mock_exported_object_command_, ExportMethod(_, _, _, _))
        .Times(AnyNumber());

    std::unique_ptr<CommandProxyInterface> command_proxy(
        new DBusCommandProxy(nullptr, bus_, command_instance_.get(), cmd_path));
    command_instance_->AddProxy(command_proxy.release());
    GetCommandProxy()->RegisterAsync(
        AsyncEventSequencer::GetDefaultCompletionAction());
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_command_, Unregister()).Times(1);
    command_instance_.reset();
    dict_.Clear();
    bus_ = nullptr;
  }

  DBusCommandProxy* GetCommandProxy() const {
    CHECK_EQ(command_instance_->proxies_.size(), 1U);
    return static_cast<DBusCommandProxy*>(command_instance_->proxies_[0]);
  }

  org::chromium::Buffet::CommandAdaptor* GetCommandAdaptor() const {
    return &GetCommandProxy()->dbus_adaptor_;
  }

  org::chromium::Buffet::CommandInterface* GetCommandInterface() const {
    // DBusCommandProxy also implements CommandInterface.
    return GetCommandProxy();
  }

  std::unique_ptr<CommandInstance> command_instance_;
  CommandDictionary dict_;

  scoped_refptr<dbus::MockExportedObject> mock_exported_object_command_;
  scoped_refptr<dbus::MockBus> bus_;
};

TEST_F(DBusCommandProxyTest, Init) {
  VariantDictionary params = {
      {"height", int32_t{53}}, {"_jumpType", std::string{"_withKick"}},
  };
  EXPECT_EQ(CommandInstance::kStatusQueued, GetCommandAdaptor()->GetStatus());
  EXPECT_EQ(params, GetCommandAdaptor()->GetParameters());
  EXPECT_EQ(VariantDictionary{}, GetCommandAdaptor()->GetProgress());
  EXPECT_EQ(VariantDictionary{}, GetCommandAdaptor()->GetResults());
  EXPECT_EQ("robot.jump", GetCommandAdaptor()->GetName());
  EXPECT_EQ(kTestCommandCategoty, GetCommandAdaptor()->GetCategory());
  EXPECT_EQ(kTestCommandId, GetCommandAdaptor()->GetId());
}

TEST_F(DBusCommandProxyTest, SetProgress) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(2);
  EXPECT_TRUE(
      GetCommandInterface()->SetProgress(nullptr, {{"progress", int32_t{10}}}));
  EXPECT_EQ(CommandInstance::kStatusInProgress,
            GetCommandAdaptor()->GetStatus());

  VariantDictionary progress{{"progress", int32_t{10}}};
  EXPECT_EQ(progress, GetCommandAdaptor()->GetProgress());
}

TEST_F(DBusCommandProxyTest, SetProgress_OutOfRange) {
  EXPECT_FALSE(GetCommandInterface()->SetProgress(
      nullptr, {{"progress", int32_t{110}}}));
  EXPECT_EQ(CommandInstance::kStatusQueued, GetCommandAdaptor()->GetStatus());
  EXPECT_EQ(VariantDictionary{}, GetCommandAdaptor()->GetProgress());
}

TEST_F(DBusCommandProxyTest, SetResults) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(1);
  const VariantDictionary results = {
      {"foo", int32_t{42}}, {"bar", std::string{"foobar"}},
  };
  EXPECT_TRUE(GetCommandInterface()->SetResults(nullptr, results));
  EXPECT_EQ(results, GetCommandAdaptor()->GetResults());
}

TEST_F(DBusCommandProxyTest, SetResults_UnknownProperty) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(0);
  const VariantDictionary results = {
      {"quux", int32_t{42}},
  };
  EXPECT_FALSE(GetCommandInterface()->SetResults(nullptr, results));
}

TEST_F(DBusCommandProxyTest, Abort) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(1);
  GetCommandInterface()->Abort();
  EXPECT_EQ(CommandInstance::kStatusAborted, GetCommandAdaptor()->GetStatus());
}

TEST_F(DBusCommandProxyTest, Cancel) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(1);
  GetCommandInterface()->Cancel();
  EXPECT_EQ(CommandInstance::kStatusCancelled,
            GetCommandAdaptor()->GetStatus());
}

TEST_F(DBusCommandProxyTest, Done) {
  // 1 property update:
  // status: queued -> done
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(1);
  GetCommandInterface()->Done();
  EXPECT_EQ(CommandInstance::kStatusDone, GetCommandAdaptor()->GetStatus());
}

}  // namespace weave
