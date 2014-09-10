// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/memory/ref_counted.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "peerd/avahi_service_publisher.h"
#include "peerd/dbus_constants.h"
#include "peerd/service.h"
#include "peerd/test_util.h"

using dbus::ObjectPath;
using dbus::Response;
using peerd::Service;
using peerd::dbus_constants::avahi::kGroupInterface;
using peerd::dbus_constants::avahi::kGroupMethodFree;
using peerd::dbus_constants::avahi::kServerInterface;
using peerd::dbus_constants::avahi::kServerMethodEntryGroupNew;
using peerd::dbus_constants::avahi::kServerPath;
using peerd::dbus_constants::avahi::kServiceName;
using peerd::test_util::IsDBusMethodCallTo;
using peerd::test_util::MakeMockCompletionAction;
using peerd::test_util::MakeMockDBusObject;
using peerd::test_util::ReturnsEmptyResponse;
using std::unique_ptr;
using testing::AnyNumber;
using testing::Invoke;
using testing::Property;
using testing::Return;
using testing::_;

namespace {

const char kGroupPath[] = "/this/is/a/group/path";

Response* ReturnsGroupPath(dbus::MethodCall* method_call, int timeout_ms) {
  method_call->SetSerial(87);
  scoped_ptr<Response> response = Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  chromeos::dbus_utils::AppendValueToWriter(&writer, ObjectPath(kGroupPath));
  // The mock wraps this back in a scoped_ptr in the function calling us.
  return response.release();
}

}  // namespace

namespace peerd {

class AvahiServicePublisherTest : public ::testing::Test {
 public:
  void SetUp() override {
    dbus::Bus::Options options;
    mock_bus_ = new dbus::MockBus(options);
    avahi_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), kServiceName, ObjectPath(kServerPath));
    group_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), kServiceName, ObjectPath(kGroupPath));
    publisher_.reset(new AvahiServicePublisher(mock_bus_, avahi_proxy_));
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    // We might try to create a new EntryGroup as part of the test.
    ON_CALL(*avahi_proxy_, MockCallMethodAndBlock(
        IsDBusMethodCallTo(kServerInterface,
                           kServerMethodEntryGroupNew), _))
        .WillByDefault(Invoke(&ReturnsGroupPath));
    ON_CALL(*mock_bus_,
            GetObjectProxy(kServiceName,
                           Property(&ObjectPath::value, kGroupPath)))
      .WillByDefault(Return(group_proxy_.get()));
  }

  unique_ptr<Service> CreateService(const std::string& service_id) {
    auto dbus_object = MakeMockDBusObject();
    EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(1);
    chromeos::ErrorPtr error;
    unique_ptr<Service> service(
        Service::MakeServiceImpl(&error, std::move(dbus_object),
                                 service_id, Service::IpAddresses(),
                                 Service::ServiceInfo(),
                                 MakeMockCompletionAction()));
    EXPECT_EQ(nullptr, error.get());
    EXPECT_NE(nullptr, service.get());
    return service;
  }

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> avahi_proxy_;
  scoped_refptr<dbus::MockObjectProxy> group_proxy_;
  unique_ptr<AvahiServicePublisher> publisher_;
};

TEST_F(AvahiServicePublisherTest, ShouldFreeAddedGroups) {
  EXPECT_CALL(*group_proxy_, MockCallMethodAndBlock(
      IsDBusMethodCallTo(kGroupInterface, kGroupMethodFree), _))
      .WillOnce(Invoke(&ReturnsEmptyResponse));
  auto service = CreateService("service-id");
  EXPECT_TRUE(publisher_->OnServiceUpdated(nullptr, *service));
  publisher_.reset();
}

}  // namespace peerd
