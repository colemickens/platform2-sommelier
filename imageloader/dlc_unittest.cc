// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader/dlc.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "imageloader/manifest.h"
#include "imageloader/mock_helper_process_proxy.h"
#include "imageloader/test_utilities.h"

namespace imageloader {

TEST(DlcTest, MountDlc) {
  base::FilePath metadata_path = GetTestDataPath("example_dlc");
  base::FilePath image_path = metadata_path.Append("dlc.img");
  base::FilePath manifest_path = metadata_path.Append("imageloader.json");
  base::FilePath table_path = metadata_path.Append("table");

  auto proxy = std::make_unique<MockHelperProcessProxy>();
  EXPECT_CALL(*proxy, SendMountCommand(testing::_, testing::_,
                                       FileSystem::kExt4, testing::_))
      .Times(1);
  ON_CALL(*proxy,
          SendMountCommand(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Return(true));

  Dlc dlc("exampledlcid");
  EXPECT_TRUE(dlc.Mount(proxy.get(), image_path, manifest_path, table_path,
                        AOrB::kDlcA, base::FilePath()));
}

TEST(DlcTest, IsIdValid) {
  // alpha numerical IDs:
  EXPECT_TRUE(Dlc::IsIdValid("alpha"));
  EXPECT_TRUE(Dlc::IsIdValid("01234"));
  EXPECT_TRUE(Dlc::IsIdValid("alphanum01234"));
  EXPECT_TRUE(Dlc::IsIdValid("01234alphanumerical"));
  EXPECT_TRUE(Dlc::IsIdValid("dash-id0123"));
  EXPECT_TRUE(Dlc::IsIdValid("underscore_id_0123"));
  EXPECT_TRUE(Dlc::IsIdValid("0123-a_dash-id"));
  EXPECT_TRUE(Dlc::IsIdValid(u8"unicode_id"));
  // first char is illegal:
  EXPECT_FALSE(Dlc::IsIdValid("-non-alpha"));
  EXPECT_FALSE(Dlc::IsIdValid("_non-alpha"));
  EXPECT_FALSE(Dlc::IsIdValid(".non-alpha"));
  // non-alpha numerical IDs:
  EXPECT_FALSE(Dlc::IsIdValid("dot.id"));
  EXPECT_FALSE(Dlc::IsIdValid("../../../../evilid"));
  EXPECT_FALSE(Dlc::IsIdValid(u8"unicode_id_#"));
  // ID is too long.
  EXPECT_FALSE(Dlc::IsIdValid("id-is-too-looooooooong"));
}

}  // namespace imageloader
