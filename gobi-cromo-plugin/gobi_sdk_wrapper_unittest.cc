// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_sdk_wrapper.h"

#include <base/memory/scoped_ptr.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

namespace gobi {
class GobiSdkTest : public ::testing::Test {
  void SetUp() {
    sdk_.reset(new Sdk(SinkSdkError));
    sdk_->Init();
  }

  static void SinkSdkError(const std::string &modem_path,
                           const std::string &sdk_function,
                           ULONG error) {
    LOG(FATAL) << "Unexpected sdk error";
  }
 public:
  scoped_ptr<gobi::Sdk> sdk_;
};

TEST_F(GobiSdkTest, InitGetServiceFromName) {
  int service;

  service = sdk_->GetServiceFromName("GetSessionState");
  EXPECT_STREQ("WirelessData", sdk_->index_to_service_name_[service]);

  service = sdk_->GetServiceFromName("QCWWANConnect");
  EXPECT_STREQ("Base", sdk_->index_to_service_name_[service]);
}

const char *kDoesNotStartWithService[] = {
  "FunctionName",
  NULL,
};

const char *kEmptyServiceName[] = {
  "",
  NULL,
};

TEST_F(GobiSdkTest, InitGetServiceFromNameDeathTest) {
  // We cannot use the DISABLE_ prefix because that breaks friend_test
  LOG(ERROR) << "Skipping this test";
  return;
  EXPECT_DEATH(sdk_->InitGetServiceFromName(kDoesNotStartWithService),
               "service_index >= 0");
  EXPECT_DEATH(sdk_->InitGetServiceFromName(kEmptyServiceName),
               "Empty servicename");
}

TEST_F(GobiSdkTest, BaseClosesAllDeathTest) {
  LOG(ERROR) << "Skipping this test";
  return;
  sdk_->EnterSdk("QCWWANConnect");
  for (int i = 0; i < sdk_->service_index_upper_bound_; ++i) {
    EXPECT_STREQ("QCWWANConnect", sdk_->service_to_function_[i]);
  }
  sdk_->LeaveSdk("QCWWANConnect");
  for (int i = 0; i < sdk_->service_index_upper_bound_; ++i) {
    EXPECT_EQ(NULL, sdk_->service_to_function_[i]);
  }

  sdk_->EnterSdk("QCWWANConnect");

  EXPECT_DEATH(sdk_->EnterSdk("QCWWANConnect"), "Reentrant SDK access");
  EXPECT_DEATH(sdk_->EnterSdk("GetSessionState"), "Reentrant SDK access");
}

TEST_F(GobiSdkTest, EnterLeaveDeathTest) {
  LOG(ERROR) << "Skipping this test";
  return;

  sdk_->EnterSdk("OMADMStartSession");
  SUCCEED() << "OMADMStartSession crash avoided";

  EXPECT_DEATH(sdk_->EnterSdk("unknown function"), "Invalid function");

  static int kWirelessData = 2;
  EXPECT_STREQ("WirelessData",
               sdk_->index_to_service_name_[kWirelessData]);

  static int kNetworkAccess = 3;
  EXPECT_STREQ("NetworkAccess",
               sdk_->index_to_service_name_[kNetworkAccess]);

  sdk_->EnterSdk("GetSessionState");
  EXPECT_STREQ("GetSessionState", sdk_->service_to_function_[kWirelessData]);

  sdk_->EnterSdk("GetRFInfo");
  EXPECT_STREQ("GetRFInfo", sdk_->service_to_function_[kNetworkAccess]);

  // Another function from WirelessData
  EXPECT_DEATH(sdk_->EnterSdk("GetConnectionRate"),
               "Reentrant SDK access");

  // Leave GetSessionState; another WirelessData function should now
  // be safe
  sdk_->LeaveSdk("GetSessionState");
  sdk_->EnterSdk("StartDataSession");
  EXPECT_STREQ("StartDataSession", sdk_->service_to_function_[kWirelessData]);

  // Last function should work too
  static char last_function[] = "SetOMADMStateCallback";
  static int kLastService = 11;
  EXPECT_STREQ("Callback",
               sdk_->index_to_service_name_[kLastService]);
  EXPECT_EQ(sdk_->service_index_upper_bound_, kLastService + 1);
  sdk_->EnterSdk(last_function);
  EXPECT_STREQ(last_function,
               sdk_->service_to_function_[kLastService]);
}

TEST_F(GobiSdkTest, InterleavedSdkCalls) {
  Sdk::CallWrapper start_data_session(sdk_.get(), "StartDataSession");
  {
    Sdk::CallWrapper qcwwan_disconnect(sdk_.get(), "QCWWANDisconnect");
    EXPECT_TRUE(qcwwan_disconnect.sdk_locked_);
  }
  Sdk::CallWrapper qcwwan_enumerate_devices(sdk_.get(),
                                            "QCWWANEnumerateDevices");
  EXPECT_FALSE(qcwwan_enumerate_devices.sdk_locked_);
}

TEST_F(GobiSdkTest, TemporaryCopier) {
  gobi::CharStarCopier a(NULL);
  EXPECT_EQ(NULL, a.get());

  gobi::CharStarCopier b("");
  EXPECT_NE(static_cast<const char *>(NULL), b.get());
  EXPECT_STREQ("", b.get());

  const char *hello="hello";
  for (int i = 0; i < (1 << 16); ++i) {
    gobi::CharStarCopier c(hello);
    EXPECT_STREQ(hello, c.get());
    EXPECT_NE(hello, c.get());
  }
}
}  // namespace gobi

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  // Documentation says this can make tests run slower, but we're
  // still at << 1s for all our death tests, and using "threadsafe"
  // quiets warnings from gtest
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  return RUN_ALL_TESTS();
}
