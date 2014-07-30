// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/sandboxed_process.h"

#include <gtest/gtest.h>

namespace debugd {

TEST(SandboxedProcessTest, GetHelperPath) {
  std::string full_path;

  // No $DEBUGD_HELPERS is defined
  EXPECT_TRUE(SandboxedProcess::GetHelperPath("", &full_path));
  EXPECT_EQ("/usr/libexec/debugd/helpers/", full_path);

  EXPECT_TRUE(SandboxedProcess::GetHelperPath("test/me", &full_path));
  EXPECT_EQ("/usr/libexec/debugd/helpers/test/me", full_path);

  // $DEBUGD_HELPERS is set to /tmp
  setenv("DEBUGD_HELPERS", "/tmp", 1);
  EXPECT_TRUE(SandboxedProcess::GetHelperPath("", &full_path));
  EXPECT_EQ("/tmp/", full_path);

  EXPECT_TRUE(SandboxedProcess::GetHelperPath("test/me", &full_path));
  EXPECT_EQ("/tmp/test/me", full_path);

  // The full path exceeds the PATH_MAX limit
  EXPECT_FALSE(SandboxedProcess::GetHelperPath(
      std::string(PATH_MAX - strlen("/tmp"), 'a'), &full_path));
  EXPECT_EQ("/tmp/test/me", full_path);  // |full_path| remains unchanged
}

}  // namespace debugd
