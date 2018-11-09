// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher_client.h"

#include <memory>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/bind_lambda.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <gtest/gtest.h>

#include "bluetooth/dispatcher/dbus_connection_factory.h"
#include "bluetooth/dispatcher/mock_dbus_connection_factory.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SaveArg;

namespace bluetooth {

namespace {

constexpr char kTestClientAddress[] = ":1.2";
constexpr char kTestInterfaceName[] = "org.example.Interface";
constexpr char kTestMethodName[] = "SomeMethod";

constexpr char kDBusSystemObjectPath[] = "/org/freedesktop/DBus";
constexpr char kDBusSystemObjectInterface[] = "org.freedesktop.DBus";
constexpr char kDBusSystemObjectAddress[] = "org.freedesktop.DBus";
constexpr char kNameOwnerChangedMember[] = "NameOwnerChanged";

}  // namespace

class DispatcherClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    EXPECT_CALL(*bus_, GetOriginTaskRunner())
        .WillRepeatedly(Return(message_loop_.task_runner().get()));
    dbus_connection_factory_ = std::make_unique<MockDBusConnectionFactory>();
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
  }

  // Will be called when client becomes available. Keeps track the count this
  // happens so we can verify it's called.
  void OnClientUnavailable() { client_unavailable_callback_count_++; }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> bus_;
  std::unique_ptr<MockDBusConnectionFactory> dbus_connection_factory_;

  int client_unavailable_callback_count_ = 0;
};

TEST_F(DispatcherClientTest, WatchClientUnavailable) {
  auto client = std::make_unique<DispatcherClient>(
      bus_, kTestClientAddress, dbus_connection_factory_.get());

  DBusHandleMessageFunction handle_message;

  EXPECT_CALL(*bus_, AddFilterFunction(_, client.get()))
      .WillOnce(SaveArg<0>(&handle_message));
  std::string match_rule = base::StringPrintf(
      "type='signal',interface='%s',member='%s',path='%s',sender='%s',"
      "arg0='%s'",
      kDBusSystemObjectInterface, kNameOwnerChangedMember,
      kDBusSystemObjectPath, kDBusSystemObjectAddress, kTestClientAddress);
  EXPECT_CALL(*bus_, AddMatch(match_rule, _)).Times(1);
  client->WatchClientUnavailable(base::Bind(
      &DispatcherClientTest::OnClientUnavailable, base::Unretained(this)));

  // Other signals shouldn't trigger client unavailable callback.
  dbus::Signal signal_other(kTestInterfaceName, kTestMethodName);
  handle_message(nullptr, signal_other.raw_message(), client.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, client_unavailable_callback_count_);

  // NameOwnerChanged signal with the right parameters should trigger client
  // unavailable callback.
  dbus::Signal signal_name_owner_changed(kDBusSystemObjectInterface,
                                         kNameOwnerChangedMember);
  signal_name_owner_changed.SetPath(dbus::ObjectPath(kDBusSystemObjectPath));
  signal_name_owner_changed.SetSender(kDBusSystemObjectAddress);
  dbus::MessageWriter writer(&signal_name_owner_changed);
  writer.AppendString(kTestClientAddress);  // 1st arg = address
  writer.AppendString(kTestClientAddress);  // 2nd arg = old owner
  writer.AppendString("");                  // 3rd arg = new owner
  handle_message(nullptr, signal_name_owner_changed.raw_message(),
                 client.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, client_unavailable_callback_count_);

  EXPECT_CALL(*bus_, RemoveMatch(match_rule, _)).Times(1);
  EXPECT_CALL(*bus_, RemoveFilterFunction(_, client.get()))
      .WillOnce(SaveArg<0>(&handle_message));
  client.reset();
}

TEST_F(DispatcherClientTest, GetClientBus) {
  scoped_refptr<dbus::MockBus> client_bus =
      new dbus::MockBus(dbus::Bus::Options());

  EXPECT_CALL(*dbus_connection_factory_, GetNewBus())
      .WillOnce(Return(client_bus));
  auto client = std::make_unique<DispatcherClient>(
      bus_, kTestClientAddress, dbus_connection_factory_.get());

  EXPECT_CALL(*client_bus, Connect()).WillOnce(Return(true));
  EXPECT_EQ(client_bus.get(), client->GetClientBus().get());

  // The connection should be shutdown when the the DispatcherClient object is
  // destructed.
  EXPECT_CALL(*client_bus, ShutdownAndBlock()).Times(1);
  client.reset();
}

}  // namespace bluetooth
