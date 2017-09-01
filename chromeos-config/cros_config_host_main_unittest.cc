// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for cros_config_host_main command-line utility.

#include <libgen.h>

#include <cstdio>
#include <string>
#include <vector>

#include <base/process/launch.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

namespace {

// The base (start of) all commands using the cros_config_host cli.
const char* base_command = nullptr;

};  // namespace

TEST(CrosConfigTest, MissingModelFlag) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "test.dtb", "/", "wallpaper"}, &output);
  EXPECT_FALSE(success);
}

TEST(CrosConfigTest, MissingPathAndKey) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--model=pyro", "test.dtb"}, &output);
  EXPECT_FALSE(success);
}

TEST(CrosConfigTest, MissingKey) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--model=pyro", "test.dtb", "/"}, &output);
  EXPECT_FALSE(success);
}

TEST(CrosConfigTest, FileDoesntExist) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--config_file=nope.dtb", "--list_models"}, &output);
  EXPECT_FALSE(success);
}

TEST(CrosConfigTest, GetStringRoot) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--model=pyro", "test.dtb", "/", "wallpaper"}, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ("default", output);
}

TEST(CrosConfigTest, GetStringNonRoot) {
  std::string output;
  bool success = base::GetAppOutput({base_command, "--model=pyro", "test.dtb",
      "/firmware", "bcs-overlay"}, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ("overlay-reef-private", output);
}

TEST(CrosConfigTest, ListModels) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--model=pyro", "--list_models", "test.dtb"}, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ("pyro\ncaroline\nreef\nbroken\nwhitetip\nwhitetip1\nwhitetip2\n",
            output);
}

TEST(CrosConfigTest, GetStringForAllMissing) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--get_all", "test.dtb", "/", "does_not_exist"}, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ("\n\n\n\n\n\n\n", output);
}

TEST(CrosConfigTest, GetStringForAll) {
  std::string output;
  bool success = base::GetAppOutput(
      {base_command, "--get_all", "test.dtb", "/", "wallpaper"}, &output);
  EXPECT_TRUE(success);
  EXPECT_EQ("default\n\nepic\n\n\nshark\nmore_shark\n", output);
}

TEST(CrosConfigTest, StdinGetString) {
  std::string command = "cat test.dtb | " + std::string(base_command) +
    " --model=pyro - / wallpaper";
  std::string output;
  bool success = base::GetAppOutput({"/bin/bash", "-c", command}, &output);
  EXPECT_EQ("default", output);
  EXPECT_TRUE(success);
}

TEST(CrosConfigTest, StdinListModels) {
  std::string command = "cat test.dtb | " + std::string(base_command) +
    " --list_models -";
  std::string output;
  bool success = base::GetAppOutput({"/bin/bash", "-c", command}, &output);
  EXPECT_EQ("pyro\ncaroline\nreef\nbroken\nwhitetip\nwhitetip1\nwhitetip2\n",
            output);
  EXPECT_TRUE(success);
}

int main(int argc, char** argv) {
  int status = system("exec ./chromeos-config-test-setup.sh");
  if (status != 0) {
    return EXIT_FAILURE;
  }
  testing::InitGoogleTest(&argc, argv);

  char* installed_dir = dirname(argv[0]);
  if (!installed_dir) {
    return EXIT_FAILURE;
  }

  std::string base_command_str =
    base::StringPrintf("%s/cros_config_host", installed_dir);
  base_command = base_command_str.c_str();

  return RUN_ALL_TESTS();
}
