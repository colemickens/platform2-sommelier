// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <limits.h>

#include <memory>
#include <set>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/process.h>
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

TEST(IncrementFileCounter, Nonexistent) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath counter = temp_dir.GetPath().Append("counter");
  EXPECT_TRUE(ClobberState::IncrementFileCounter(counter));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(counter, &contents));
  EXPECT_EQ(contents, "1\n");
}

TEST(IncrementFileCounter, SmallNumber) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath counter = temp_dir.GetPath().Append("counter");
  ASSERT_TRUE(base::WriteFile(counter, "42\n", 3));
  EXPECT_TRUE(ClobberState::IncrementFileCounter(counter));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(counter, &contents));
  EXPECT_EQ(contents, "43\n");
}

TEST(IncrementFileCounter, LargeNumber) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath counter = temp_dir.GetPath().Append("counter");
  ASSERT_TRUE(base::WriteFile(counter, "1238761\n", 8));
  EXPECT_TRUE(ClobberState::IncrementFileCounter(counter));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(counter, &contents));
  EXPECT_EQ(contents, "1238762\n");
}

TEST(IncrementFileCounter, NonNumber) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath counter = temp_dir.GetPath().Append("counter");
  ASSERT_TRUE(base::WriteFile(counter, "cruciverbalist", 14));
  EXPECT_TRUE(ClobberState::IncrementFileCounter(counter));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(counter, &contents));
  EXPECT_EQ(contents, "1\n");
}

TEST(IncrementFileCounter, LongMax) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath counter = temp_dir.GetPath().Append("counter");
  std::string value = std::to_string(LONG_MAX);
  ASSERT_TRUE(base::WriteFile(counter, value.c_str(), value.size()));
  EXPECT_TRUE(ClobberState::IncrementFileCounter(counter));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(counter, &contents));
  EXPECT_EQ(contents, "1\n");
}

TEST(IncrementFileCounter, InputNoNewline) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath counter = temp_dir.GetPath().Append("counter");
  std::string value = std::to_string(7);
  ASSERT_TRUE(base::WriteFile(counter, value.c_str(), value.size()));
  EXPECT_TRUE(ClobberState::IncrementFileCounter(counter));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(counter, &contents));
  EXPECT_EQ(contents, "8\n");
}

TEST(PreserveFiles, NoFiles) {
  base::ScopedTempDir fake_stateful_dir;
  ASSERT_TRUE(fake_stateful_dir.CreateUniqueTempDir());
  base::FilePath fake_stateful = fake_stateful_dir.GetPath();
  ASSERT_TRUE(base::CreateDirectory(
      fake_stateful.Append("unimportant/directory/structure")));

  base::ScopedTempDir fake_tmp_dir;
  ASSERT_TRUE(fake_tmp_dir.CreateUniqueTempDir());
  base::FilePath tar_file = fake_tmp_dir.GetPath().Append("preserved.tar");

  EXPECT_EQ(ClobberState::PreserveFiles(
                fake_stateful, std::vector<base::FilePath>(), tar_file),
            0);

  EXPECT_FALSE(base::PathExists(tar_file));

  ASSERT_EQ(base::WriteFile(tar_file, "", 0), 0);
  EXPECT_TRUE(base::PathExists(tar_file));
  EXPECT_EQ(ClobberState::PreserveFiles(
                fake_stateful, std::vector<base::FilePath>(), tar_file),
            0);

  // PreserveFiles should have deleted existing tar_file.
  EXPECT_FALSE(base::PathExists(tar_file));
}

TEST(PreserveFiles, OneFile) {
  base::FilePath not_preserved_file("unimportant/directory/structure/file.img");
  base::FilePath preserved_file("good/directory/file.tiff");

  base::ScopedTempDir fake_stateful_dir;
  ASSERT_TRUE(fake_stateful_dir.CreateUniqueTempDir());
  base::FilePath fake_stateful = fake_stateful_dir.GetPath();

  base::FilePath stateful_not_preserved =
      fake_stateful.Append(not_preserved_file);
  base::FilePath stateful_preserved = fake_stateful.Append(preserved_file);

  ASSERT_TRUE(base::CreateDirectory(stateful_not_preserved.DirName()));
  ASSERT_EQ(base::WriteFile(stateful_not_preserved, "unneeded", 8), 8);
  ASSERT_TRUE(base::CreateDirectory(stateful_preserved.DirName()));
  ASSERT_EQ(base::WriteFile(stateful_preserved, "test_contents", 13), 13);

  base::ScopedTempDir fake_tmp_dir;
  ASSERT_TRUE(fake_tmp_dir.CreateUniqueTempDir());
  base::FilePath tar_file = fake_tmp_dir.GetPath().Append("preserved.tar");

  std::vector<base::FilePath> preserved_files{preserved_file};
  EXPECT_EQ(
      ClobberState::PreserveFiles(fake_stateful, preserved_files, tar_file), 0);

  ASSERT_TRUE(base::PathExists(tar_file));

  base::ScopedTempDir expand_tar_dir;
  ASSERT_TRUE(expand_tar_dir.CreateUniqueTempDir());
  base::FilePath expand_tar_path = expand_tar_dir.GetPath();

  brillo::ProcessImpl tar;
  tar.AddArg("/bin/tar");
  tar.AddArg("-C");
  tar.AddArg(expand_tar_path.value());
  tar.AddArg("-xf");
  tar.AddArg(tar_file.value());
  ASSERT_EQ(tar.Run(), 0);

  EXPECT_FALSE(base::PathExists(expand_tar_path.Append(not_preserved_file)));

  base::FilePath expanded_preserved = expand_tar_path.Append(preserved_file);
  EXPECT_TRUE(base::PathExists(expanded_preserved));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(expanded_preserved, &contents));
  EXPECT_EQ(contents, "test_contents");
}

TEST(PreserveFiles, ManyFiles) {
  base::FilePath not_preserved_file("unimportant/directory/structure/file.img");
  base::FilePath preserved_file_a("good/directory/file.tiff");
  base::FilePath preserved_file_b("other/folder/saved.bin");

  base::ScopedTempDir fake_stateful_dir;
  ASSERT_TRUE(fake_stateful_dir.CreateUniqueTempDir());
  base::FilePath fake_stateful = fake_stateful_dir.GetPath();

  base::FilePath stateful_not_preserved =
      fake_stateful.Append(not_preserved_file);
  base::FilePath stateful_preserved_a = fake_stateful.Append(preserved_file_a);
  base::FilePath stateful_preserved_b = fake_stateful.Append(preserved_file_b);

  ASSERT_TRUE(base::CreateDirectory(stateful_not_preserved.DirName()));
  ASSERT_EQ(base::WriteFile(stateful_not_preserved, "unneeded", 8), 8);
  ASSERT_TRUE(base::CreateDirectory(stateful_preserved_a.DirName()));
  ASSERT_EQ(base::WriteFile(stateful_preserved_a, "test_contents", 13), 13);
  ASSERT_TRUE(base::CreateDirectory(stateful_preserved_b.DirName()));
  ASSERT_EQ(base::WriteFile(stateful_preserved_b, "data", 4), 4);

  base::ScopedTempDir fake_tmp_dir;
  ASSERT_TRUE(fake_tmp_dir.CreateUniqueTempDir());
  base::FilePath tar_file = fake_tmp_dir.GetPath().Append("preserved.tar");

  std::vector<base::FilePath> preserved_files{preserved_file_a,
                                              preserved_file_b};
  EXPECT_EQ(
      ClobberState::PreserveFiles(fake_stateful, preserved_files, tar_file), 0);

  ASSERT_TRUE(base::PathExists(tar_file));

  base::ScopedTempDir expand_tar_dir;
  ASSERT_TRUE(expand_tar_dir.CreateUniqueTempDir());
  base::FilePath expand_tar_path = expand_tar_dir.GetPath();

  brillo::ProcessImpl tar;
  tar.AddArg("/bin/tar");
  tar.AddArg("-C");
  tar.AddArg(expand_tar_path.value());
  tar.AddArg("-xf");
  tar.AddArg(tar_file.value());
  ASSERT_EQ(tar.Run(), 0);

  EXPECT_FALSE(base::PathExists(expand_tar_path.Append(not_preserved_file)));

  base::FilePath expanded_preserved_a =
      expand_tar_path.Append(preserved_file_a);
  EXPECT_TRUE(base::PathExists(expanded_preserved_a));
  std::string contents_a;
  EXPECT_TRUE(base::ReadFileToString(expanded_preserved_a, &contents_a));
  EXPECT_EQ(contents_a, "test_contents");

  base::FilePath expanded_preserved_b =
      expand_tar_path.Append(preserved_file_b);
  EXPECT_TRUE(base::PathExists(expanded_preserved_b));
  std::string contents_b;
  EXPECT_TRUE(base::ReadFileToString(expanded_preserved_b, &contents_b));
  EXPECT_EQ(contents_b, "data");
}

class MarkDeveloperModeTest : public ::testing::Test {
 protected:
  MarkDeveloperModeTest()
      : cros_system_(std::make_unique<CrosSystemFake>()),
        clobber_(ClobberState::Arguments(), cros_system_.get()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_stateful_ = temp_dir_.GetPath();
    clobber_.SetStatefulForTest(fake_stateful_);
  }

  std::unique_ptr<CrosSystemFake> cros_system_;
  ClobberState clobber_;
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_stateful_;
};

TEST_F(MarkDeveloperModeTest, NotDeveloper) {
  clobber_.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(fake_stateful_.Append(".developer_mode")));

  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDevSwitchBoot, 0));
  clobber_.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(fake_stateful_.Append(".developer_mode")));

  ASSERT_TRUE(
      cros_system_->SetString(CrosSystem::kMainFirmwareActive, "recovery"));
  clobber_.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(fake_stateful_.Append(".developer_mode")));

  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDevSwitchBoot, 1));
  clobber_.MarkDeveloperMode();
  EXPECT_FALSE(base::PathExists(fake_stateful_.Append(".developer_mode")));
}

TEST_F(MarkDeveloperModeTest, IsDeveloper) {
  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDevSwitchBoot, 1));
  ASSERT_TRUE(
      cros_system_->SetString(CrosSystem::kMainFirmwareActive, "not_recovery"));
  clobber_.MarkDeveloperMode();
  EXPECT_TRUE(base::PathExists(fake_stateful_.Append(".developer_mode")));
}

class GetPreservedFilesListTest : public ::testing::Test {
 protected:
  GetPreservedFilesListTest()
      : cros_system_(std::make_unique<CrosSystemFake>()),
        clobber_(ClobberState::Arguments(), cros_system_.get()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_stateful_ = temp_dir_.GetPath();
    clobber_.SetStatefulForTest(fake_stateful_);

    base::FilePath extensions =
        fake_stateful_.Append("unencrypted/import_extensions/extensions");
    ASSERT_TRUE(base::CreateDirectory(extensions));
    ASSERT_EQ(base::WriteFile(extensions.Append("fileA.crx"), "", 0), 0);
    ASSERT_EQ(base::WriteFile(extensions.Append("fileB.crx"), "", 0), 0);
    ASSERT_EQ(base::WriteFile(extensions.Append("fileC.tar"), "", 0), 0);
    ASSERT_EQ(base::WriteFile(extensions.Append("fileD.bmp"), "", 0), 0);
  }

  void SetCompare(std::set<std::string> expected,
                  std::set<base::FilePath> actual) {
    for (const std::string& s : expected) {
      EXPECT_TRUE(actual.count(base::FilePath(s)) == 1)
          << "Expected preserved file not found: " << s;
    }
    for (const base::FilePath& fp : actual) {
      EXPECT_TRUE(expected.count(fp.value()) == 1)
          << "Unexpected preserved file found: " << fp.value();
    }
  }

  std::unique_ptr<CrosSystemFake> cros_system_;
  ClobberState clobber_;
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_stateful_;
};

TEST_F(GetPreservedFilesListTest, NoOptions) {
  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDebugBuild, 0));
  EXPECT_EQ(clobber_.GetPreservedFilesList().size(), 0);

  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDebugBuild, 1));
  std::vector<base::FilePath> preserved_files =
      clobber_.GetPreservedFilesList();
  std::set<base::FilePath> preserved_set(preserved_files.begin(),
                                         preserved_files.end());
  std::set<std::string> expected_preserved_set{".labmachine"};
  SetCompare(expected_preserved_set, preserved_set);
}

TEST_F(GetPreservedFilesListTest, SafeWipe) {
  ClobberState::Arguments args;
  args.safe_wipe = true;
  clobber_.SetArgsForTest(args);

  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDebugBuild, 0));
  std::vector<base::FilePath> preserved_files =
      clobber_.GetPreservedFilesList();
  std::set<base::FilePath> preserved_set(preserved_files.begin(),
                                         preserved_files.end());
  std::set<std::string> expected_preserved_set{
      "unencrypted/preserve/powerwash_count",
      "unencrypted/preserve/tpm_firmware_update_request",
      "unencrypted/preserve/update_engine/prefs/rollback-happened",
      "unencrypted/preserve/update_engine/prefs/rollback-version",
      "unencrypted/cros-components/demo_mode_resources/image.squash",
      "unencrypted/cros-components/demo_mode_resources/imageloader.json",
      "unencrypted/cros-components/demo_mode_resources/imageloader.sig.2",
      "unencrypted/cros-components/demo_mode_resources/table"};
  SetCompare(expected_preserved_set, preserved_set);
}

TEST_F(GetPreservedFilesListTest, SafeAndRollbackWipe) {
  ClobberState::Arguments args;
  args.safe_wipe = true;
  args.rollback_wipe = true;
  clobber_.SetArgsForTest(args);
  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDebugBuild, 0));

  std::vector<base::FilePath> preserved_files =
      clobber_.GetPreservedFilesList();
  std::set<base::FilePath> preserved_set(preserved_files.begin(),
                                         preserved_files.end());
  std::set<std::string> expected_preserved_set{
      "unencrypted/preserve/powerwash_count",
      "unencrypted/preserve/tpm_firmware_update_request",
      "unencrypted/preserve/update_engine/prefs/rollback-happened",
      "unencrypted/preserve/update_engine/prefs/rollback-version",
      "unencrypted/cros-components/demo_mode_resources/image.squash",
      "unencrypted/cros-components/demo_mode_resources/imageloader.json",
      "unencrypted/cros-components/demo_mode_resources/imageloader.sig.2",
      "unencrypted/cros-components/demo_mode_resources/table",
      "unencrypted/preserve/attestation.epb",
      "unencrypted/preserve/rollback_data"};
  SetCompare(expected_preserved_set, preserved_set);
}

TEST_F(GetPreservedFilesListTest, FactoryWipe) {
  ClobberState::Arguments args;
  args.factory_wipe = true;
  clobber_.SetArgsForTest(args);

  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDebugBuild, 0));
  std::vector<base::FilePath> preserved_files =
      clobber_.GetPreservedFilesList();
  std::set<base::FilePath> preserved_set(preserved_files.begin(),
                                         preserved_files.end());
  std::set<std::string> expected_preserved_set{
      "unencrypted/import_extensions/extensions/fileA.crx",
      "unencrypted/import_extensions/extensions/fileB.crx"};
  SetCompare(expected_preserved_set, preserved_set);
}

TEST_F(GetPreservedFilesListTest, SafeRollbackFactoryWipe) {
  ClobberState::Arguments args;
  args.safe_wipe = true;
  args.rollback_wipe = true;
  args.factory_wipe = true;
  clobber_.SetArgsForTest(args);

  ASSERT_TRUE(cros_system_->SetInt(CrosSystem::kDebugBuild, 0));
  std::vector<base::FilePath> preserved_files =
      clobber_.GetPreservedFilesList();
  std::set<base::FilePath> preserved_set(preserved_files.begin(),
                                         preserved_files.end());
  std::set<std::string> expected_preserved_set{
      "unencrypted/preserve/powerwash_count",
      "unencrypted/preserve/tpm_firmware_update_request",
      "unencrypted/preserve/update_engine/prefs/rollback-happened",
      "unencrypted/preserve/update_engine/prefs/rollback-version",
      "unencrypted/cros-components/demo_mode_resources/image.squash",
      "unencrypted/cros-components/demo_mode_resources/imageloader.json",
      "unencrypted/cros-components/demo_mode_resources/imageloader.sig.2",
      "unencrypted/cros-components/demo_mode_resources/table",
      "unencrypted/preserve/attestation.epb",
      "unencrypted/preserve/rollback_data",
      "unencrypted/import_extensions/extensions/fileA.crx",
      "unencrypted/import_extensions/extensions/fileB.crx"};
  SetCompare(expected_preserved_set, preserved_set);
}
