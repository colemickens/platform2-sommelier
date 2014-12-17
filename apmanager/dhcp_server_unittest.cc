// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/dhcp_server.h"

#include <string>

#include <net/if.h>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/process_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <shill/net/mock_rtnl_handler.h>

using chromeos::ProcessMock;
using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using std::string;

namespace {
  const uint16_t kServerAddressIndex = 1;
  const char kTestInterfaceName[] = "test_interface";
  const char kBinSleep[] = "/bin/sleep";
  const char kExpectedDnsmasqConfigFile[] =
      "port=0\n"
      "bind-interfaces\n"
      "log-dhcp\n"
      "keep-in-foreground\n"
      "user=apmanager\n"
      "dhcp-range=192.168.1.1,192.168.1.128\n"
      "interface=test_interface\n"
      "dhcp-leasefile=/tmp/dhcpd-1.leases\n";
}  // namespace

namespace apmanager {

class DHCPServerTest : public testing::Test {
 public:
  DHCPServerTest()
      : dhcp_server_(new DHCPServer(kServerAddressIndex, kTestInterfaceName)),
        rtnl_handler_(new shill::MockRTNLHandler()) {}
  virtual ~DHCPServerTest() {}

  virtual void SetUp() {
    dhcp_server_->rtnl_handler_ = rtnl_handler_.get();
  }

  virtual void TearDown() {
    // Reset DHCP server now while RTNLHandler is still valid.
    dhcp_server_.reset();
  }

  void StartDummyProcess() {
    dhcp_server_->dnsmasq_process_.reset(new chromeos::ProcessImpl);
    dhcp_server_->dnsmasq_process_->AddArg(kBinSleep);
    dhcp_server_->dnsmasq_process_->AddArg("12345");
    CHECK(dhcp_server_->dnsmasq_process_->Start());
  }

  string GenerateConfigFile() {
    return dhcp_server_->GenerateConfigFile();
  }

 protected:
  std::unique_ptr<DHCPServer> dhcp_server_;
  std::unique_ptr<shill::MockRTNLHandler> rtnl_handler_;
};


TEST_F(DHCPServerTest, GenerateConfigFile) {
  string config_content = GenerateConfigFile();
  EXPECT_STREQ(kExpectedDnsmasqConfigFile, config_content.c_str())
      << "Expected to find the following config...\n"
      << kExpectedDnsmasqConfigFile << ".....\n"
      << config_content;
}

TEST_F(DHCPServerTest, StartWhenServerAlreadyStarted) {
  StartDummyProcess();

  EXPECT_FALSE(dhcp_server_->Start());
}

TEST_F(DHCPServerTest, StartSuccess) {
  const int kInterfaceIndex = 1;
  EXPECT_CALL(*rtnl_handler_.get(), GetInterfaceIndex(kTestInterfaceName))
      .WillOnce(Return(kInterfaceIndex));
  EXPECT_CALL(*rtnl_handler_.get(),
      AddInterfaceAddress(kInterfaceIndex, _, _, _)).Times(1);
  EXPECT_CALL(*rtnl_handler_.get(),
      SetInterfaceFlags(kInterfaceIndex, IFF_UP, IFF_UP)).Times(1);
  // This will attempt to start a real dnsmasq process, and won't cause any
  // trouble since the interface name specified is invalid. This can be avoid
  // once we move to use ProcessFactory for process creation, which allows us
  // to use the MockProcess instead of the real Process.
  EXPECT_TRUE(dhcp_server_->Start());
  Mock::VerifyAndClearExpectations(rtnl_handler_.get());
}

}  // namespace apmanager
