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

using chromeos::ProcessMock;
using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace {
  const int kServiceIdentifier = 1;
  const char kHostapdConfig[] = "ssid=test\n";
}  // namespace

namespace apmanager {

class ServiceTest : public testing::Test {
 public:
  ServiceTest() : service_(kServiceIdentifier) {}

  void StartDummyProcess() {
    service_.hostapd_process_.reset(new chromeos::ProcessImpl);
    service_.hostapd_process_->AddArg("sleep");
    service_.hostapd_process_->AddArg("12345");
    CHECK(service_.hostapd_process_->Start());
    LOG(INFO) << "DummyProcess: " << service_.hostapd_process_->pid();
  }

  void SetConfig(Config* config) {
    service_.config_.reset(config);
  }

 protected:
  Service service_;
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

  std::string config_str(kHostapdConfig);
  chromeos::ErrorPtr error;
  EXPECT_CALL(*config, GenerateConfigFile(_, _)).WillOnce(
      DoAll(SetArgPointee<1>(config_str), Return(true)));
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

  chromeos::ErrorPtr error;
  EXPECT_TRUE(service_.Stop(&error));
}

}  // namespace apmanager
