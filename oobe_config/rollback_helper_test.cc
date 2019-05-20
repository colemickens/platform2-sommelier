// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/rollback_helper.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "oobe_config/rollback_constants.h"

namespace oobe_config {

// Writes test file in a directory. Content of the file is the file name.
// Creates parent directories if needed.
void WriteTestFile(const base::FilePath& file_path) {
  ASSERT_TRUE(base::CreateDirectory(file_path.DirName()));
  std::string content = file_path.BaseName().value();
  ASSERT_EQ(content.size(),
            base::WriteFile(file_path, content.c_str(), content.size()));
  LOG(INFO) << "Wrote test file content " << file_path.BaseName().value()
            << " to " << file_path.value();
}

// Verifies that the file at file_path exists and its content matches with the
// file name.
void VerifyTestFile(const base::FilePath& file_path) {
  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_content));
  EXPECT_EQ(file_path.BaseName().value(), file_content);
}

TEST(OobeConfigPrepareSaveTest, PrepareSaveTest) {
  // Creating pre-prepare state.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = temp_dir.GetPath();

  WriteTestFile(PrefixAbsolutePath(temp_path, kInstallAttributesPath));
  WriteTestFile(PrefixAbsolutePath(temp_path, kOwnerKeyFilePath));
  WriteTestFile(PrefixAbsolutePath(temp_path, kShillDefaultProfilePath));
  WriteTestFile(PrefixAbsolutePath(
      temp_path, kPolicyFileDirectory.Append(kPolicyFileName)));
  WriteTestFile(PrefixAbsolutePath(
      temp_path, kPolicyFileDirectory.Append(kPolicyDotOneFileNameForTesting)));
  WriteTestFile(PrefixAbsolutePath(temp_path, kOobeCompletedFile));
  WriteTestFile(PrefixAbsolutePath(temp_path, kMetricsReportingEnabledFile));

  ASSERT_TRUE(PrepareSave(temp_path, /*ignore_permissions_for_testing=*/true));

  VerifyTestFile(PrefixAbsolutePath(temp_path, kSaveTempPath)
                     .Append(kInstallAttributesFileName));
  VerifyTestFile(
      PrefixAbsolutePath(temp_path, kSaveTempPath).Append(kOwnerKeyFileName));
  VerifyTestFile(PrefixAbsolutePath(temp_path, kSaveTempPath)
                     .Append(kShillDefaultProfileFileName));
  VerifyTestFile(
      PrefixAbsolutePath(temp_path, kSaveTempPath).Append(kPolicyFileName));
  VerifyTestFile(PrefixAbsolutePath(temp_path, kSaveTempPath)
                     .Append(kPolicyDotOneFileNameForTesting));
  VerifyTestFile(PrefixAbsolutePath(temp_path, kSaveTempPath)
                     .Append(kOobeCompletedFileName));
  VerifyTestFile(PrefixAbsolutePath(temp_path, kSaveTempPath)
                     .Append(kMetricsReportingEnabledFileName));
}

TEST(OobeConfigPrepareSaveTest, PrepareSaveFolderAlreadyExistedTest) {
  // Creating pre-prepare state.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = temp_dir.GetPath();

  // Create file /var/lib/oobe_config_save/already_existing
  const base::FilePath already_existing_file_path =
      PrefixAbsolutePath(temp_path, kSaveTempPath).Append("already_existing");
  WriteTestFile(already_existing_file_path);
  ASSERT_TRUE(base::PathExists(already_existing_file_path));

  ASSERT_TRUE(PrepareSave(temp_path, /*ignore_permissions_for_testing=*/true));

  EXPECT_FALSE(base::PathExists(already_existing_file_path));
}

TEST(OobeConfigFinishRestoreTest, FinishRestoreTest) {
  // Creating pre-restore state.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = temp_dir.GetPath();

  WriteTestFile(
      PrefixAbsolutePath(temp_path, kUnencryptedStatefulRollbackDataPath));
  WriteTestFile(
      PrefixAbsolutePath(temp_path, kEncryptedStatefulRollbackDataPath));

  WriteTestFile(PrefixAbsolutePath(
      temp_path, kRestoreTempPath.Append(kInstallAttributesFileName)));
  WriteTestFile(PrefixAbsolutePath(temp_path,
                                   kRestoreTempPath.Append(kOwnerKeyFileName)));
  WriteTestFile(PrefixAbsolutePath(
      temp_path, kRestoreTempPath.Append(kShillDefaultProfileFileName)));
  WriteTestFile(
      PrefixAbsolutePath(temp_path, kRestoreTempPath.Append(kPolicyFileName)));
  WriteTestFile(PrefixAbsolutePath(
      temp_path, kRestoreTempPath.Append(kPolicyDotOneFileNameForTesting)));

  // Create dirs
  ASSERT_TRUE(base::CreateDirectory(
      PrefixAbsolutePath(temp_path, kInstallAttributesPath.DirName())));
  ASSERT_TRUE(base::CreateDirectory(
      PrefixAbsolutePath(temp_path, kOwnerKeyFilePath.DirName())));
  ASSERT_TRUE(base::CreateDirectory(
      PrefixAbsolutePath(temp_path, kPolicyFileDirectory)));
  ASSERT_TRUE(base::CreateDirectory(
      PrefixAbsolutePath(temp_path, kShillDefaultProfilePath.DirName())));

  // Make sure the first stage completed file exists.
  WriteTestFile(PrefixAbsolutePath(temp_path, kFirstStageCompletedFile));

  ASSERT_TRUE(
      FinishRestore(temp_path, /*ignore_permissions_for_testing=*/true));

  VerifyTestFile(PrefixAbsolutePath(temp_path, kInstallAttributesPath));
  VerifyTestFile(PrefixAbsolutePath(temp_path, kOwnerKeyFilePath));
  VerifyTestFile(PrefixAbsolutePath(temp_path, kShillDefaultProfilePath));
  WriteTestFile(PrefixAbsolutePath(
      temp_path, kPolicyFileDirectory.Append(kPolicyFileName)));
  VerifyTestFile(PrefixAbsolutePath(
      temp_path, kPolicyFileDirectory.Append(kPolicyDotOneFileNameForTesting)));
}

}  // namespace oobe_config
