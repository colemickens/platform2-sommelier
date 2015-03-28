// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/group_config.h"

#include <gtest/gtest.h>

namespace leaderd {

class GroupConfigTest : public testing::Test {
 public:
  GroupConfig config_;
};

TEST_F(GroupConfigTest, AcceptsEmptyOptions) {
  std::map<std::string, chromeos::Any> options;
  chromeos::ErrorPtr error;
  EXPECT_TRUE(config_.Load(options, &error));
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(GroupConfigTest, RejectsExtraFields) {
  std::map<std::string, chromeos::Any> options{
    {"not-a-real-field", 100},
  };
  chromeos::ErrorPtr error;
  EXPECT_FALSE(config_.Load(options, &error));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(GroupConfigTest, ParsesKnownOptions) {
  std::map<std::string, chromeos::Any> options{
    {group_options::kMinWandererTimeoutSeconds, 100},
    {group_options::kMaxWandererTimeoutSeconds, 200},
    {group_options::kLeaderSteadyStateTimeoutSeconds, 300},
    {group_options::kIsPersistent, false},
  };
  chromeos::ErrorPtr error;
  EXPECT_TRUE(config_.Load(options, &error));
  EXPECT_EQ(nullptr, error.get());
}

TEST_F(GroupConfigTest, RejectsBadTypes) {
  std::map<std::string, chromeos::Any> options{
    {group_options::kMinWandererTimeoutSeconds, std::string{"boo"}},
  };
  chromeos::ErrorPtr error;
  EXPECT_FALSE(config_.Load(options, &error));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(GroupConfigTest, EnsuresWandererMinLessThanMax) {
  std::map<std::string, chromeos::Any> options{
    {group_options::kMinWandererTimeoutSeconds, 201},
    {group_options::kMaxWandererTimeoutSeconds, 200},
  };
  chromeos::ErrorPtr error;
  EXPECT_FALSE(config_.Load(options, &error));
  EXPECT_NE(nullptr, error.get());
}

TEST_F(GroupConfigTest, HandlesMinWandererTimeoutEqualsMax) {
  int32_t kWandererTimeout = 200;
  std::map<std::string, chromeos::Any> options{
    {group_options::kMinWandererTimeoutSeconds, kWandererTimeout},
    {group_options::kMaxWandererTimeoutSeconds, kWandererTimeout},
  };
  chromeos::ErrorPtr error;
  EXPECT_TRUE(config_.Load(options, &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(config_.PickWandererTimeoutMs(), kWandererTimeout);
}

}  // namespace leaderd
