// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_operator_info.h"

#include <base/file_util.h>
#include <gtest/gtest.h>

using std::string;

namespace shill {

namespace {

const char kTestInfoFileContent[] =
    "[000001]\n"
    "OLP.URL=https://testurl\n"
    "OLP.Method=POST\n"
    "OLP.PostData=imei=${imei}&iccid=${iccid}\n"
    "\n"
    "[000002]\n"
    "OLP.URL=https://testurl2\n"
    "\n"
    "[000003]\n"
    "Name=test\n";

}  // namespace

class CellularOperatorInfoTest : public testing::Test {
 public:
  CellularOperatorInfoTest() : info_(&glib_) {}

 protected:
  void SetUp() {
    ASSERT_TRUE(file_util::CreateTemporaryFile(&info_file_path_));
    ASSERT_EQ(arraysize(kTestInfoFileContent),
              file_util::WriteFile(info_file_path_, kTestInfoFileContent,
                                   arraysize(kTestInfoFileContent)));
  }

  void TearDown() {
    ASSERT_TRUE(file_util::Delete(info_file_path_, false));
  }

  GLib glib_;
  FilePath info_file_path_;
  CellularOperatorInfo info_;
};

TEST_F(CellularOperatorInfoTest, GetOLP) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  CellularService::OLP olp;
  EXPECT_TRUE(info_.GetOLP("000001", &olp));
  EXPECT_EQ("https://testurl", olp.GetURL());
  EXPECT_EQ("POST", olp.GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}", olp.GetPostData());

  EXPECT_TRUE(info_.GetOLP("000002", &olp));
  EXPECT_EQ("https://testurl2", olp.GetURL());
  EXPECT_EQ("", olp.GetMethod());
  EXPECT_EQ("", olp.GetPostData());

  EXPECT_FALSE(info_.GetOLP("000003", &olp));
  EXPECT_FALSE(info_.GetOLP("000004", &olp));
}

}  // namespace shill
