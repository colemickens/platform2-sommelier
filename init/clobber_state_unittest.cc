// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <memory>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "init/crossystem.h"
#include "init/crossystem_fake.h"

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

TEST(MarkDeveloperMode, NotDeveloper) {
  ClobberState::Arguments args;
  std::unique_ptr<CrosSystemFake> cros_system =
      std::make_unique<CrosSystemFake>();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp = temp_dir.GetPath();
  ClobberState clobber(args, cros_system.get());
  clobber.SetStatefulForTest(temp);

  clobber.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(temp.Append(".developer_mode")));

  ASSERT_TRUE(cros_system->SetInt(CrosSystem::kDevSwitchBoot, 0));
  clobber.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(temp.Append(".developer_mode")));

  ASSERT_TRUE(
      cros_system->SetString(CrosSystem::kMainFirmwareActive, "recovery"));
  clobber.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(temp.Append(".developer_mode")));

  ASSERT_TRUE(cros_system->SetInt(CrosSystem::kDevSwitchBoot, 1));
  clobber.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(temp.Append(".developer_mode")));
}

TEST(MarkDeveloperMode, IsDeveloper) {
  ClobberState::Arguments args;
  std::unique_ptr<CrosSystemFake> cros_system =
      std::make_unique<CrosSystemFake>();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp = temp_dir.GetPath();
  ClobberState clobber(args, cros_system.get());
  clobber.SetStatefulForTest(temp);

  ASSERT_TRUE(cros_system->SetInt(CrosSystem::kDevSwitchBoot, 1));
  ASSERT_TRUE(
      cros_system->SetString(CrosSystem::kMainFirmwareActive, "not_recovery"));
  clobber.MarkDeveloperMode();
  EXPECT_TRUE(base::PathExists(temp.Append(".developer_mode")));
}
