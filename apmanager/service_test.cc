// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/service.h"

#include <string>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/process_mock.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "apmanager/error.h"
#include "apmanager/fake_config_adaptor.h"
#include "apmanager/mock_config.h"
#include "apmanager/mock_control.h"
#include "apmanager/mock_dhcp_server.h"
#include "apmanager/mock_dhcp_server_factory.h"
#include "apmanager/mock_file_writer.h"
#include "apmanager/mock_hostapd_monitor.h"
#include "apmanager/mock_manager.h"
#include "apmanager/mock_process_factory.h"
#include "apmanager/mock_service_adaptor.h"

using brillo::ProcessMock;
using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::SetArgPointee;

namespace {
const int kServiceIdentifier = 1;
const char kHostapdConfig[] = "ssid=test\n";
const char kBinSleep[] = "/bin/sleep";
const char kHostapdConfigFilePath[] = "/run/apmanager/hostapd/hostapd-1.conf";
}  // namespace

namespace apmanager {

class ServiceTest : public testing::Test {
 public:
  ServiceTest()
      : manager_(&control_interface_),
        hostapd_monitor_(new MockHostapdMonitor()) {
    ON_CALL(control_interface_, CreateServiceAdaptorRaw())
        .WillByDefault(ReturnNew<MockServiceAdaptor>());
    ON_CALL(control_interface_, CreateConfigAdaptorRaw())
        .WillByDefault(ReturnNew<FakeConfigAdaptor>());
    // Defer creation of Service object to allow ControlInterface to
    // setup expectations for generating fake adaptors.
    service_ = new Service(&manager_, kServiceIdentifier);
  }
  virtual void SetUp() {
    service_->dhcp_server_factory_ = &dhcp_server_factory_;
    service_->file_writer_ = &file_writer_;
    service_->process_factory_ = &process_factory_;
    service_->hostapd_monitor_.reset(hostapd_monitor_);
  }

  bool StartService(Error* error) {
    return service_->StartInternal(error);
  }

  void StartDummyProcess() {
    service_->hostapd_process_.reset(new brillo::ProcessImpl);
    service_->hostapd_process_->AddArg(kBinSleep);
    service_->hostapd_process_->AddArg("12345");
    CHECK(service_->hostapd_process_->Start());
    LOG(INFO) << "DummyProcess: " << service_->hostapd_process_->pid();
  }

  void SetConfig(Config* config) {
    service_->config_.reset(config);
  }

  void VerifyError(const Error& error,
                   Error::Type expected_type,
                   const std::string& expected_message_start) {
    EXPECT_EQ(expected_type, error.type());
    EXPECT_TRUE(base::StartsWith(error.message(), expected_message_start,
                                 base::CompareCase::INSENSITIVE_ASCII));
  }

 protected:
  MockControl control_interface_;
  MockManager manager_;
  MockDHCPServerFactory dhcp_server_factory_;
  MockFileWriter file_writer_;
  MockProcessFactory process_factory_;
  MockHostapdMonitor* hostapd_monitor_;
  scoped_refptr<Service> service_;
};

TEST_F(ServiceTest, StartWhenServiceAlreadyRunning) {
  StartDummyProcess();

  Error error;
  EXPECT_FALSE(StartService(&error));
  VerifyError(error, Error::kInternalError, "Service already running");
}

TEST_F(ServiceTest, StartWhenConfigFileFailed) {
  MockConfig* config = new MockConfig(&manager_);
  SetConfig(config);

  Error error;
  EXPECT_CALL(*config, GenerateConfigFile(_, _)).WillOnce(Return(false));
  EXPECT_FALSE(StartService(&error));
}

TEST_F(ServiceTest, StartSuccess) {
  MockConfig* config = new MockConfig(&manager_);
  SetConfig(config);

  // Setup mock DHCP server.
  MockDHCPServer* dhcp_server = new MockDHCPServer();
  // Setup mock process.
  ProcessMock* process = new ProcessMock();

  std::string config_str(kHostapdConfig);
  Error error;
  EXPECT_CALL(*config, GenerateConfigFile(_, _)).WillOnce(
      DoAll(SetArgPointee<1>(config_str), Return(true)));
  EXPECT_CALL(file_writer_, Write(kHostapdConfigFilePath, kHostapdConfig))
      .WillOnce(Return(true));
  EXPECT_CALL(*config, ClaimDevice()).WillOnce(Return(true));
  EXPECT_CALL(process_factory_, CreateProcess()).WillOnce(Return(process));
  EXPECT_CALL(*process, Start()).WillOnce(Return(true));
  EXPECT_CALL(dhcp_server_factory_, CreateDHCPServer(_, _))
      .WillOnce(Return(dhcp_server));
  EXPECT_CALL(*dhcp_server, Start()).WillOnce(Return(true));
  EXPECT_CALL(manager_, RequestDHCPPortAccess(_));
  EXPECT_CALL(*hostapd_monitor_, Start());
  EXPECT_TRUE(StartService(&error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(ServiceTest, StopWhenServiceNotRunning) {
  Error error;
  EXPECT_FALSE(service_->Stop(&error));
  VerifyError(
      error, Error::kInternalError, "Service is not currently running");
}

TEST_F(ServiceTest, StopSuccess) {
  StartDummyProcess();

  MockConfig* config = new MockConfig(&manager_);
  SetConfig(config);
  Error error;
  EXPECT_CALL(*config, ReleaseDevice()).Times(1);
  EXPECT_TRUE(service_->Stop(&error));
  Mock::VerifyAndClearExpectations(config);
}

}  // namespace apmanager
