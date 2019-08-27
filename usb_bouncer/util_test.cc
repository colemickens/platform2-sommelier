// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>

#include "usb_bouncer/util.h"

namespace usb_bouncer {

namespace {

constexpr char kSubdirName[] = "subdir";
constexpr char kSysFSAuthorized[] = "authorized";
constexpr char kSysFSAuthorizedDefault[] = "authorized_default";
constexpr char kSysFSEnabled[] = "1";
constexpr char kSysFSDisabled[] = "0";

bool CreateDisabledFlag(const base::FilePath& flag) {
  if (!base::WriteFile(flag, kSysFSDisabled, sizeof(kSysFSDisabled) - 1)) {
    LOG(ERROR) << "WriteFile('" << flag.value() << "', ...) failed.";
    return false;
  }
  return true;
}

bool CreateDeviceNode(const base::FilePath& dir) {
  if (!base::CreateDirectory(dir)) {
    LOG(ERROR) << "CreateDirectory('" << dir.value() << "') failed.";
    return false;
  }

  return CreateDisabledFlag(dir.Append(kSysFSAuthorized)) &&
         CreateDisabledFlag(dir.Append(kSysFSAuthorizedDefault));
}

bool CheckEnabledFlag(const base::FilePath& flag) {
  std::string flag_content;
  if (!base::ReadFileToString(flag, &flag_content)) {
    LOG(ERROR) << "ReadFileToString('" << flag.value() << "', ...) failed.";
    return false;
  }
  return flag_content == kSysFSEnabled;
}

bool CheckDeviceNodeAuthorized(const base::FilePath& dir) {
  return CheckEnabledFlag(dir.Append(kSysFSAuthorized)) &&
         CheckEnabledFlag(dir.Append(kSysFSAuthorizedDefault));
}

}  // namespace

TEST(UtilTest, IncludeRuleAtLockscreen) {
  EXPECT_FALSE(IncludeRuleAtLockscreen(""));

  const std::string blocked_device =
      "allow id 0781:5588 serial \"12345678BF05\" name \"USB Extreme Pro\" "
      "hash \"9hMkYEMPjuNegGmzLIKwUp2MPctSL0tCWk7ruWGuOzc=\" with-interface "
      "08:06:50 with-connect-type \"unknown\"";
  EXPECT_FALSE(IncludeRuleAtLockscreen(blocked_device))
      << "Failed to filter: " << blocked_device;

  const std::string allowed_device =
      "allow id 0bda:8153 serial \"000001000000\" name \"USB 10/100/1000 LAN\" "
      "hash \"dljXy8thtljhoJo+O+hfhSlp1J89rz0Z4404iqKzakI=\" with-interface "
      "{ ff:ff:00 02:06:00 0a:00:00 0a:00:00 }";
  EXPECT_TRUE(IncludeRuleAtLockscreen(allowed_device))
      << "Failed to allow:" << allowed_device;
}

TEST(UtilTest, AuthorizeAll_Success) {
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()) << strerror(errno);
  ASSERT_TRUE(AuthorizeAll(temp_dir_.GetPath().value()));

  const base::FilePath subdir = temp_dir_.GetPath().Append(kSubdirName);
  ASSERT_TRUE(CreateDeviceNode(subdir))
      << "CreateDeviceNode('" << subdir.value() << "') failed.";
  const base::FilePath deepdir =
      temp_dir_.GetPath().Append(kSubdirName).Append(kSubdirName);
  ASSERT_TRUE(CreateDeviceNode(deepdir))
      << "CreateDeviceNode('" << deepdir.value() << "') failed.";

  EXPECT_TRUE(AuthorizeAll(temp_dir_.GetPath().value()));
  EXPECT_TRUE(CheckDeviceNodeAuthorized(subdir));
  EXPECT_TRUE(CheckDeviceNodeAuthorized(deepdir));
}

TEST(UtilTest, AuthorizeAll_Failure) {
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()) << strerror(errno);

  const base::FilePath subdir = temp_dir_.GetPath().Append(kSubdirName);
  ASSERT_TRUE(CreateDeviceNode(subdir))
      << "CreateDeviceNode('" << subdir.value() << "') failed.";
  const base::FilePath deepdir =
      temp_dir_.GetPath().Append(kSubdirName).Append(kSubdirName);
  ASSERT_TRUE(CreateDeviceNode(deepdir))
      << "CreateDeviceNode('" << deepdir.value() << "') failed.";

  chmod(subdir.Append(kSysFSAuthorizedDefault).value().c_str(), 0000);

  EXPECT_FALSE(AuthorizeAll(temp_dir_.GetPath().value()));
  EXPECT_FALSE(CheckDeviceNodeAuthorized(subdir));
  EXPECT_TRUE(CheckDeviceNodeAuthorized(deepdir));
}

}  // namespace usb_bouncer
