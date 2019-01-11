// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

#include <base/files/file_util.h>
#include <gtest/gtest.h>

namespace dlcservice {

namespace {
constexpr char kDlcRootPath[] = "/tmp/dlc/";
constexpr char kDlcId[] = "id";
}  // namespace

class UtilsTest : public testing::Test {};

TEST(UtilsTest, GetDlcModulePathTest) {
  EXPECT_EQ(
      utils::GetDlcModulePath(base::FilePath(kDlcRootPath), kDlcId).value(),
      "/tmp/dlc/id");
}

TEST(UtilsTest, GetDlcModuleImagePathBadSlotTest) {
  // IF current_slot is negative, THEN GetDlcModuleImagePath() returns an empty
  // path.
  EXPECT_TRUE(
      utils::GetDlcModuleImagePath(base::FilePath(kDlcRootPath), kDlcId, -1)
          .value()
          .empty());
}

TEST(UtilsTest, GetDlcModuleImagePathA) {
  EXPECT_EQ(
      utils::GetDlcModuleImagePath(base::FilePath(kDlcRootPath), kDlcId, 0)
          .value(),
      "/tmp/dlc/id/dlc_a/dlc.img");
}

TEST(UtilsTest, GetDlcModuleImagePathB) {
  EXPECT_EQ(
      utils::GetDlcModuleImagePath(base::FilePath(kDlcRootPath), kDlcId, 1)
          .value(),
      "/tmp/dlc/id/dlc_b/dlc.img");
}

TEST(UtilsTest, GetDlcRootInModulePath) {
  base::FilePath path("foo-path");
  base::FilePath expected_path("foo-path/root");
  EXPECT_EQ(utils::GetDlcRootInModulePath(path), expected_path);
}

}  // namespace dlcservice
