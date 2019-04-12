// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/util.h"

#include <stdlib.h>

#include <memory>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "crash-reporter/crash_sender_paths.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/test_util.h"

namespace util {
namespace {

const char kLsbReleaseContents[] =
    "CHROMEOS_RELEASE_BOARD=bob\n"
    "CHROMEOS_RELEASE_NAME=Chromium OS\n"
    "CHROMEOS_RELEASE_VERSION=10964.0.2018_08_13_1405\n";

constexpr char kHwClassContents[] = "fake_hwclass";

}  // namespace

class CrashCommonUtilTest : public testing::Test {
 private:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();
    paths::SetPrefixForTesting(test_dir_);
  }

  void TearDown() override { paths::SetPrefixForTesting(base::FilePath()); }

  base::FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(CrashCommonUtilTest, IsCrashTestInProgress) {
  EXPECT_FALSE(IsCrashTestInProgress());
  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                         paths::kCrashTestInProgress),
                            ""));
  EXPECT_TRUE(IsCrashTestInProgress());
}

TEST_F(CrashCommonUtilTest, IsDeviceCoredumpUploadAllowed) {
  EXPECT_FALSE(IsDeviceCoredumpUploadAllowed());
  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kCrashReporterStateDirectory,
                                         paths::kDeviceCoredumpUploadAllowed),
                            ""));
  EXPECT_TRUE(IsDeviceCoredumpUploadAllowed());
}

TEST_F(CrashCommonUtilTest, IsDeveloperImage) {
  EXPECT_FALSE(IsDeveloperImage());

  ASSERT_TRUE(test_util::CreateFile(paths::Get(paths::kLeaveCoreFile), ""));
  EXPECT_TRUE(IsDeveloperImage());

  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                         paths::kCrashTestInProgress),
                            ""));
  EXPECT_FALSE(IsDeveloperImage());
}

TEST_F(CrashCommonUtilTest, IsTestImage) {
  EXPECT_FALSE(IsTestImage());

  // Should return false because the channel is stable.
  ASSERT_TRUE(test_util::CreateFile(
      paths::GetAt(paths::kEtcDirectory, paths::kLsbRelease),
      "CHROMEOS_RELEASE_TRACK=stable-channel"));
  EXPECT_FALSE(IsTestImage());

  // Should return true because the channel is testimage.
  ASSERT_TRUE(test_util::CreateFile(
      paths::GetAt(paths::kEtcDirectory, paths::kLsbRelease),
      "CHROMEOS_RELEASE_TRACK=testimage-channel"));
  EXPECT_TRUE(IsTestImage());

  // Should return false if kCrashTestInProgress is present.
  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                         paths::kCrashTestInProgress),
                            ""));
  EXPECT_FALSE(IsTestImage());
}

TEST_F(CrashCommonUtilTest, IsForceOfficialSet) {
  EXPECT_FALSE(IsForceOfficialSet());

  // Check if FORCE_OFFICIAL is handled correctly.
  setenv("FORCE_OFFICIAL", "0", 1 /* overwrite */);
  EXPECT_FALSE(IsForceOfficialSet());

  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  EXPECT_TRUE(IsForceOfficialSet());
  unsetenv("FORCE_OFFICIAL");
}

TEST_F(CrashCommonUtilTest, IsOfficialImage) {
  EXPECT_FALSE(IsOfficialImage());

  // Check if FORCE_OFFICIAL is handled correctly.
  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  EXPECT_TRUE(IsOfficialImage());
  unsetenv("FORCE_OFFICIAL");

  // Check if lsb-release is handled correctly.
  ASSERT_TRUE(test_util::CreateFile(
      paths::Get("/etc/lsb-release"),
      "CHROMEOS_RELEASE_DESCRIPTION=10964.0 (Test Build) developer-build"));
  EXPECT_FALSE(IsOfficialImage());

  ASSERT_TRUE(test_util::CreateFile(
      paths::Get("/etc/lsb-release"),
      "CHROMEOS_RELEASE_DESCRIPTION=10964.0 (Official Build) canary-channel"));
  EXPECT_TRUE(IsOfficialImage());
}

TEST_F(CrashCommonUtilTest, GetHardwareClass) {
  EXPECT_EQ("undefined", GetHardwareClass());

  ASSERT_TRUE(test_util::CreateFile(
      paths::Get("/sys/devices/platform/chromeos_acpi/HWID"),
      kHwClassContents));
  EXPECT_EQ(kHwClassContents, GetHardwareClass());
}

TEST_F(CrashCommonUtilTest, GetBootModeString) {
  EXPECT_EQ("missing-crossystem", GetBootModeString());

  // Check if MOCK_DEVELOPER_MODE is handled correctly.
  setenv("MOCK_DEVELOPER_MODE", "0", 1 /* overwrite */);
  EXPECT_EQ("missing-crossystem", GetBootModeString());

  setenv("MOCK_DEVELOPER_MODE", "1", 1 /* overwrite */);
  EXPECT_EQ("dev", GetBootModeString());
  unsetenv("MOCK_DEVELOPER_MODE");

  ASSERT_TRUE(
      test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                         paths::kCrashTestInProgress),
                            ""));
  EXPECT_EQ("", GetBootModeString());
}

TEST_F(CrashCommonUtilTest, GetCachedKeyValue) {
  ASSERT_TRUE(test_util::CreateFile(paths::Get("/etc/lsb-release"),
                                    kLsbReleaseContents));
  ASSERT_TRUE(test_util::CreateFile(paths::Get("/empty/lsb-release"), ""));

  std::string value;
  // No directories are specified.
  EXPECT_FALSE(GetCachedKeyValue(base::FilePath("lsb-release"),
                                 "CHROMEOS_RELEASE_VERSION", {}, &value));
  // A non-existent directory is specified.
  EXPECT_FALSE(GetCachedKeyValue(base::FilePath("lsb-release"),
                                 "CHROMEOS_RELEASE_VERSION",
                                 {paths::Get("/non-existent")}, &value));

  // A non-existent base name is specified.
  EXPECT_FALSE(GetCachedKeyValue(base::FilePath("non-existent"),
                                 "CHROMEOS_RELEASE_VERSION",
                                 {paths::Get("/etc")}, &value));

  // A wrong key is specified.
  EXPECT_FALSE(GetCachedKeyValue(base::FilePath("lsb-release"), "WRONG_KEY",
                                 {paths::Get("/etc")}, &value));

  // This should succeed.
  EXPECT_TRUE(GetCachedKeyValue(base::FilePath("lsb-release"),
                                "CHROMEOS_RELEASE_VERSION",
                                {paths::Get("/etc")}, &value));
  EXPECT_EQ("10964.0.2018_08_13_1405", value);

  // A non-existent directory is included, but this should still succeed.
  EXPECT_TRUE(GetCachedKeyValue(
      base::FilePath("lsb-release"), "CHROMEOS_RELEASE_VERSION",
      {paths::Get("/non-existent"), paths::Get("/etc")}, &value));
  EXPECT_EQ("10964.0.2018_08_13_1405", value);

  // A empty file is included, but this should still succeed.
  EXPECT_TRUE(GetCachedKeyValue(
      base::FilePath("lsb-release"), "CHROMEOS_RELEASE_VERSION",
      {paths::Get("/empty"), paths::Get("/etc")}, &value));
  EXPECT_EQ("10964.0.2018_08_13_1405", value);
}

TEST_F(CrashCommonUtilTest, GetCachedKeyValueDefault) {
  std::string value;
  EXPECT_FALSE(
      GetCachedKeyValueDefault(base::FilePath("test.txt"), "FOO", &value));

  // kEtcDirectory is the second candidate directory.
  ASSERT_TRUE(test_util::CreateFile(
      paths::GetAt(paths::kEtcDirectory, "test.txt"), "FOO=2\n"));
  EXPECT_TRUE(
      GetCachedKeyValueDefault(base::FilePath("test.txt"), "FOO", &value));
  EXPECT_EQ("2", value);

  // kCrashReporterStateDirectory is the first candidate directory.
  ASSERT_TRUE(test_util::CreateFile(
      paths::GetAt(paths::kCrashReporterStateDirectory, "test.txt"),
      "FOO=1\n"));
  EXPECT_TRUE(
      GetCachedKeyValueDefault(base::FilePath("test.txt"), "FOO", &value));
  EXPECT_EQ("1", value);
}

TEST_F(CrashCommonUtilTest, GetUserCrashDirectories) {
  auto mock =
      std::make_unique<org::chromium::SessionManagerInterfaceProxyMock>();

  std::vector<base::FilePath> directories;

  test_util::SetActiveSessions(mock.get(), {});
  EXPECT_TRUE(GetUserCrashDirectories(mock.get(), &directories));
  EXPECT_TRUE(directories.empty());

  test_util::SetActiveSessions(mock.get(),
                               {{"user1", "hash1"}, {"user2", "hash2"}});
  EXPECT_TRUE(GetUserCrashDirectories(mock.get(), &directories));
  EXPECT_EQ(2, directories.size());
  EXPECT_EQ(paths::Get("/home/user/hash1/crash").value(),
            directories[0].value());
  EXPECT_EQ(paths::Get("/home/user/hash2/crash").value(),
            directories[1].value());
}

}  // namespace util
