// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/service.h"

#include <map>
#include <string>

#include <chromeos/errors/error.h>
#include <gtest/gtest.h>

#include "peerd/test_util.h"

using IpAddresses = peerd::Service::IpAddresses;
using ServiceInfo = peerd::Service::ServiceInfo;
using peerd::errors::service::kInvalidServiceId;
using peerd::errors::service::kInvalidServiceInfo;
using peerd::test_util::MakeMockCompletionAction;
using peerd::test_util::MakeMockDBusObject;
using std::map;
using std::string;
using std::unique_ptr;
using testing::_;

namespace {

IpAddresses MakeValidIpAddresses() {
  // TODO(wiley) return a non-trivial list here.
  return {};
}

}  // namespace


namespace peerd {

class ServiceTest : public ::testing::Test {
 public:
  void AssertMakeServiceFails(const string& service_id,
                              const IpAddresses& addresses,
                              const ServiceInfo& service_info,
                              const string& error_code) {
    auto dbus_object = MakeMockDBusObject();
    EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(0);
    chromeos::ErrorPtr error;
    auto service = Service::MakeServiceImpl(&error, std::move(dbus_object),
                                            service_id, addresses, service_info,
                                            MakeMockCompletionAction());
    ASSERT_NE(nullptr, error.get());
    EXPECT_TRUE(error->HasError(kPeerdErrorDomain, error_code));
    EXPECT_EQ(nullptr, service.get());
  }

  unique_ptr<Service> AssertMakeServiceSuccess(
      const string& service_id,
      const IpAddresses& addresses,
      const ServiceInfo& service_info) {
    auto dbus_object = MakeMockDBusObject();
    EXPECT_CALL(*dbus_object, RegisterAsync(_)).Times(1);
    chromeos::ErrorPtr error;
    unique_ptr<Service> service(
        Service::MakeServiceImpl(&error, std::move(dbus_object),
                                 service_id, addresses, service_info,
                                 MakeMockCompletionAction()));
    EXPECT_EQ(nullptr, error.get());
    EXPECT_NE(nullptr, service.get());
    return service;
  }
};

TEST_F(ServiceTest, ShouldRejectZeroLengthServiceId) {
  AssertMakeServiceFails("",
                         MakeValidIpAddresses(),
                         ServiceInfo(),
                         kInvalidServiceId);
}

TEST_F(ServiceTest, ShouldRejectLongServiceId) {
  AssertMakeServiceFails(string(Service::kMaxServiceIdLength + 1, 'a'),
                         MakeValidIpAddresses(),
                         ServiceInfo(),
                         kInvalidServiceId);
}

TEST_F(ServiceTest, ShouldRejectInvalidCharInServiceId) {
  AssertMakeServiceFails("*_not_allowed",
                         MakeValidIpAddresses(),
                         ServiceInfo(),
                         kInvalidServiceId);
}

TEST_F(ServiceTest, ShouldRejectInvalidCharInServiceInfoKey) {
  const ServiceInfo info = {{"spaces are illegal", "valid value"}};
  AssertMakeServiceFails("valid_id",
                         MakeValidIpAddresses(),
                         info,
                         kInvalidServiceInfo);
}

TEST_F(ServiceTest, ShouldRejectServiceInfoPairTooLong) {
  const ServiceInfo info = {
      {"k", string(Service::kMaxServiceInfoPairLength, 'v')},
  };
  AssertMakeServiceFails("valid_id",
                         MakeValidIpAddresses(),
                         info,
                         kInvalidServiceInfo);
}

TEST_F(ServiceTest, RegisterWhenInputIsValid) {
  auto service = AssertMakeServiceSuccess("valid_id",
                                          MakeValidIpAddresses(),
                                          ServiceInfo());
}

TEST_F(ServiceTest, RegisterWhenInputIsValidBoundaryCases) {
  const ServiceInfo service_info = {
      {"a", string(Service::kMaxServiceInfoPairLength - 1, 'b')},
      {"", ""},
      {"b", ""},
  };
  auto service = AssertMakeServiceSuccess(
      string(Service::kMaxServiceIdLength, 'a'),
      MakeValidIpAddresses(),
      service_info);
}

}  // namespace peerd
