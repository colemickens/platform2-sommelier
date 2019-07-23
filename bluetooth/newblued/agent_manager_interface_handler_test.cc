// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/agent_manager_interface_handler.h"

#include <memory>
#include <string>
#include <utility>

#include <base/memory/ref_counted.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/dbus/exported_property_set.h>
#include <brillo/dbus/mock_exported_object_manager.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/exported_object.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;

namespace bluetooth {

namespace {

constexpr char kTestAgentPath[] = "/some/agent";
constexpr char kTestSender[] = ":1.1";
constexpr char kTestDeviceAddress[] = "11:22:33:44:55:66";
constexpr char kTestCapability[] = "some capability";

constexpr int kTestSerial = 123;
constexpr int kTestPasskey = 123456;

void SaveResponse(std::unique_ptr<dbus::Response>* saved_response,
                  std::unique_ptr<dbus::Response> response) {
  *saved_response = std::move(response);
}

// Matcher to compare method call by its interface and member name, ignoring
// the message payload.
MATCHER_P(MethodCallEq, expected_method_call, "") {
  return (arg->GetInterface() == (expected_method_call)->GetInterface() &&
          arg->GetMember() == (expected_method_call)->GetMember());
}

}  // namespace

class AgentManagerInterfaceHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    auto exported_object_manager =
        std::make_unique<brillo::dbus_utils::MockExportedObjectManager>(
            bus_, dbus::ObjectPath("/"));
    exported_object_manager_wrapper_ =
        std::make_unique<ExportedObjectManagerWrapper>(
            bus_, std::move(exported_object_manager));
    agent_manager_interface_handler_ =
        std::make_unique<AgentManagerInterfaceHandler>(
            bus_, exported_object_manager_wrapper_.get());
    exported_agent_manager_object_ = SetupExportedAgentManagerObject();
  }

 protected:
  void ExpectAgentManagerMethodsExported(
      dbus::ExportedObject::MethodCallCallback* register_agent_method_handler,
      dbus::ExportedObject::MethodCallCallback* unregister_agent_method_handler,
      dbus::ExportedObject::MethodCallCallback*
          request_default_agent_method_handler) {
    // Catch the standard dbus property handler
    EXPECT_CALL(*exported_agent_manager_object_,
                ExportMethodAndBlock(dbus::kDBusPropertiesInterface, _, _))
        .WillRepeatedly(DoAll(SaveArg<2>(request_default_agent_method_handler),
                              Return(true)));

    EXPECT_CALL(*exported_agent_manager_object_,
                ExportMethodAndBlock(
                    bluetooth_agent_manager::kBluetoothAgentManagerInterface,
                    bluetooth_agent_manager::kRegisterAgent, _))
        .WillOnce(
            DoAll(SaveArg<2>(register_agent_method_handler), Return(true)));
    EXPECT_CALL(*exported_agent_manager_object_,
                ExportMethodAndBlock(
                    bluetooth_agent_manager::kBluetoothAgentManagerInterface,
                    bluetooth_agent_manager::kUnregisterAgent, _))
        .WillOnce(
            DoAll(SaveArg<2>(unregister_agent_method_handler), Return(true)));
    EXPECT_CALL(*exported_agent_manager_object_,
                ExportMethodAndBlock(
                    bluetooth_agent_manager::kBluetoothAgentManagerInterface,
                    bluetooth_agent_manager::kRequestDefaultAgent, _))
        .WillOnce(DoAll(SaveArg<2>(request_default_agent_method_handler),
                        Return(true)));
  }

  scoped_refptr<dbus::MockExportedObject> SetupExportedAgentManagerObject() {
    dbus::ObjectPath agent_manager_path(
        bluetooth_agent_manager::kBluetoothAgentManagerServicePath);
    scoped_refptr<dbus::MockExportedObject> exported_agent_manager_object =
        new dbus::MockExportedObject(bus_.get(), agent_manager_path);
    EXPECT_CALL(*bus_, GetExportedObject(agent_manager_path))
        .WillRepeatedly(Return(exported_agent_manager_object.get()));
    return exported_agent_manager_object;
  }

  scoped_refptr<dbus::MockBus> bus_;

  // A raw pointer of this is kept by exported_object_manager_wrapper_, so
  // declare this first to make sure it's destructed later.
  scoped_refptr<dbus::MockExportedObject> exported_agent_manager_object_;

  std::unique_ptr<ExportedObjectManagerWrapper>
      exported_object_manager_wrapper_;
  std::unique_ptr<AgentManagerInterfaceHandler>
      agent_manager_interface_handler_;
};

TEST_F(AgentManagerInterfaceHandlerTest, DisplayPasskey) {
  dbus::ExportedObject::MethodCallCallback register_agent_method_handler;
  dbus::ExportedObject::MethodCallCallback unregister_agent_method_handler;
  dbus::ExportedObject::MethodCallCallback request_default_agent_method_handler;
  ExpectAgentManagerMethodsExported(&register_agent_method_handler,
                                    &unregister_agent_method_handler,
                                    &request_default_agent_method_handler);
  agent_manager_interface_handler_->Init();

  scoped_refptr<dbus::MockObjectProxy> agent_object_proxy =
      new dbus::MockObjectProxy(
          bus_.get(), bluetooth_agent_manager::kBluetoothAgentManagerInterface,
          dbus::ObjectPath(kTestAgentPath));

  // Before any client registers as an agent, DisplayPasskey won't call any
  // agent.
  EXPECT_CALL(*agent_object_proxy, CallMethod(_, _, _)).Times(0);
  agent_manager_interface_handler_->DisplayPasskey(kTestDeviceAddress,
                                                   kTestPasskey);

  // Test org.bluez.AgentManager1.RegisterAgent.
  dbus::MethodCall register_agent_method_call(
      bluetooth_agent_manager::kBluetoothAgentManagerInterface,
      bluetooth_agent_manager::kRegisterAgent);
  register_agent_method_call.SetPath(dbus::ObjectPath(
      bluetooth_agent_manager::kBluetoothAgentManagerServicePath));
  register_agent_method_call.SetSender(kTestSender);
  register_agent_method_call.SetSerial(kTestSerial);
  dbus::MessageWriter register_agent_writer(&register_agent_method_call);
  register_agent_writer.AppendObjectPath(dbus::ObjectPath(kTestAgentPath));
  register_agent_writer.AppendString(kTestCapability);
  std::unique_ptr<dbus::Response> register_agent_response;
  register_agent_method_handler.Run(
      &register_agent_method_call,
      base::Bind(&SaveResponse, &register_agent_response));
  EXPECT_EQ("", register_agent_response->GetErrorName());

  // A client has registered as an agent but has not requested to become the
  // default agent. So DisplayPasskey should still not call any agent.
  EXPECT_CALL(*agent_object_proxy, CallMethod(_, _, _)).Times(0);
  agent_manager_interface_handler_->DisplayPasskey(kTestDeviceAddress,
                                                   kTestPasskey);

  // Test org.bluez.AgentManager1.RequestDefaultAgent.
  dbus::MethodCall request_default_agent_method_call(
      bluetooth_agent_manager::kBluetoothAgentManagerInterface,
      bluetooth_agent_manager::kRequestDefaultAgent);
  request_default_agent_method_call.SetPath(dbus::ObjectPath(
      bluetooth_agent_manager::kBluetoothAgentManagerServicePath));
  request_default_agent_method_call.SetSender(kTestSender);
  request_default_agent_method_call.SetSerial(kTestSerial);
  dbus::MessageWriter request_default_agent_writer(
      &request_default_agent_method_call);
  request_default_agent_writer.AppendObjectPath(
      dbus::ObjectPath(kTestAgentPath));
  std::unique_ptr<dbus::Response> request_default_agent_response;
  request_default_agent_method_handler.Run(
      &request_default_agent_method_call,
      base::Bind(&SaveResponse, &request_default_agent_response));
  EXPECT_EQ("", request_default_agent_response->GetErrorName());

  // Now that a client has requested to become the default agent, check that
  // DisplayPasskey correctly handles this based on the current default agent.
  EXPECT_CALL(*bus_,
              GetObjectProxy(kTestSender, dbus::ObjectPath(kTestAgentPath)))
      .WillOnce(Return(agent_object_proxy.get()));
  dbus::MethodCall expected_method_call(
      bluetooth_agent::kBluetoothAgentInterface,
      bluetooth_agent::kDisplayPasskey);
  EXPECT_CALL(*agent_object_proxy,
              CallMethod(MethodCallEq(&expected_method_call), _, _))
      .Times(1);
  agent_manager_interface_handler_->DisplayPasskey(kTestDeviceAddress,
                                                   kTestPasskey);

  // Test org.bluez.AgentManager1.UnregisterAgent.
  dbus::MethodCall unregister_agent_method_call(
      bluetooth_agent_manager::kBluetoothAgentManagerInterface,
      bluetooth_agent_manager::kUnregisterAgent);
  unregister_agent_method_call.SetPath(dbus::ObjectPath(
      bluetooth_agent_manager::kBluetoothAgentManagerServicePath));
  unregister_agent_method_call.SetSender(kTestSender);
  unregister_agent_method_call.SetSerial(kTestSerial);
  dbus::MessageWriter unregister_agent_writer(&unregister_agent_method_call);
  unregister_agent_writer.AppendObjectPath(dbus::ObjectPath(kTestAgentPath));
  std::unique_ptr<dbus::Response> unregister_agent_response;
  register_agent_method_handler.Run(
      &unregister_agent_method_call,
      base::Bind(&SaveResponse, &unregister_agent_response));
  EXPECT_EQ("", unregister_agent_response->GetErrorName());

  // After the client unregisters from an agent, DisplayPasskey won't call any
  // agent.
  EXPECT_CALL(*agent_object_proxy, CallMethod(_, _, _)).Times(0);
  agent_manager_interface_handler_->DisplayPasskey(kTestDeviceAddress,
                                                   kTestPasskey);
}

}  // namespace bluetooth
