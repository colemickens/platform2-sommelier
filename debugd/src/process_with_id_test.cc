// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "process_with_id.h"

using debugd::ProcessWithId;

TEST(ProcessWithId, GeneratedId) {
  ProcessWithId p0;
  ProcessWithId p1;
  EXPECT_EQ(p0.Init(), true);
  EXPECT_EQ(p1.Init(), true);
  EXPECT_NE(p0.id(), p1.id());
  EXPECT_GT(p0.id().length(), 0);
  EXPECT_GT(p1.id().length(), 0);
}
