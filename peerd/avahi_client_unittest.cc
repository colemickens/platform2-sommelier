// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <avahi-common/defs.h>
#include <base/memory/ref_counted.h>
#include <chromeos/dbus/data_serialization.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/avahi_client.h"
#include "peerd/dbus_constants.h"
#include "peerd/test_util.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using dbus::ObjectPath;
using dbus::Response;
using peerd::dbus_constants::avahi::kServerInterface;
using peerd::dbus_constants::avahi::kServerMethodGetState;
using peerd::dbus_constants::avahi::kServerPath;
using peerd::dbus_constants::avahi::kServerSignalStateChanged;
using peerd::dbus_constants::avahi::kServiceName;
using peerd::test_util::IsDBusMethodCallTo;
using testing::AnyNumber;
using testing::Invoke;
using testing::Property;
using testing::Return;
using testing::Unused;
using testing::_;

namespace {

Response* GetStateDelegate(dbus::MethodCall* method_call, int32_t state) {
  method_call->SetSerial(87);
  scoped_ptr<Response> response = Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  chromeos::dbus_utils::AppendValueToWriter(&writer, state);
  // The mock wraps this back in a scoped_ptr in the function calling us.
  return response.release();
}

Response* ReturnsRunning(dbus::MethodCall* method_call, Unused, Unused) {
  return GetStateDelegate(method_call, AVAHI_SERVER_RUNNING);
}

Response* ReturnsInvalid(dbus::MethodCall* method_call, Unused, Unused) {
  return GetStateDelegate(method_call, AVAHI_SERVER_INVALID);
}

}  // namespace


namespace peerd {

class AvahiClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    dbus::Bus::Options options;
    mock_bus_ = new dbus::MockBus(options);
    avahi_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), kServiceName, ObjectPath(kServerPath));
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Return the ObjectProxy for Avahi when asked.
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(kServiceName,
                               Property(&ObjectPath::value, kServerPath)))
        .WillRepeatedly(Return(avahi_proxy_.get()));
    // Immediately signal that signal connection has succeeded.
    EXPECT_CALL(*avahi_proxy_,
                ConnectToSignal(kServerInterface,
                                kServerSignalStateChanged,
                                _, _))
        .WillOnce(Invoke(&test_util::HandleConnectToSignal));
    client_.reset(new AvahiClient(mock_bus_));
  }

  void TearDown() override {
    EXPECT_CALL(*avahi_proxy_, Detach()).Times(1);
  }

  void SetInitialAvahiState(bool is_running) {
    auto handler = &ReturnsInvalid;
    if (is_running) { handler = &ReturnsRunning; }
    EXPECT_CALL(
        *avahi_proxy_,
        MockCallMethodAndBlockWithErrorDetails(
            IsMethodCallTo(kServerInterface, kServerMethodGetState), _, _))
        .WillOnce(Invoke(handler));
  }

  void SendOnStateChanged(int32_t state) {
    dbus::Signal signal(kServerInterface, kServerSignalStateChanged);
    dbus::MessageWriter writer(&signal);
    chromeos::dbus_utils::AppendValueToWriter(&writer, state);
    // No error message to report cap'n.
    chromeos::dbus_utils::AppendValueToWriter(&writer, "");
    client_->OnServerStateChanged(&signal);
  }

  MOCK_METHOD0(AvahiReady, void());

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> avahi_proxy_;
  std::unique_ptr<AvahiClient> client_;
};  // class AvahiClientTest


TEST_F(AvahiClientTest, ShouldNotifyRestart_WhenAvahiIsUp) {
  SetInitialAvahiState(true);  // Pretend Avahi is up and running.
  // We don't get called back until we finish RegisterAsync().
  EXPECT_CALL(*this, AvahiReady()).Times(0);
  client_->RegisterOnAvahiRestartCallback(
      base::Bind(&AvahiClientTest::AvahiReady, base::Unretained(this)));
  // However, on finishing RegisterAsync(), we should notice Avahi is up, and
  // signal this to interested parties.
  EXPECT_CALL(*this, AvahiReady()).Times(1);
  client_->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
}

TEST_F(AvahiClientTest, ShouldNotifyRestart_WhenAvahiComesUpLater) {
  SetInitialAvahiState(false);  // Pretend Avahi isn't ready.
  // Avahi should remain down until we say otherwise.
  EXPECT_CALL(*this, AvahiReady()).Times(0);
  client_->RegisterOnAvahiRestartCallback(
      base::Bind(&AvahiClientTest::AvahiReady, base::Unretained(this)));
  client_->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
  // The StateChanged signal should trigger our callback.
  EXPECT_CALL(*this, AvahiReady()).Times(1);
  SendOnStateChanged(AVAHI_SERVER_RUNNING);
}

}  // namespace peerd
