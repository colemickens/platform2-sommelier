//
// Copyright 2016 The Chromium OS Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/throttler.h"

#include "shill/mock_control.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_file_io.h"
#include "shill/mock_log.h"
#include "shill/mock_manager.h"
#include "shill/mock_process_manager.h"
#include "shill/testing.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

class ThrottlerTest : public Test {
 public:
  ThrottlerTest()
      : mock_manager_(&control_interface_, &dispatcher_, nullptr),
        throttler_(&dispatcher_, &mock_manager_) {
    throttler_.process_manager_ = &mock_process_manager_;
    throttler_.file_io_ = &mock_file_io_;
  }

 protected:
  static const char kIfaceName0[];
  static const char kIfaceName1[];
  static const char kIfaceName2[];
  static const pid_t kPID1;
  static const pid_t kPID2;
  static const pid_t kPID3;
  static const uint32_t kThrottleRate;

  MockControl control_interface_;
  MockEventDispatcher dispatcher_;
  StrictMock<MockManager> mock_manager_;
  NiceMock<MockProcessManager> mock_process_manager_;
  NiceMock<MockFileIO> mock_file_io_;
  Throttler throttler_;
};

const char ThrottlerTest::kIfaceName0[] = "eth0";
const char ThrottlerTest::kIfaceName1[] = "wlan0";
const char ThrottlerTest::kIfaceName2[] = "ppp0";
const pid_t ThrottlerTest::kPID1 = 9900;
const pid_t ThrottlerTest::kPID2 = 9901;
const pid_t ThrottlerTest::kPID3 = 9902;
const uint32_t ThrottlerTest::kThrottleRate = 100;

TEST_F(ThrottlerTest, ThrottleCallsTCExpectedTimesAndSetsState) {
  std::vector<std::string> interfaces;
  interfaces.push_back(kIfaceName0);
  interfaces.push_back(kIfaceName1);
  EXPECT_CALL(mock_manager_, GetDeviceInterfaceNames())
      .WillOnce(Return(interfaces));
  EXPECT_CALL(mock_process_manager_,
              StartProcessInMinijailWithPipes(
                  _, base::FilePath(Throttler::kTCPath), _, Throttler::kTCUser,
                  Throttler::kTCGroup, CAP_TO_MASK(CAP_NET_ADMIN), _, _,
                  nullptr, nullptr))
      .Times(interfaces.size())
      .WillOnce(Return(kPID1))
      .WillOnce(Return(kPID2));
  EXPECT_CALL(mock_file_io_, SetFdNonBlocking(_))
      .Times(interfaces.size())
      .WillRepeatedly(Return(false));
  const ResultCallback callback;
  throttler_.ThrottleInterfaces(callback, kThrottleRate, kThrottleRate);
  throttler_.OnProcessExited(0);
  throttler_.OnProcessExited(0);
  EXPECT_TRUE(throttler_.desired_throttling_enabled_);
  EXPECT_EQ(throttler_.desired_upload_rate_kbits_, kThrottleRate);
  EXPECT_EQ(throttler_.desired_download_rate_kbits_, kThrottleRate);
}

TEST_F(ThrottlerTest, NewlyAddedInterfaceIsThrottled) {
  throttler_.desired_throttling_enabled_ = true;
  throttler_.desired_upload_rate_kbits_ = kThrottleRate;
  throttler_.desired_download_rate_kbits_ = kThrottleRate;
  EXPECT_CALL(mock_process_manager_,
              StartProcessInMinijailWithPipes(
                  _, base::FilePath(Throttler::kTCPath), _, Throttler::kTCUser,
                  Throttler::kTCGroup, CAP_TO_MASK(CAP_NET_ADMIN), _, _,
                  nullptr, nullptr))
      .Times(1)
      .WillOnce(Return(kPID3));
  EXPECT_CALL(mock_file_io_, SetFdNonBlocking(_)).WillOnce(Return(false));
  throttler_.ApplyThrottleToNewInterface(kIfaceName2);
}

TEST_F(ThrottlerTest, DisablingThrottleClearsState) {
  throttler_.desired_throttling_enabled_ = true;
  throttler_.desired_upload_rate_kbits_ = kThrottleRate;
  throttler_.desired_download_rate_kbits_ = kThrottleRate;
  std::vector<std::string> interfaces;
  interfaces.push_back(kIfaceName0);
  EXPECT_CALL(mock_manager_, GetDeviceInterfaceNames())
      .WillOnce(Return(interfaces));
  EXPECT_CALL(mock_process_manager_,
              StartProcessInMinijailWithPipes(
                  _, base::FilePath(Throttler::kTCPath), _, Throttler::kTCUser,
                  Throttler::kTCGroup, CAP_TO_MASK(CAP_NET_ADMIN), _, _,
                  nullptr, nullptr))
      .Times(1)
      .WillOnce(Return(kPID1));
  EXPECT_CALL(mock_file_io_, SetFdNonBlocking(_))
      .Times(interfaces.size())
      .WillRepeatedly(Return(false));
  const ResultCallback callback;
  throttler_.DisableThrottlingOnAllInterfaces(callback);
  throttler_.OnProcessExited(0);
  EXPECT_FALSE(throttler_.desired_throttling_enabled_);
  EXPECT_EQ(throttler_.desired_upload_rate_kbits_, 0);
  EXPECT_EQ(throttler_.desired_download_rate_kbits_, 0);
}

}  // namespace shill
