// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/service.h"

#include <string>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/process_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "apmanager/mock_config.h"
#include "apmanager/mock_dhcp_server.h"
#include "apmanager/mock_dhcp_server_factory.h"
#include "apmanager/mock_file_writer.h"
#include "apmanager/mock_manager.h"

using chromeos::ProcessMock;
using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace {
  const int kServiceIdentifier = 1;
  const char kHostapdConfig[] = "ssid=test\n";
  const char kBinSleep[] = "/bin/sleep";
  const char kHostapdConfigFilePath[] =
      "/var/run/apmanager/hostapd/hostapd-1.conf";
}  // namespace

namespace apmanager {

class ServiceTest : public testing::Test {
 public:
  ServiceTest() : service_(&manager_, kServiceIdentifier) {}

  void StartDummyProcess() {
    service_.hostapd_process_.reset(new chromeos::ProcessImpl);
    service_.hostapd_process_->AddArg(kBinSleep);
    service_.hostapd_process_->AddArg("12345");
    CHECK(service_.hostapd_process_->Start());
    LOG(INFO) << "DummyProcess: " << service_.hostapd_process_->pid();
  }

  void SetConfig(Config* config) {
    service_.config_.reset(config);
  }

  void SetDHCPServerFactory(DHCPServerFactory* factory) {
    service_.dhcp_server_factory_ = factory;
  }

  void SetFileWriter(FileWriter* file_writer) {
    service_.file_writer_ = file_writer;
  }

 protected:
  Service service_;
  MockManager manager_;
};

MATCHER_P(IsServiceErrorStartingWith, message, "") {
  return arg != nullptr &&
      arg->GetDomain() == chromeos::errors::dbus::kDomain &&
      arg->GetCode() == kServiceError &&
      EndsWith(arg->GetMessage(), message, false);
}

TEST_F(ServiceTest, StartWhenServiceAlreadyRunning) {
  StartDummyProcess();

  chromeos::ErrorPtr error;
  EXPECT_FALSE(service_.Start(&error));
  EXPECT_THAT(error, IsServiceErrorStartingWith("Service already running"));
}

TEST_F(ServiceTest, StartWhenConfigFileFailed) {
  MockConfig* config = new MockConfig();
  SetConfig(config);

  chromeos::ErrorPtr error;
  EXPECT_CALL(*config, GenerateConfigFile(_, _)).WillOnce(Return(false));
  EXPECT_FALSE(service_.Start(&error));
  EXPECT_THAT(error, IsServiceErrorStartingWith(
      "Failed to generate config file"));
}

TEST_F(ServiceTest, StartSuccess) {
  MockConfig* config = new MockConfig();
  SetConfig(config);

  // Setup mock DHCP server.
  MockDHCPServerFactory* dhcp_server_factory =
      MockDHCPServerFactory::GetInstance();
  MockDHCPServer* dhcp_server = new MockDHCPServer();
  SetDHCPServerFactory(dhcp_server_factory);

  // Setup mock file writer.
  MockFileWriter* file_writer = MockFileWriter::GetInstance();
  SetFileWriter(file_writer);

  std::string config_str(kHostapdConfig);
  chromeos::ErrorPtr error;
  EXPECT_CALL(*config, GenerateConfigFile(_, _)).WillOnce(
      DoAll(SetArgPointee<1>(config_str), Return(true)));
  EXPECT_CALL(*file_writer, Write(kHostapdConfigFilePath, kHostapdConfig))
      .WillOnce(Return(true));
  EXPECT_CALL(*config, ClaimDevice()).WillOnce(Return(true));
  EXPECT_CALL(*dhcp_server_factory, CreateDHCPServer(_, _))
      .WillOnce(Return(dhcp_server));
  EXPECT_CALL(*dhcp_server, Start()).WillOnce(Return(true));
  EXPECT_TRUE(service_.Start(&error));
  EXPECT_EQ(nullptr, error);
}

TEST_F(ServiceTest, StopWhenServiceNotRunning) {
  chromeos::ErrorPtr error;
  EXPECT_FALSE(service_.Stop(&error));
  EXPECT_THAT(error, IsServiceErrorStartingWith(
      "Service is not currently running"));
}

TEST_F(ServiceTest, StopSuccess) {
  StartDummyProcess();

  MockConfig* config = new MockConfig();
  SetConfig(config);
  chromeos::ErrorPtr error;
  EXPECT_CALL(*config, ReleaseDevice()).Times(1);
  EXPECT_TRUE(service_.Stop(&error));
}

}  // namespace apmanager
