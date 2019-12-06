// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

#include <base/bind.h>
#include <base/files/file_util.h>
#include <gtest/gtest.h>

namespace dlcservice {

namespace {
constexpr char kDlcRootPath[] = "/tmp/dlc/";
constexpr char kDlcId[] = "id";
constexpr char kDlcPackage[] = "package";
}  // namespace

class UtilsTest : public testing::Test {};

TEST(UtilsTest, GetDlcPathTest) {
  EXPECT_EQ(utils::GetDlcPath(base::FilePath(kDlcRootPath), kDlcId).value(),
            "/tmp/dlc/id");
}

TEST(UtilsTest, GetDlcPackagePathTest) {
  EXPECT_EQ(utils::GetDlcPackagePath(base::FilePath(kDlcRootPath), kDlcId,
                                     kDlcPackage)
                .value(),
            "/tmp/dlc/id/package");
}

TEST(UtilsTest, GetDlcModuleImagePathA) {
  EXPECT_EQ(utils::GetDlcImagePath(base::FilePath(kDlcRootPath), kDlcId,
                                   kDlcPackage, BootSlot::Slot::A)
                .value(),
            "/tmp/dlc/id/package/dlc_a/dlc.img");
}

TEST(UtilsTest, GetDlcModuleImagePathB) {
  EXPECT_EQ(utils::GetDlcImagePath(base::FilePath(kDlcRootPath), kDlcId,
                                   kDlcPackage, BootSlot::Slot::B)
                .value(),
            "/tmp/dlc/id/package/dlc_b/dlc.img");
}

TEST(UtilsTest, GetDlcRootInModulePath) {
  base::FilePath path("foo-path");
  base::FilePath expected_path("foo-path/root");
  EXPECT_EQ(utils::GetDlcRootInModulePath(path), expected_path);
}

TEST(UtilsTest, ScopedCleanupsTest) {
  bool flag = false;
  base::Callback<void()> cleanup =
      base::Bind([](bool* flag_ptr) { *flag_ptr = true; }, &flag);

  {
    utils::ScopedCleanups<decltype(cleanup)> scoped_cleanups;
    scoped_cleanups.Insert(cleanup);
  }
  EXPECT_TRUE(flag);

  flag = false;
  {
    utils::ScopedCleanups<decltype(cleanup)> scoped_cleanups;
    scoped_cleanups.Insert(cleanup);
    scoped_cleanups.Cancel();
  }
  EXPECT_FALSE(flag);
}

}  // namespace dlcservice
