// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <netinet/in.h>

#include <base/macros.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "vm_tools/maitred/service_impl.h"

namespace vm_tools {
namespace maitred {
namespace {

constexpr char kValidAddress[] = "100.115.92.6";
constexpr char kValidNetmask[] = "255.255.255.252";
constexpr char kValidGateway[] = "100.115.92.5";
constexpr char kInvalidConfig[] = R"(ipv4_config {
address: 0
netmask: 0
gateway: 0
})";

class ServiceTest : public ::testing::Test {
 public:
  ServiceTest();
  ~ServiceTest() override = default;

 protected:
  ServiceImpl service_impl_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceTest);
};

ServiceTest::ServiceTest() : service_impl_(nullptr) {}

}  // namespace

// Tests that ConfigureNetwork will reject invalid input.
TEST_F(ServiceTest, ConfigureNetwork_InvalidInput) {
  grpc::ServerContext ctx;

  vm_tools::NetworkConfigRequest request;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kInvalidConfig, &request));

  vm_tools::EmptyMessage response;

  grpc::Status invalid(grpc::INVALID_ARGUMENT, "invalid argument");

  // None of the fields are set.
  grpc::Status result =
      service_impl_.ConfigureNetwork(&ctx, &request, &response);
  EXPECT_EQ(invalid.error_code(), result.error_code());

  // Only one field is valid.
  struct in_addr in;
  ASSERT_GT(inet_pton(AF_INET, kValidNetmask, &in), 0);
  request.mutable_ipv4_config()->set_netmask(in.s_addr);
  result = service_impl_.ConfigureNetwork(&ctx, &request, &response);
  EXPECT_EQ(invalid.error_code(), result.error_code());

  // Two fields are set.
  ASSERT_GT(inet_pton(AF_INET, kValidAddress, &in), 0);
  request.mutable_ipv4_config()->set_address(in.s_addr);
  result = service_impl_.ConfigureNetwork(&ctx, &request, &response);
  EXPECT_EQ(invalid.error_code(), result.error_code());

  // Two different fields are set.
  request.mutable_ipv4_config()->set_address(0);
  ASSERT_GT(inet_pton(AF_INET, kValidGateway, &in), 0);
  request.mutable_ipv4_config()->set_gateway(in.s_addr);
  result = service_impl_.ConfigureNetwork(&ctx, &request, &response);
  EXPECT_EQ(invalid.error_code(), result.error_code());
}

}  // namespace maitred
}  // namespace vm_tools
