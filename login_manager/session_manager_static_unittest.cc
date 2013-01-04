// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <gtest/gtest.h>

namespace login_manager {

TEST(SessionManagerTestStatic, GetArgLists0) {
  std::vector<std::string> args;
  std::vector<std::string> arg_list = SessionManagerService::GetArgList(args);
  EXPECT_EQ(0, arg_list.size());
}

namespace {

static std::vector<std::string> GetArgs(const char** c_args) {
  std::vector<std::string> args;
  while (*c_args) {
    args.push_back(*c_args);
    c_args++;
  }
  return SessionManagerService::GetArgList(args);
}

}  // anonymous namespace

TEST(SessionManagerTestStatic, GetArgLists1) {
  const char* c_args[] = {"a", "b", "c", NULL};
  std::vector<std::string> arg_list = GetArgs(c_args);
  ASSERT_EQ(3, arg_list.size());
  for (uint i = 0; i < arg_list.size(); ++i)
    EXPECT_EQ(c_args[i], arg_list[i]);
}

TEST(SessionManagerTestStatic, GetArgLists_InitialDashes) {
  const char* c_args[] = {"--", "a", "b", "c", NULL};
  std::vector<std::string> arg_list = GetArgs(c_args);
  ASSERT_EQ(3, arg_list.size());
  for (uint i = 0; i < arg_list.size(); ++i)
    EXPECT_EQ(c_args[i+1], arg_list[i]);
}

}  // namespace login_manager
