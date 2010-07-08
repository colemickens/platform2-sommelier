// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <base/scoped_ptr.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "configuration.h"

using ::testing::StrEq;


TEST(ConfigurationTest, FileError) {
  // Cannot be read or written
  const char nonexistant[] = "/nonexistant/nonexistant";

  Configuration c;
  bool rc;
  rc = c.Init(nonexistant);
  EXPECT_FALSE(rc);
  std::string value;

  value = c.GetValueString(Configuration::kCarrierKey);
  EXPECT_THAT(value, StrEq(""));

  // Can still set and read the in-memory copy
  c.SetValueString(Configuration::kCarrierKey, "rhinoceros");
  value = c.GetValueString(Configuration::kCarrierKey);
  EXPECT_THAT(value, StrEq("rhinoceros"));

  // But a new configuration doesn't reflect in-memory change
  Configuration new_from_disk;
  rc = new_from_disk.Init(nonexistant);
  EXPECT_FALSE(rc);
  value = new_from_disk.GetValueString(Configuration::kCarrierKey);
  EXPECT_THAT(value, StrEq(""));
}

TEST(ConfigurationTest, Simple) {
  Configuration c;
  bool rc;
  std::string value;
  const char filename[] = "/tmp/configuration_unittest";

  int unlink_rc = unlink(filename);
  if (unlink_rc != 0 && errno != ENOENT) {
    PLOG(FATAL) << "Could not unlink: " << filename;
  }

  rc = c.Init(filename);
  EXPECT_FALSE(rc);

  value = c.GetValueString(Configuration::kCarrierKey);
  EXPECT_THAT(value,
              StrEq(""));

  c.SetValueString(Configuration::kCarrierKey, "fictitious");
  value = c.GetValueString(Configuration::kCarrierKey);
  EXPECT_THAT(value,
              StrEq("fictitious"));


  Configuration new_from_disk;
  rc = new_from_disk.Init(filename);
  EXPECT_TRUE(rc);

  value = new_from_disk.GetValueString(Configuration::kCarrierKey);
  EXPECT_THAT(value, StrEq("fictitious"));
}

TEST(ConfigurationDeathTest, BadKey) {
  Configuration c;
  c.Init("/nonexistant/nonexistant");

  EXPECT_DEATH(c.SetValueString("bogus", "bogus"),
               "Invalid key to SetValueString");

  EXPECT_DEATH(c.GetValueString("bogus"),
               "Invalid key to GetValueString");
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  google::ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}
