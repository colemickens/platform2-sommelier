// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <chromeos/dbus/exported_object_manager.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "buffet/commands/command_dictionary.h"
#include "buffet/commands/command_queue.h"
#include "buffet/commands/dbus_command_dispatcher.h"
#include "buffet/commands/unittest_utils.h"
#include "buffet/libbuffet/dbus_constants.h"

using buffet::unittests::CreateDictionaryValue;
using chromeos::dbus_utils::AsyncEventSequencer;
using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::_;

namespace buffet {

namespace {

const char kCommandCategory[] = "test_category";

}  // anonymous namespace

class DBusCommandDispacherTest : public testing::Test {
 public:
  void SetUp() override {
    const dbus::ObjectPath kExportedObjectManagerPath("/test/om_path");
    std::string cmd_path = dbus_constants::kCommandServicePathPrefix;
    cmd_path += "1";
    const dbus::ObjectPath kCmdObjPath(cmd_path);

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object manager.
    mock_exported_object_manager_ = new dbus::MockExportedObject(
        bus_.get(), kExportedObjectManagerPath);
    EXPECT_CALL(*bus_, GetExportedObject(kExportedObjectManagerPath))
        .WillRepeatedly(Return(mock_exported_object_manager_.get()));
    EXPECT_CALL(*mock_exported_object_manager_,
                ExportMethod(_, _, _, _)).Times(AnyNumber());
    om_.reset(new chromeos::dbus_utils::ExportedObjectManager(
        bus_.get(), kExportedObjectManagerPath));
    om_->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
    command_dispatcher_.reset(
        new DBusCommandDispacher(om_->GetBus(), om_.get()));
    command_queue_.SetCommandDispachInterface(command_dispatcher_.get());
    // Use a mock exported object for command proxy.
    mock_exported_command_proxy_ = new dbus::MockExportedObject(
        bus_.get(), kCmdObjPath);
    EXPECT_CALL(*bus_, GetExportedObject(kCmdObjPath))
        .WillRepeatedly(Return(mock_exported_command_proxy_.get()));
    EXPECT_CALL(*mock_exported_command_proxy_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(MockExportMethod));

    auto json = CreateDictionaryValue(R"({
      'base': {
        'reboot': {
          'parameters': {'delay': 'integer'}
        },
        'shutdown': {
          'parameters': {}
        }
      }
    })");
    CHECK(dictionary_.LoadCommands(*json, kCommandCategory, nullptr, nullptr))
        << "Failed to load command dictionary";
  }

  void TearDown() override {
    EXPECT_CALL(*mock_exported_object_manager_, Unregister()).Times(1);
    om_.reset();
    bus_ = nullptr;
  }

  static void MockExportMethod(
      const std::string& interface_name,
      const std::string& method_name,
      dbus::ExportedObject::MethodCallCallback method_call_callback,
      dbus::ExportedObject::OnExportedCallback on_exported_callback) {
    on_exported_callback.Run(interface_name, method_name, true);
  }


  std::string AddNewCommand(const std::string& json) {
    auto command_instance = CommandInstance::FromJson(
        CreateDictionaryValue(json.c_str()).get(), dictionary_, nullptr);
    // Two interfaces are added - Command and Properties.
    EXPECT_CALL(*mock_exported_object_manager_, SendSignal(_)).Times(2);
    return command_instance ?
        command_queue_.Add(std::move(command_instance)) : std::string();
  }

  void FinishCommand(DBusCommandProxy* proxy) {
    proxy->HandleDone(nullptr);
  }

  void SetProgress(DBusCommandProxy* proxy, int progress) {
    proxy->HandleSetProgress(nullptr, progress);
  }


  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_manager_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_command_proxy_;
  std::unique_ptr<chromeos::dbus_utils::ExportedObjectManager> om_;
  CommandDictionary dictionary_;
  CommandQueue command_queue_;
  std::unique_ptr<DBusCommandDispacher> command_dispatcher_;
};

TEST_F(DBusCommandDispacherTest, Test_Command_Base_Shutdown) {
  std::string id = AddNewCommand("{'name':'base.shutdown'}");
  EXPECT_EQ("1", id);
  CommandInstance* command_instance = command_queue_.Find(id);
  ASSERT_NE(nullptr, command_instance);
  DBusCommandProxy* command_proxy =
      command_dispatcher_->FindProxy(command_instance);
  ASSERT_NE(nullptr, command_proxy);
  EXPECT_EQ(CommandInstance::kStatusQueued, command_instance->GetStatus());

  // Two properties are set, Progress = 50%, Status = "inProgress"
  EXPECT_CALL(*mock_exported_command_proxy_, SendSignal(_)).Times(2);
  SetProgress(command_proxy, 50);
  EXPECT_EQ(CommandInstance::kStatusInProgress, command_instance->GetStatus());
  EXPECT_EQ(50, command_instance->GetProgress());

  // Command must be removed from the queue and proxy destroyed after calling
  // FinishCommand().
  // Two properties are set, Progress = 100%, Status = "done"
  EXPECT_CALL(*mock_exported_command_proxy_, SendSignal(_)).Times(2);
  // D-Bus command proxy is going away.
  EXPECT_CALL(*mock_exported_command_proxy_, Unregister()).Times(1);
  // Two interfaces are being removed on the D-Bus command object.
  EXPECT_CALL(*mock_exported_object_manager_, SendSignal(_)).Times(2);
  FinishCommand(command_proxy);

  EXPECT_EQ(nullptr,
            command_dispatcher_->FindProxy(command_instance));
  EXPECT_EQ(nullptr, command_queue_.Find(id));
}

TEST_F(DBusCommandDispacherTest, Test_Command_Base_Reboot) {
  std::string id = AddNewCommand(R"({
    'name': 'base.reboot',
    'parameters': {
      'delay': 20
    }
  })");
  EXPECT_EQ("1", id);
  CommandInstance* command_instance = command_queue_.Find(id);
  ASSERT_NE(nullptr, command_instance);
  DBusCommandProxy* command_proxy =
      command_dispatcher_->FindProxy(command_instance);
  ASSERT_NE(nullptr, command_proxy);
  EXPECT_EQ(CommandInstance::kStatusQueued, command_instance->GetStatus());

  // Two properties are set, Progress = 50%, Status = "inProgress"
  EXPECT_CALL(*mock_exported_command_proxy_, SendSignal(_)).Times(2);
  SetProgress(command_proxy, 50);
  EXPECT_EQ(CommandInstance::kStatusInProgress, command_instance->GetStatus());
  EXPECT_EQ(50, command_instance->GetProgress());

  // Command must be removed from the queue and proxy destroyed after calling
  // FinishCommand().
  // Two properties are set, Progress = 100%, Status = "done"
  EXPECT_CALL(*mock_exported_command_proxy_, SendSignal(_)).Times(2);
  // D-Bus command proxy is going away.
  EXPECT_CALL(*mock_exported_command_proxy_, Unregister()).Times(1);
  // Two interfaces are being removed on the D-Bus command object.
  EXPECT_CALL(*mock_exported_object_manager_, SendSignal(_)).Times(2);
  FinishCommand(command_proxy);

  EXPECT_EQ(nullptr,
            command_dispatcher_->FindProxy(command_instance));
  EXPECT_EQ(nullptr, command_queue_.Find(id));
}


}  // namespace buffet
