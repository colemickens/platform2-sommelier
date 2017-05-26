// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/macros.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <authpolicy/authpolicy_flags.h>

namespace authpolicy {

#if 0
// https://bugs.chromium.org/p/chromium/issues/detail?id=726757

class AuthPolicyFlagsTest : public ::testing::Test {
 public:
  AuthPolicyFlagsTest() {}
  ~AuthPolicyFlagsTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthPolicyFlagsTest);
};

// By default, all debug flags should be off.
TEST_F(AuthPolicyFlagsTest, TestAllFlagsOff) {
  AuthPolicyFlags flags_container;
  const protos::DebugFlags& flags = flags_container.Get();

  EXPECT_FALSE(flags.disable_seccomp());
  EXPECT_FALSE(flags.log_seccomp());
  EXPECT_FALSE(flags.trace_kinit());
  EXPECT_FALSE(flags.log_policy_values());
  EXPECT_FALSE(flags.log_commands());
  EXPECT_FALSE(flags.log_command_output());
  EXPECT_FALSE(flags.log_command_output_on_error());
  EXPECT_FALSE(flags.log_gpo());
  EXPECT_FALSE(flags.log_config());
  EXPECT_EQ("0", flags.net_log_level());
}

// Check whether parsing the flags data works as expected.
TEST_F(AuthPolicyFlagsTest, TestAllFlagsOn) {
  AuthPolicyFlags flags_container;
  flags_container.LoadFromJsonString(R"!!!(
    { "disable_seccomp":true,
      "log_seccomp":true,
      "trace_kinit":true,
      "log_policy_values":true,
      "log_commands":true,
      "log_command_output":true,
      "log_command_output_on_error":true,
      "log_gpo":true,
      "log_config":true,
      "net_log_level":"10" })!!!");
  const protos::DebugFlags& flags = flags_container.Get();

  EXPECT_TRUE(flags.disable_seccomp());
  EXPECT_TRUE(flags.log_seccomp());
  EXPECT_TRUE(flags.trace_kinit());
  EXPECT_TRUE(flags.log_policy_values());
  EXPECT_TRUE(flags.log_commands());
  EXPECT_TRUE(flags.log_command_output());
  EXPECT_TRUE(flags.log_command_output_on_error());
  EXPECT_TRUE(flags.log_gpo());
  EXPECT_TRUE(flags.log_config());
  EXPECT_EQ("10", flags.net_log_level());
}

TEST_F(AuthPolicyFlagsTest, FlagsSerialization) {
  protos::DebugFlags flags;
  flags.set_log_config(true);
  flags.set_net_log_level("5");
  std::string flags_encoded = SerializeFlags(flags);
  EXPECT_FALSE(flags_encoded.empty());
  protos::DebugFlags flags2;
  EXPECT_TRUE(DeserializeFlags(flags_encoded, &flags2));

  // Same as EXPECT_EQ(flags, flags2), except that that's not supported.
  std::string flags_str, flags_str2;
  flags.SerializeToString(&flags_str);
  flags2.SerializeToString(&flags_str2);
  EXPECT_EQ(flags_str, flags_str2);
}

TEST_F(AuthPolicyFlagsTest, FlagsDeserializationFailsBadString) {
  protos::DebugFlags flags;
  EXPECT_FALSE(DeserializeFlags("!@#$%bogus", &flags));
}
#endif

}  // namespace authpolicy
