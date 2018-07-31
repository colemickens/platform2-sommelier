// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "oobe_config/oobe_config.h"

namespace oobe_config {

TEST(OobeConfigTest, DummyTest) {
  EXPECT_EQ("OOBE Config", Hello());
}

}  // namespace oobe_config
