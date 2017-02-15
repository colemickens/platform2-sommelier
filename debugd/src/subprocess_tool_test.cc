// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <tuple>

#include "debugd/src/process_with_id.h"
#include "debugd/src/subprocess_tool.h"

namespace debugd {
namespace {

using SubprocessToolTestParam = std::tuple<bool,   // sandboxed
                                           bool>;  // allow_root_mount_ns

class SubprocessToolTest
    : public testing::TestWithParam<SubprocessToolTestParam> {
 protected:
  SubprocessTool tool_;
};

TEST_P(SubprocessToolTest, CreateProcessAndStop) {
  bool sandboxed;
  bool allow_root_mount_ns;
  std::tie(sandboxed, allow_root_mount_ns) = GetParam();

  ProcessWithId* process = tool_.CreateProcess(sandboxed, allow_root_mount_ns);
  EXPECT_NE(nullptr, process);
  EXPECT_FALSE(process->id().empty());

  std::string handle = process->id();

  DBus::Error error;
  tool_.Stop(handle, &error);
  EXPECT_FALSE(error);
  // |process| is now destroyed by SubprocessTool::Stop().

  tool_.Stop(handle, &error);
  EXPECT_TRUE(error);
  EXPECT_EQ(handle, error.message());
}

INSTANTIATE_TEST_CASE_P(SubprocessToolCreateProcess,
                        SubprocessToolTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

TEST_F(SubprocessToolTest, StopInvalidProcessHandle) {
  std::string invalid_handle = "some_invalid_handle";
  DBus::Error error;
  tool_.Stop(invalid_handle, &error);
  EXPECT_TRUE(error);
  EXPECT_EQ(invalid_handle, error.message());
}

}  // namespace
}  // namespace debugd
