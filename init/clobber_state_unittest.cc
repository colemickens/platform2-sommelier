// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <vector>

#include <gtest/gtest.h>

TEST(ParseArgv, EmptyArgs) {
  std::vector<const char*> argv{"clobber-state"};
  ClobberState::Arguments args = ClobberState::ParseArgv(argv.size(), &argv[0]);
  EXPECT_FALSE(args.factory_wipe);
  EXPECT_FALSE(args.fast_wipe);
  EXPECT_FALSE(args.keepimg);
  EXPECT_FALSE(args.safe_wipe);
  EXPECT_FALSE(args.rollback_wipe);
}

TEST(ParseArgv, AllArgsIndividual) {
  std::vector<const char*> argv{"clobber-state", "fast",     "factory",
                                "keepimg",       "rollback", "safe"};
  ClobberState::Arguments args = ClobberState::ParseArgv(argv.size(), &argv[0]);
  EXPECT_TRUE(args.factory_wipe);
  EXPECT_TRUE(args.fast_wipe);
  EXPECT_TRUE(args.keepimg);
  EXPECT_TRUE(args.safe_wipe);
  EXPECT_TRUE(args.rollback_wipe);
}

TEST(ParseArgv, AllArgsSquished) {
  std::vector<const char*> argv{"clobber-state",
                                "fast factory keepimg rollback safe"};
  ClobberState::Arguments args = ClobberState::ParseArgv(argv.size(), &argv[0]);
  EXPECT_TRUE(args.factory_wipe);
  EXPECT_TRUE(args.fast_wipe);
  EXPECT_TRUE(args.keepimg);
  EXPECT_TRUE(args.safe_wipe);
  EXPECT_TRUE(args.rollback_wipe);
}

TEST(ParseArgv, SomeArgsIndividual) {
  std::vector<const char*> argv{"clobber-state", "rollback", "fast", "keepimg"};
  ClobberState::Arguments args = ClobberState::ParseArgv(argv.size(), &argv[0]);
  EXPECT_FALSE(args.factory_wipe);
  EXPECT_TRUE(args.fast_wipe);
  EXPECT_TRUE(args.keepimg);
  EXPECT_FALSE(args.safe_wipe);
  EXPECT_TRUE(args.rollback_wipe);
}

TEST(ParseArgv, SomeArgsSquished) {
  std::vector<const char*> argv{"clobber-state", "rollback safe fast"};
  ClobberState::Arguments args = ClobberState::ParseArgv(argv.size(), &argv[0]);
  EXPECT_FALSE(args.factory_wipe);
  EXPECT_TRUE(args.fast_wipe);
  EXPECT_FALSE(args.keepimg);
  EXPECT_TRUE(args.safe_wipe);
  EXPECT_TRUE(args.rollback_wipe);
}
