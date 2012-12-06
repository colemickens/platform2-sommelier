// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stateful_recovery.h"

#include <base/file_util.h>
#include <gtest/gtest.h>

#include "mock_platform.h"

namespace cryptohome {
using std::string;
using std::ostringstream;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;

TEST(StatefulRecovery, ValidRequest) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportBlockUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverBlockUsage))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportInodeUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverInodeUsage))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
              ReportFilesystemDetails(StatefulRecovery::kRecoverSource,
                StatefulRecovery::kRecoverFilesystemDetails))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, InvalidFlagFileContents) {
  MockPlatform platform;
  std::string flag_content = "0 hello";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  StatefulRecovery recovery(&platform);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UnreadableFlagFile) {
  MockPlatform platform;
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(Return(false));
  StatefulRecovery recovery(&platform);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UncopyableData) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, DirectoryCreationFailure) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, BlockUsageFailure) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportBlockUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverBlockUsage))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, InodeUsageFailure) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportBlockUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverBlockUsage))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportInodeUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverInodeUsage))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, FilesystemDetailsFailure) {
  MockPlatform platform;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportBlockUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverBlockUsage))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportInodeUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverInodeUsage))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
              ReportFilesystemDetails(StatefulRecovery::kRecoverSource,
                StatefulRecovery::kRecoverFilesystemDetails))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, MountsParseOk) {
  Platform platform;
  FilePath mtab;
  FILE *fp;
  std::string filesystem, device_in, device_out, mtab_path, mtab_contents;

  filesystem = "/second/star/to/the/right";
  device_in = "/dev/pan";

  mtab_contents.append(device_in);
  mtab_contents.append(" ");
  mtab_contents.append(filesystem);
  mtab_contents.append(" pixie default 0 0\n");

  fp = file_util::CreateAndOpenTemporaryFile(&mtab);
  ASSERT_TRUE(fp != NULL);
  EXPECT_TRUE(fwrite(mtab_contents.c_str(), mtab_contents.length(), 1,
                     fp) == 1);
  EXPECT_TRUE(fclose(fp) == 0);

  mtab_path = mtab.value();
  platform.set_mtab_path(mtab_path);

  /* Fails if item is missing. */
  EXPECT_FALSE(platform.FindFilesystemDevice("monkey", &device_out));

  /* Works normally. */
  device_out.clear();
  EXPECT_TRUE(platform.FindFilesystemDevice(filesystem, &device_out));
  EXPECT_TRUE(device_out == device_in);

  /* Trailing slashes are okay. */
  filesystem += "///";
  device_out.clear();
  EXPECT_TRUE(platform.FindFilesystemDevice(filesystem, &device_out));
  EXPECT_TRUE(device_out == device_in);

  /* Clean up. */
  EXPECT_TRUE(file_util::Delete(mtab, false));
}

TEST(StatefulRecovery, UsageReportOk) {
  Platform platform;
  FilePath tempdir;
  std::string output;
  int64 size;

  ASSERT_TRUE(file_util::CreateNewTempDirectory("unittestXXXXXX", &tempdir));
  output = tempdir.value();
  output += "/output.txt";

  /* Reporting on a valid location produces output. */
  EXPECT_FALSE(file_util::PathExists(FilePath(output)));
  EXPECT_TRUE(platform.ReportBlockUsage("/", output));
  EXPECT_TRUE(file_util::GetFileSize(FilePath(output), &size));
  EXPECT_FALSE(size == 0);
  EXPECT_TRUE(file_util::Delete(FilePath(output), false));

  /* Reporting on a valid location produces output. */
  EXPECT_FALSE(file_util::PathExists(FilePath(output)));
  EXPECT_TRUE(platform.ReportInodeUsage("/", output));
  EXPECT_TRUE(file_util::GetFileSize(FilePath(output), &size));
  EXPECT_FALSE(size == 0);
  EXPECT_TRUE(file_util::Delete(FilePath(output), false));

  /* Reporting on an invalid location fails. */
  EXPECT_FALSE(file_util::PathExists(FilePath(output)));
  EXPECT_FALSE(platform.ReportBlockUsage("/this/is/very/wrong", output));
  EXPECT_TRUE(file_util::Delete(FilePath(output), false));

  /* Reporting on an invalid location fails. */
  EXPECT_FALSE(file_util::PathExists(FilePath(output)));
  EXPECT_FALSE(platform.ReportInodeUsage("/this/is/very/wrong", output));
  EXPECT_TRUE(file_util::Delete(FilePath(output), false));

  /* TODO(keescook): mockable tune2fs, since it's not installed in chroot. */
}

}  // namespace cryptohome
