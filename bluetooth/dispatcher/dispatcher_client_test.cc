// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher_client.h"

#include <memory>

#include <dbus/mock_bus.h>
#include <gtest/gtest.h>

#include "bluetooth/dispatcher/dbus_connection_factory.h"
#include "bluetooth/dispatcher/mock_dbus_connection_factory.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace bluetooth {

namespace {

constexpr char kTestClientAddress[] = ":1.2";

}  // namespace

class DispatcherClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    bus_ = new dbus::MockBus(dbus::Bus::Options());
    dbus_connection_factory_ = std::make_unique<MockDBusConnectionFactory>();
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
  }

  // Will be called when client becomes available. Keeps track the count this
  // happens so we can verify it's called.
  void OnClientUnavailable() { client_unavailable_callback_count_++; }

 protected:
  scoped_refptr<dbus::MockBus> bus_;
  std::unique_ptr<MockDBusConnectionFactory> dbus_connection_factory_;

  int client_unavailable_callback_count_ = 0;
};

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
