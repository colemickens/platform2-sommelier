// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stateful_recovery.h"

#include <base/file_util.h>
#include <gtest/gtest.h>

#include "mock_platform.h"
#include "mock_service.h"

namespace cryptohome {
using std::string;
using std::ostringstream;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::SetArgumentPointee;

TEST(StatefulRecovery, ValidRequestV1) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(false));
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

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV1WriteProtected) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV2) {
  MockPlatform platform;
  MockService service;
  gboolean result = true;
  std::string user = "user@example.com";
  std::string passkey = "abcd1234";
  std::string flag_content = "2\n" + user + "\n" + passkey;
  std::string mount_path = "/home/.shadow/hashhashash/mount";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<5>(result), Return(true)));
  EXPECT_CALL(service, GetMountPointForUser(StrEq(user), _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(mount_path), Return(true)));
  EXPECT_CALL(platform, Copy(StrEq(mount_path),
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(service, UnmountForUser(StrEq(user), _, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(result), Return(true)));

  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(service, IsOwner(_))
    .Times(1)
    .WillOnce(Return(true));

  // CopyPartitionInfo
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

  // CopyPartitionContents
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV2NotOwner) {
  MockPlatform platform;
  MockService service;
  gboolean result = true;
  std::string user = "user@example.com";
  std::string passkey = "abcd1234";
  std::string flag_content = "2\n" + user + "\n" + passkey;
  std::string mount_path = "/home/.shadow/hashhashash/mount";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<5>(result), Return(true)));
  EXPECT_CALL(service, GetMountPointForUser(StrEq(user), _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(mount_path), Return(true)));
  EXPECT_CALL(platform, Copy(StrEq(mount_path),
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(service, UnmountForUser(StrEq(user), _, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(result), Return(true)));

  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(service, IsOwner(_))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV2NotOwnerNotWriteProtected) {
  MockPlatform platform;
  MockService service;
  gboolean result = true;
  std::string user = "user@example.com";
  std::string passkey = "abcd1234";
  std::string flag_content = "2\n" + user + "\n" + passkey;
  std::string mount_path = "/home/.shadow/hashhashash/mount";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<5>(result), Return(true)));
  EXPECT_CALL(service, GetMountPointForUser(StrEq(user), _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(mount_path), Return(true)));
  EXPECT_CALL(platform, Copy(StrEq(mount_path),
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(service, UnmountForUser(StrEq(user), _, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(result), Return(true)));

  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(false));

  // CopyPartitionInfo
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

  // CopyPartitionContents
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, InvalidFlagFileContents) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "0 hello";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  StatefulRecovery recovery(&platform, &service);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UnreadableFlagFile) {
  MockPlatform platform;
  MockService service;
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(Return(false));
  StatefulRecovery recovery(&platform, &service);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UncopyableData) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, DirectoryCreationFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, BlockUsageFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_CALL(platform, Copy(StatefulRecovery::kRecoverSource,
                             StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportBlockUsage(StatefulRecovery::kRecoverSource,
                                         StatefulRecovery::kRecoverBlockUsage))
    .Times(1)
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, InodeUsageFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(false));
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

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, FilesystemDetailsFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform, ReadFileToString(StatefulRecovery::kFlagFile, _))
    .Times(1)
    .WillOnce(DoAll(SetArgumentPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform, CreateDirectory(StatefulRecovery::kRecoverDestination))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .Times(1)
    .WillOnce(Return(false));
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

  StatefulRecovery recovery(&platform, &service);
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
