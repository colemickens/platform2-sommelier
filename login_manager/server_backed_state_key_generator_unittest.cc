// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/server_backed_state_key_generator.h"

#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <set>
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gtest/gtest.h>

#include "login_manager/system_utils_impl.h"

namespace login_manager {

namespace {

// A SystemUtils implementation that mocks time.
class FakeSystemUtils : public SystemUtilsImpl {
 public:
  FakeSystemUtils() : time_(0) {}
  virtual ~FakeSystemUtils() {}

  time_t time(time_t* t) override {
    if (t)
      *t = time_;
    return time_;
  }

  void forward_time(time_t offset) { time_ += offset; }

 private:
  // Current time.
  time_t time_;

  DISALLOW_COPY_AND_ASSIGN(FakeSystemUtils);
};

}  // namespace

class ServerBackedStateKeyGeneratorTest : public ::testing::Test {
 public:
  ServerBackedStateKeyGeneratorTest()
      : generator_(&system_utils_), state_keys_received_(false) {}
  virtual ~ServerBackedStateKeyGeneratorTest() {}

  // Installs mock data for the required parameters.
  void InitMachineInfo(bool first_boot) {
    std::map<std::string, std::string> params;
    params["serial_number"] = "fake-machine-serial-number";
    params["root_disk_serial_number"] = "fake-disk-serial-number";
    if (!first_boot)
      params["ActivateDate"] = "2001-51";
    ASSERT_TRUE(generator_.InitMachineInfo(params));
  }

  void CompletionHandler(const std::vector<std::vector<uint8_t> >& state_keys,
                         bool first_boot) {
    state_keys_received_ = true;
    state_keys_ = state_keys;
    first_boot_ = first_boot;
  }

  void RequestStateKeys(bool expect_immediate_callback) {
    state_keys_received_ = false;
    state_keys_.clear();
    generator_.RequestStateKeys(
        base::Bind(&ServerBackedStateKeyGeneratorTest::CompletionHandler,
                   base::Unretained(this)));
    EXPECT_EQ(expect_immediate_callback, state_keys_received_);
  }

  FakeSystemUtils system_utils_;

  ServerBackedStateKeyGenerator generator_;

  bool state_keys_received_;
  std::vector<std::vector<uint8_t> > state_keys_;
  bool first_boot_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerBackedStateKeyGeneratorTest);
};

TEST_F(ServerBackedStateKeyGeneratorTest, RequestStateKeys) {
  InitMachineInfo(false);
  RequestStateKeys(true);
  EXPECT_EQ(ServerBackedStateKeyGenerator::kDeviceStateKeyFutureQuanta,
            state_keys_.size());
  EXPECT_FALSE(first_boot_);
}

TEST_F(ServerBackedStateKeyGeneratorTest, TimedStateKeys) {
  InitMachineInfo(false);
  system_utils_.forward_time(base::TimeDelta::FromDays(100).InSeconds());

  // The correct number of state keys gets returned.
  RequestStateKeys(true);
  ASSERT_EQ(ServerBackedStateKeyGenerator::kDeviceStateKeyFutureQuanta,
            state_keys_.size());
  std::vector<std::vector<uint8_t> > initial_state_keys = state_keys_;

  // All state keys are different.
  std::set<std::vector<uint8_t> > state_key_set(state_keys_.begin(),
                                                state_keys_.end());
  EXPECT_EQ(ServerBackedStateKeyGenerator::kDeviceStateKeyFutureQuanta,
            state_key_set.size());

  // Moving forward just a little yields the same keys.
  system_utils_.forward_time(base::TimeDelta::FromDays(1).InSeconds());
  RequestStateKeys(true);
  EXPECT_EQ(initial_state_keys, state_keys_);

  // Jumping to a future quantum results in the state keys rolling forward.
  int64_t step =
      1 << ServerBackedStateKeyGenerator::kDeviceStateKeyTimeQuantumPower;
  system_utils_.forward_time(2 * step);

  RequestStateKeys(true);
  ASSERT_EQ(ServerBackedStateKeyGenerator::kDeviceStateKeyFutureQuanta,
            state_keys_.size());
  EXPECT_TRUE(std::equal(initial_state_keys.begin() + 2,
                         initial_state_keys.end(),
                         state_keys_.begin()));
}

TEST_F(ServerBackedStateKeyGeneratorTest, PendingMachineInfo) {
  // No callback as long as machine info has not been provided.
  RequestStateKeys(false);

  // Supplying machine info fires callbacks.
  InitMachineInfo(false);
  EXPECT_TRUE(state_keys_received_);
  EXPECT_EQ(ServerBackedStateKeyGenerator::kDeviceStateKeyFutureQuanta,
            state_keys_.size());
}

TEST_F(ServerBackedStateKeyGeneratorTest, PendingMachineInfoFailure) {
  // No callback as long as machine info has not been provided.
  RequestStateKeys(false);

  // Supplying machine info fires callbacks even if info is missing.
  std::map<std::string, std::string> empty;
  EXPECT_FALSE(generator_.InitMachineInfo(empty));
  EXPECT_TRUE(state_keys_received_);
  EXPECT_EQ(0, state_keys_.size());

  // Later requests get answered immediately.
  RequestStateKeys(true);
  EXPECT_EQ(0, state_keys_.size());
}

TEST_F(ServerBackedStateKeyGeneratorTest, ParseMachineInfoSuccess) {
  std::map<std::string, std::string> params;
  EXPECT_TRUE(ServerBackedStateKeyGenerator::ParseMachineInfo(
      "\"serial_number\"=\"fake-machine-serial-number\"\n"
      "# This is a comment.\n"
      "root_disk_serial_number=fake-disk-serial-number\n",
      &params));
  EXPECT_EQ(2, params.size());
  EXPECT_EQ("fake-machine-serial-number", params["serial_number"]);
  EXPECT_EQ("fake-disk-serial-number", params["root_disk_serial_number"]);
}

TEST_F(ServerBackedStateKeyGeneratorTest, ParseMachineInfoFailure) {
  std::map<std::string, std::string> params;
  EXPECT_FALSE(
      ServerBackedStateKeyGenerator::ParseMachineInfo("bad!", &params));
}

TEST_F(ServerBackedStateKeyGeneratorTest, FirstBoot) {
  InitMachineInfo(true);
  RequestStateKeys(true);
  EXPECT_TRUE(first_boot_);
}

}  // namespace login_manager
