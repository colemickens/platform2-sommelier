// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>

#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/property.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/dbus_object_test_helpers.h>
#include <gtest/gtest.h>

#include "buffet/commands/command_dictionary.h"
#include "buffet/commands/command_instance.h"
#include "buffet/commands/dbus_command_proxy.h"
#include "buffet/commands/unittest_utils.h"
#include "buffet/libbuffet/dbus_constants.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;

using buffet::unittests::CreateDictionaryValue;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using chromeos::VariantDictionary;

namespace buffet {

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
    command_instance_ = CommandInstance::FromJson(json.get(), dict_, nullptr);
    command_instance_->SetID(kTestCommandId);

    // Set up a mock ExportedObject to be used with the DBus command proxy.
    std::string cmd_path = dbus_constants::kCommandServicePathPrefix;
    cmd_path += kTestCommandId;
    const dbus::ObjectPath kCmdObjPath(cmd_path);
    // Use a mock exported object for the exported object manager.
    mock_exported_object_command_ =
        new dbus::MockExportedObject(bus_.get(), kCmdObjPath);
    EXPECT_CALL(*bus_, GetExportedObject(kCmdObjPath)).Times(AnyNumber())
        .WillRepeatedly(Return(mock_exported_object_command_.get()));
    EXPECT_CALL(*mock_exported_object_command_,
                ExportMethod(_, _, _, _)).Times(AnyNumber());

    command_proxy_.reset(new DBusCommandProxy(nullptr, bus_,
                                              command_instance_.get(),
                                              cmd_path));
    command_instance_->AddProxy(command_proxy_.get());
    command_proxy_->RegisterAsync(
        AsyncEventSequencer::GetDefaultCompletionAction());
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_command_, Unregister()).Times(1);
    command_instance_->ClearProxies();
    command_proxy_.reset();
    command_instance_.reset();
    dict_.Clear();
    bus_ = nullptr;
  }

  chromeos::dbus_utils::DBusObject* GetProxyDBusObject() {
    return &command_proxy_->dbus_object_;
  }

  std::string GetStatus() const {
    return command_proxy_->status_.value();
  }

  int32_t GetProgress() const {
    return command_proxy_->progress_.value();
  }

  VariantDictionary GetParameters() const {
    return command_proxy_->parameters_.value();
  }

  std::unique_ptr<dbus::Response> CallMethod(
      const std::string& method_name,
      const std::function<void(dbus::MessageWriter*)>& param_callback) {
    dbus::MethodCall method_call(dbus_constants::kCommandInterface,
                                 method_name);
    method_call.SetSerial(1234);
    dbus::MessageWriter writer(&method_call);
    if (param_callback)
      param_callback(&writer);
    return chromeos::dbus_utils::testing::CallMethod(*GetProxyDBusObject(),
                                                     &method_call);
  }

  static bool IsResponseError(const std::unique_ptr<dbus::Response>& response) {
    return (response->GetMessageType() == dbus::Message::MESSAGE_ERROR);
  }

  static void VerifyResponse(
      const std::unique_ptr<dbus::Response>& response,
      const std::function<void(dbus::MessageReader*)>& result_callback) {
    EXPECT_FALSE(IsResponseError(response));
    dbus::MessageReader reader(response.get());
    if (result_callback)
      result_callback(&reader);
    EXPECT_FALSE(reader.HasMoreData());
  }

  template<typename T>
  T GetPropertyValue(const std::string& property_name) {
    dbus::MethodCall method_call(dbus::kPropertiesInterface,
                                 dbus::kPropertiesGet);
    method_call.SetSerial(1234);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(dbus_constants::kCommandInterface);
    writer.AppendString(property_name);
    auto response = chromeos::dbus_utils::testing::CallMethod(
        *GetProxyDBusObject(), &method_call);
    T value{};
    VerifyResponse(response, [&value](dbus::MessageReader* reader) {
      EXPECT_TRUE(chromeos::dbus_utils::PopValueFromReader(reader, &value));
    });
    return value;
  }

  std::unique_ptr<DBusCommandProxy> command_proxy_;
  std::unique_ptr<CommandInstance> command_instance_;
  CommandDictionary dict_;

  scoped_refptr<dbus::MockExportedObject> mock_exported_object_command_;
  scoped_refptr<dbus::MockBus> bus_;
};

TEST_F(DBusCommandProxyTest, Init) {
  VariantDictionary params = {
    {"height", int32_t{53}},
    {"_jumpType", std::string{"_withKick"}},
  };
  EXPECT_EQ(CommandInstance::kStatusQueued, GetStatus());
  EXPECT_EQ(0, GetProgress());
  EXPECT_EQ(params, GetParameters());
  EXPECT_EQ("robot.jump",
            GetPropertyValue<std::string>(dbus_constants::kCommandName));
  EXPECT_EQ(kTestCommandCategoty,
            GetPropertyValue<std::string>(dbus_constants::kCommandCategory));
  EXPECT_EQ(kTestCommandId,
            GetPropertyValue<std::string>(dbus_constants::kCommandId));
  EXPECT_EQ(CommandInstance::kStatusQueued,
            GetPropertyValue<std::string>(dbus_constants::kCommandStatus));
  EXPECT_EQ(0, GetPropertyValue<int32_t>(dbus_constants::kCommandProgress));
  EXPECT_EQ(params,
            GetPropertyValue<VariantDictionary>(
                dbus_constants::kCommandParameters));
}

TEST_F(DBusCommandProxyTest, SetProgress) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(2);
  auto response = CallMethod(dbus_constants::kCommandSetProgress,
                             [](dbus::MessageWriter* writer) {
    writer->AppendInt32(10);
  });
  VerifyResponse(response, {});
  EXPECT_EQ(CommandInstance::kStatusInProgress, GetStatus());
  EXPECT_EQ(10, GetProgress());
  EXPECT_EQ(CommandInstance::kStatusInProgress,
            GetPropertyValue<std::string>(dbus_constants::kCommandStatus));
  EXPECT_EQ(10, GetPropertyValue<int32_t>(dbus_constants::kCommandProgress));
}

TEST_F(DBusCommandProxyTest, SetProgress_OutOfRange) {
  auto response = CallMethod(dbus_constants::kCommandSetProgress,
                             [](dbus::MessageWriter* writer) {
    writer->AppendInt32(110);
  });
  EXPECT_TRUE(IsResponseError(response));
  EXPECT_EQ(CommandInstance::kStatusQueued, GetStatus());
  EXPECT_EQ(0, GetProgress());
}

TEST_F(DBusCommandProxyTest, Abort) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(1);
  auto response = CallMethod(dbus_constants::kCommandAbort, {});
  VerifyResponse(response, {});
  EXPECT_EQ(CommandInstance::kStatusAborted, GetStatus());
}

TEST_F(DBusCommandProxyTest, Cancel) {
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(1);
  auto response = CallMethod(dbus_constants::kCommandCancel, {});
  VerifyResponse(response, {});
  EXPECT_EQ(CommandInstance::kStatusCanceled, GetStatus());
}

TEST_F(DBusCommandProxyTest, Done) {
  // 3 property updates:
  // status: queued -> inProgress
  // progress: 0 -> 100
  // status: inProgress -> done
  EXPECT_CALL(*mock_exported_object_command_, SendSignal(_)).Times(3);
  auto response = CallMethod(dbus_constants::kCommandDone, {});
  VerifyResponse(response, {});
  EXPECT_EQ(CommandInstance::kStatusDone, GetStatus());
  EXPECT_EQ(100, GetProgress());
}

}  // namespace buffet
