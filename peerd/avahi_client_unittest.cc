// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/avahi_client.h"

#include <memory>

#include <avahi-common/defs.h>
#include <base/memory/ref_counted.h>
#include <brillo/dbus/dbus_param_writer.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/complete_mock_object_proxy.h"
#include "peerd/dbus_constants.h"
#include "peerd/mock_peer_manager.h"
#include "peerd/test_util.h"

using brillo::dbus_utils::AsyncEventSequencer;
using dbus::MockBus;
using dbus::ObjectPath;
using dbus::Response;
using peerd::dbus_constants::avahi::kServerInterface;
using peerd::dbus_constants::avahi::kServerMethodGetHostName;
using peerd::dbus_constants::avahi::kServerMethodGetState;
using peerd::dbus_constants::avahi::kServerMethodServiceBrowserNew;
using peerd::dbus_constants::avahi::kServerPath;
using peerd::dbus_constants::avahi::kServerSignalStateChanged;
using peerd::dbus_constants::avahi::kServiceName;
using peerd::dbus_constants::avahi::kServiceBrowserInterface;
using peerd::dbus_constants::avahi::kServiceBrowserMethodFree;
using peerd::dbus_constants::avahi::kServiceBrowserSignalItemNew;
using peerd::dbus_constants::avahi::kServiceBrowserSignalItemRemove;
using peerd::dbus_constants::avahi::kServiceBrowserSignalFailure;
using peerd::test_util::IsDBusMethodCallTo;
using std::string;
using testing::AnyNumber;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::Unused;
using testing::_;

namespace {

const char kDiscovererPath[] = "/path/to/avahi/discoverer";

Response* ReturnsDiscovererPath(dbus::MethodCall* method_call, Unused, Unused) {
  method_call->SetSerial(87);
  std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  brillo::dbus_utils::AppendValueToWriter(
      &writer, ObjectPath{string{kDiscovererPath}});
  return response.release();
}

}  // namespace


namespace peerd {

class AvahiClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    avahi_proxy_ = new peerd::CompleteMockObjectProxy(
        mock_bus_.get(), kServiceName, ObjectPath(kServerPath));
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Return the ObjectProxy for Avahi when asked.
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(kServiceName,
                               Property(&ObjectPath::value, kServerPath)))
        .WillRepeatedly(Return(avahi_proxy_.get()));
    client_.reset(new AvahiClient(mock_bus_, &peer_manager_));
  }

  void TearDown() override {
    EXPECT_CALL(*avahi_proxy_, Detach());
    if (client_->discoverer_) {
      EXPECT_CALL(*service_browser_proxy_, Detach());
    }
  }

  void RegisterAsyncWhenAvahiIs(bool is_running, AvahiServerState state) {
    // Immediately signal that signal connection has succeeded.
    EXPECT_CALL(*avahi_proxy_,
                ConnectToSignal(kServerInterface,
                                kServerSignalStateChanged,
                                _, _))
        .WillOnce(DoAll(Invoke(&test_util::HandleConnectToSignal),
                        SaveArg<2>(&state_changed_signal_callback_)));

    // After we connect the signal, we'll be asked to handle a callback
    // for the initial state of Avahi.
    EXPECT_CALL(*avahi_proxy_, WaitForServiceToBeAvailable(_));
    if (is_running) {
      EXPECT_CALL(*avahi_proxy_,
                  MockCallMethodAndBlockWithErrorDetails(IsDBusMethodCallTo(
                      kServerInterface, kServerMethodGetState), _, _))
          .WillOnce(Invoke(this, &AvahiClientTest::ReturnsCurrentState));
    } else {
      EXPECT_CALL(*avahi_proxy_,
                  MockCallMethodAndBlockWithErrorDetails(IsDBusMethodCallTo(
                      kServerInterface, kServerMethodGetState), _, _))
          .Times(0);
    }
    current_avahi_state_ = state;
    client_->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
    // This is the callback that would normally require a task runner.
    client_->OnServiceAvailable(is_running);
  }

  Response *ReturnsCurrentState(dbus::MethodCall* method_call, Unused, Unused) {
    method_call->SetSerial(87);
    std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    brillo::dbus_utils::AppendValueToWriter(&writer,
                                            int32_t{current_avahi_state_});
    // The mock wraps this back in a std::unique_ptr in the function calling us.
    return response.release();
  }

  void SendOnStateChanged(AvahiServerState state) {
    dbus::Signal signal(kServerInterface, kServerSignalStateChanged);
    dbus::MessageWriter writer(&signal);
    // No error message to report cap'n.
    brillo::dbus_utils::DBusParamWriter::Append(&writer,
                                                int32_t{state},
                                                string{});
    ASSERT_FALSE(state_changed_signal_callback_.is_null());
    state_changed_signal_callback_.Run(&signal);
  }

  void ExpectServiceDiscovererCreated(bool will_be_created) {
    Mock::VerifyAndClearExpectations(avahi_proxy_.get());
    Mock::VerifyAndClearExpectations(service_browser_proxy_.get());
    if (!will_be_created) {
      EXPECT_CALL(*avahi_proxy_,
                  MockCallMethodAndBlockWithErrorDetails(IsDBusMethodCallTo(
                      kServerInterface, kServerMethodServiceBrowserNew), _, _))
          .Times(0);
      return;
    }
    EXPECT_CALL(*avahi_proxy_,
                MockCallMethodAndBlockWithErrorDetails(IsDBusMethodCallTo(
                    kServerInterface, kServerMethodServiceBrowserNew), _, _))
        .WillOnce(Invoke(&ReturnsDiscovererPath));
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(kServiceName,
                               Property(&ObjectPath::value, kDiscovererPath)))
        .WillRepeatedly(Return(service_browser_proxy_.get()));
    EXPECT_CALL(*service_browser_proxy_,
                ConnectToSignal(kServiceBrowserInterface,
                                kServiceBrowserSignalItemNew,
                                _, _)).Times(1);
    EXPECT_CALL(*service_browser_proxy_,
                ConnectToSignal(kServiceBrowserInterface,
                                kServiceBrowserSignalItemRemove,
                                _, _)).Times(1);
    EXPECT_CALL(*service_browser_proxy_,
                ConnectToSignal(kServiceBrowserInterface,
                                kServiceBrowserSignalFailure,
                                _, _)).Times(1);
    EXPECT_CALL(*service_browser_proxy_, Detach()).Times(0);
  }

  MOCK_METHOD0(AvahiReady, void());

  scoped_refptr<MockBus> mock_bus_{new MockBus{dbus::Bus::Options{}}};
  MockPeerManager peer_manager_;
  scoped_refptr<peerd::CompleteMockObjectProxy> avahi_proxy_;
  scoped_refptr<dbus::MockObjectProxy> service_browser_proxy_{
      new dbus::MockObjectProxy{mock_bus_.get(), kServiceName,
                                ObjectPath{kDiscovererPath}}};
  std::unique_ptr<AvahiClient> client_;
  AvahiServerState current_avahi_state_;
  dbus::ObjectProxy::SignalCallback state_changed_signal_callback_;
};  // class AvahiClientTest


TEST_F(AvahiClientTest, ShouldNotifyRestart_WhenAvahiIsUp) {
  // We don't get called back until we finish RegisterAsync().
  EXPECT_CALL(*this, AvahiReady()).Times(0);
  client_->RegisterOnAvahiRestartCallback(
      base::Bind(&AvahiClientTest::AvahiReady, base::Unretained(this)));
  // However, on finishing RegisterAsync(), we should notice Avahi is up, and
  // signal this to interested parties.
  EXPECT_CALL(*this, AvahiReady()).Times(1);
  RegisterAsyncWhenAvahiIs(true, AVAHI_SERVER_RUNNING);
}

TEST_F(AvahiClientTest, ShouldNotifyRestart_WhenAvahiComesUpLater) {
  // Avahi should remain down until we say otherwise.
  EXPECT_CALL(*this, AvahiReady()).Times(0);
  client_->RegisterOnAvahiRestartCallback(
      base::Bind(&AvahiClientTest::AvahiReady, base::Unretained(this)));
  RegisterAsyncWhenAvahiIs(false, AVAHI_SERVER_INVALID);
  // The StateChanged signal should trigger our callback.
  EXPECT_CALL(*this, AvahiReady()).Times(1);
  SendOnStateChanged(AVAHI_SERVER_RUNNING);
}

TEST_F(AvahiClientTest, CanGetPublisherWhenAvahiIsUp) {
  RegisterAsyncWhenAvahiIs(true, AVAHI_SERVER_RUNNING);
  EXPECT_NE(nullptr, client_->GetPublisher("uuid"));
}

TEST_F(AvahiClientTest, ShouldStartMonitoringWhenUp) {
  RegisterAsyncWhenAvahiIs(false, AVAHI_SERVER_INVALID);
  ExpectServiceDiscovererCreated(false);
  client_->StartMonitoring();
  ExpectServiceDiscovererCreated(true);
  SendOnStateChanged(AVAHI_SERVER_RUNNING);
}

TEST_F(AvahiClientTest, ShouldStopMonitoringWhenDown) {
  RegisterAsyncWhenAvahiIs(true, AVAHI_SERVER_RUNNING);
  ExpectServiceDiscovererCreated(true);
  client_->StartMonitoring();
  EXPECT_CALL(*service_browser_proxy_, Detach());
  SendOnStateChanged(AVAHI_SERVER_INVALID);
}

TEST_F(AvahiClientTest, ShouldStopMonitoringWhenRequested) {
  RegisterAsyncWhenAvahiIs(true, AVAHI_SERVER_RUNNING);
  ExpectServiceDiscovererCreated(true);
  client_->StartMonitoring();
  EXPECT_CALL(*service_browser_proxy_, Detach());
  client_->StopMonitoring();
}

TEST_F(AvahiClientTest, ShouldResumeMonitoring) {
  RegisterAsyncWhenAvahiIs(true, AVAHI_SERVER_RUNNING);
  ExpectServiceDiscovererCreated(true);
  client_->StartMonitoring();
  EXPECT_CALL(*service_browser_proxy_, Detach());
  // We're not monitoring now, but if Avahi comes back, resume monitoring.
  SendOnStateChanged(AVAHI_SERVER_INVALID);
  ExpectServiceDiscovererCreated(true);
  SendOnStateChanged(AVAHI_SERVER_RUNNING);
}

TEST_F(AvahiClientTest, StopMonitoringIsPersistent) {
  RegisterAsyncWhenAvahiIs(true, AVAHI_SERVER_RUNNING);
  ExpectServiceDiscovererCreated(true);
  client_->StartMonitoring();
  EXPECT_CALL(*service_browser_proxy_, Detach());
  client_->StopMonitoring();
  ExpectServiceDiscovererCreated(false);
  // Changes in Avahi state going forward don't matter, no monitoring
  // should happen.
  SendOnStateChanged(AVAHI_SERVER_INVALID);
  SendOnStateChanged(AVAHI_SERVER_RUNNING);
}

}  // namespace peerd
