// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "chromeos/syslog_logging.h"
#include "chromeos/test_helpers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "vpn-manager/service_manager.h"

using ::chromeos::ClearLog;
using ::chromeos::FindLog;
using ::chromeos::GetLog;
using ::testing::InSequence;
using ::testing::Return;

class MockService : public ServiceManager {
 public:
  MockService() : ServiceManager("mock") {}
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Poll, int());
  MOCK_METHOD0(ProcessOutput, void());
  MOCK_METHOD1(IsChild, bool(pid_t pid));
};

class ServiceManagerTest : public ::testing::Test {
 public:
  void SetUp() {
    test_path_ = FilePath("test");
    file_util::Delete(test_path_, true);
    file_util::CreateDirectory(test_path_);
    temp_path_ = test_path_.Append("service");
    ServiceManager::temp_base_path_ = temp_path_.value().c_str();
    ServiceManager::temp_path_ = &temp_path_;
    ServiceManager::SetLayerOrder(&outer_service_, &inner_service_);
    chromeos::ClearLog();
  }
  void TearDown() {
    ServiceManager::temp_base_path_ = NULL;
    ServiceManager::temp_path_ = NULL;
  }
 protected:
  FilePath temp_path_;
  FilePath test_path_;
  MockService outer_service_;
  MockService inner_service_;
  MockService single_service_;
};

TEST_F(ServiceManagerTest, InitializeDirectories) {
  FilePath picked_temp;
  {
    ScopedTempDir my_temp;
    EXPECT_FALSE(my_temp.IsValid());
    ServiceManager::InitializeDirectories(&my_temp);
    EXPECT_TRUE(my_temp.IsValid());
    picked_temp = my_temp.path();
    EXPECT_TRUE(file_util::DirectoryExists(picked_temp));
  }
  EXPECT_FALSE(file_util::DirectoryExists(picked_temp));
}

TEST_F(ServiceManagerTest, OnStartedInnerSucceeds) {
  EXPECT_CALL(inner_service_, Start()).WillOnce(Return(true));
  EXPECT_FALSE(outer_service_.is_running());
  EXPECT_FALSE(outer_service_.was_stopped());
  outer_service_.OnStarted();
  EXPECT_TRUE(outer_service_.is_running());
  EXPECT_FALSE(outer_service_.was_stopped());
}

TEST_F(ServiceManagerTest, OnStartedInnerFails) {
  InSequence unused;
  EXPECT_CALL(inner_service_, Start()).WillOnce(Return(false));
  EXPECT_CALL(outer_service_, Stop());
  EXPECT_FALSE(outer_service_.is_running());
  outer_service_.OnStarted();
  // The outer service keeps saying it's running until its OnStop is called.
  EXPECT_TRUE(outer_service_.is_running());
  EXPECT_TRUE(FindLog("Inner service mock failed"));
}

TEST_F(ServiceManagerTest, OnStartedNoInner) {
  EXPECT_FALSE(single_service_.is_running());
  EXPECT_FALSE(single_service_.was_stopped());
  single_service_.OnStarted();
  EXPECT_TRUE(single_service_.is_running());
  EXPECT_FALSE(single_service_.was_stopped());
}

TEST_F(ServiceManagerTest, OnStoppedFromSuccess) {
  EXPECT_CALL(outer_service_, Stop());
  inner_service_.is_running_ = true;
  EXPECT_TRUE(inner_service_.is_running());
  EXPECT_FALSE(inner_service_.was_stopped());
  inner_service_.OnStopped(true);
  EXPECT_FALSE(inner_service_.is_running());
  EXPECT_TRUE(inner_service_.was_stopped());
}

TEST_F(ServiceManagerTest, OnStoppedFromFailure) {
  EXPECT_CALL(outer_service_, Stop());
  inner_service_.is_running_ = true;
  EXPECT_TRUE(inner_service_.is_running());
  EXPECT_FALSE(inner_service_.was_stopped());
  inner_service_.OnStopped(false);
  EXPECT_FALSE(inner_service_.is_running());
  EXPECT_TRUE(inner_service_.was_stopped());
}

TEST_F(ServiceManagerTest, WriteFdToSyslog) {
  int mypipe[2];
  ASSERT_EQ(0, pipe(mypipe));
  std::string partial;

  const char kMessage1[] = "good morning\npipe\n";
  EXPECT_EQ(strlen(kMessage1), write(mypipe[1], kMessage1, strlen(kMessage1)));
  ServiceManager::WriteFdToSyslog(mypipe[0], "prefix: ", &partial);
  EXPECT_EQ("prefix: good morning\nprefix: pipe\n", GetLog());
  EXPECT_EQ("", partial);

  ClearLog();

  const char kMessage2[] = "partial line";
  EXPECT_EQ(strlen(kMessage2), write(mypipe[1], kMessage2, strlen(kMessage2)));
  ServiceManager::WriteFdToSyslog(mypipe[0], "prefix: ", &partial);
  EXPECT_EQ(kMessage2, partial);
  EXPECT_EQ("", GetLog());

  const char kMessage3[] = " end\nbegin\nlast";
  EXPECT_EQ(strlen(kMessage3), write(mypipe[1], kMessage3, strlen(kMessage3)));
  ServiceManager::WriteFdToSyslog(mypipe[0], "prefix: ", &partial);
  EXPECT_EQ("last", partial);
  EXPECT_EQ("prefix: partial line end\nprefix: begin\n", GetLog());

  close(mypipe[0]);
  close(mypipe[1]);
}

TEST_F(ServiceManagerTest, GetLocalAddressFromRemote) {
  struct sockaddr remote_address;
  struct sockaddr local_address;
  std::string local_address_text;
  EXPECT_TRUE(ServiceManager::ConvertIPStringToSockAddr("127.0.0.1",
                                                        &remote_address));
  EXPECT_TRUE(ServiceManager::GetLocalAddressFromRemote(remote_address,
                                                        &local_address));
  EXPECT_TRUE(ServiceManager::ConvertSockAddrToIPString(local_address,
                                                        &local_address_text));
  EXPECT_EQ("127.0.0.1", local_address_text);
}

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
