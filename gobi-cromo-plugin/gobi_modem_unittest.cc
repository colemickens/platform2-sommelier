// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"

#include <gtest/gtest.h>


class GobiModemTest : public ::testing::Test {



};

TEST_F(GobiModemTest, GetSignalQuality) {
  EXPECT_TRUE(true);
}



int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
