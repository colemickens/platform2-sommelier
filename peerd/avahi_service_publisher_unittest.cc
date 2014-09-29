// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

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
using peerd::dbus_constants::avahi::kGroupMethodAddService;
using peerd::dbus_constants::avahi::kGroupMethodCommit;
using peerd::dbus_constants::avahi::kGroupMethodFree;
using peerd::dbus_constants::avahi::kGroupMethodReset;
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
using testing::Unused;
using testing::_;

namespace {

const char kHost[] = "this_is_a_hostname";
const char kGroupPath[] = "/this/is/a/group/path";
const char kServiceId[] = "service-id";

Response* ReturnsGroupPath(dbus::MethodCall* method_call, Unused, Unused) {
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
    publisher_.reset(new AvahiServicePublisher(kHost, "uuid", "name", "note",
                                               mock_bus_, avahi_proxy_));
    // Ignore threading concerns.
    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*mock_bus_, AssertOnDBusThread()).Times(AnyNumber());
    // We might try to create new EntryGroups as part of the test.
    ON_CALL(*avahi_proxy_, MockCallMethodAndBlockWithErrorDetails(
        IsDBusMethodCallTo(kServerInterface,
                           kServerMethodEntryGroupNew), _, _))
        .WillByDefault(Invoke(&ReturnsGroupPath));
    ON_CALL(*mock_bus_,
            GetObjectProxy(kServiceName,
                           Property(&ObjectPath::value, kGroupPath)))
      .WillByDefault(Return(group_proxy_.get()));
  }

  unique_ptr<Service> CreateServiceWithInfo(const std::string& service_id,
                                            const Service::ServiceInfo& info) {
    auto dbus_object = MakeMockDBusObject();
    EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(1);
    chromeos::ErrorPtr error;
    unique_ptr<Service> service(
        Service::MakeServiceImpl(&error, std::move(dbus_object),
                                 service_id, Service::IpAddresses(),
                                 info,
                                 MakeMockCompletionAction()));
    EXPECT_EQ(nullptr, error.get());
    EXPECT_NE(nullptr, service.get());
    return service;
  }

  unique_ptr<Service> CreateService(const std::string& service_id) {
    return CreateServiceWithInfo(service_id, {});
  }

  void ExpectServiceAdded() {
    // Each new service triggers two AddService calls and two Commit calls.
    // The first of each is for the service itself.  The second set is for
    // the master Serbus record.
    EXPECT_CALL(*group_proxy_, MockCallMethodAndBlockWithErrorDetails(
        IsDBusMethodCallTo(kGroupInterface, kGroupMethodAddService), _, _))
        .Times(2)
        .WillRepeatedly(Invoke(&ReturnsEmptyResponse));
    EXPECT_CALL(*group_proxy_, MockCallMethodAndBlockWithErrorDetails(
        IsDBusMethodCallTo(kGroupInterface, kGroupMethodCommit), _, _))
        .Times(2)
        .WillRepeatedly(Invoke(&ReturnsEmptyResponse));
  }

  void ExpectServiceModified() {
    // When modifying existing services, we reset both the service in
    // question and the master record.
    EXPECT_CALL(*group_proxy_, MockCallMethodAndBlockWithErrorDetails(
        IsDBusMethodCallTo(kGroupInterface, kGroupMethodReset), _, _))
        .Times(2)
        .WillRepeatedly(Invoke(&ReturnsEmptyResponse));
    ExpectServiceAdded();
  }

  void ExpectFinalServiceFree() {
    // The final service free also removes the master record.
    EXPECT_CALL(*group_proxy_, MockCallMethodAndBlockWithErrorDetails(
        IsDBusMethodCallTo(kGroupInterface, kGroupMethodFree), _, _))
        .Times(2)
        .WillRepeatedly(Invoke(&ReturnsEmptyResponse));
  }

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> avahi_proxy_;
  scoped_refptr<dbus::MockObjectProxy> group_proxy_;
  unique_ptr<AvahiServicePublisher> publisher_;
};

TEST_F(AvahiServicePublisherTest, ShouldFreeAddedGroups) {
  auto service = CreateService(kServiceId);
  ExpectServiceAdded();
  EXPECT_TRUE(publisher_->OnServiceUpdated(nullptr, *service));
  ExpectFinalServiceFree();
  publisher_.reset();
}

TEST_F(AvahiServicePublisherTest, CanEncodeTxtRecords) {
  auto service = CreateServiceWithInfo(
      kServiceId, {{"a", "w"}, {"b", "foo"}});
  std::vector<std::vector<uint8_t>> result{{'a', '=', 'w'},
                                           {'b', '=', 'f', 'o', 'o'}};
  EXPECT_EQ(result,
            AvahiServicePublisher::GetTxtRecord(service->GetServiceInfo()));
}

TEST_F(AvahiServicePublisherTest, CanHandleServiceUpdates) {
  auto service = CreateService(kServiceId);
  ExpectServiceAdded();
  EXPECT_TRUE(publisher_->OnServiceUpdated(nullptr, *service));
  ExpectServiceModified();
  // The service publisher doesn't actually know anything about the previous
  // values inside the service.  So we don't need to mutate the service.
  EXPECT_TRUE(publisher_->OnServiceUpdated(nullptr, *service));
  ExpectFinalServiceFree();
}

TEST_F(AvahiServicePublisherTest, CanRemoveService) {
  auto service = CreateService(kServiceId);
  ExpectServiceAdded();
  EXPECT_TRUE(publisher_->OnServiceUpdated(nullptr, *service));
  ExpectFinalServiceFree();
  EXPECT_TRUE(publisher_->OnServiceRemoved(nullptr, kServiceId));
  publisher_.reset();  // Note that we should *not* see a Group.Free() call.
}

}  // namespace peerd
