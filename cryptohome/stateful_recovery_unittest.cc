// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/stateful_recovery.h"

#include <base/files/file_util.h>
#include <gtest/gtest.h>

#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_service.h"

namespace cryptohome {

using base::FilePath;
using std::ostringstream;

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::_;

TEST(StatefulRecovery, ValidRequestV1) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(false));
  EXPECT_CALL(platform, StatVFS(FilePath(StatefulRecovery::kRecoverSource), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      WriteStringToFile(FilePath(StatefulRecovery::kRecoverBlockUsage), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      ReportFilesystemDetails(FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverFilesystemDetails)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      Copy(FilePath(StatefulRecovery::kRecoverSource),
           FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV1WriteProtected) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
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
  FilePath mount_path("/home/.shadow/hashhashash/mount");
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .WillOnce(DoAll(SetArgPointee<5>(result), Return(true)));
  EXPECT_CALL(service, GetMountPointForUser(StrEq(user), _))
    .WillOnce(DoAll(SetArgPointee<1>(mount_path), Return(true)));
  EXPECT_CALL(platform,
      Copy(mount_path, FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(service, Unmount(_, _))
    .WillOnce(DoAll(SetArgPointee<0>(result), Return(true)));

  EXPECT_CALL(service, IsOwner(_))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(true));

  // CopyPartitionInfo
  EXPECT_CALL(platform, StatVFS(FilePath(StatefulRecovery::kRecoverSource), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      WriteStringToFile(FilePath(StatefulRecovery::kRecoverBlockUsage), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportFilesystemDetails(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverFilesystemDetails)))
    .WillOnce(Return(true));

  // CopyPartitionContents
  EXPECT_CALL(platform, Copy(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverDestination)))
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
  FilePath mount_path("/home/.shadow/hashhashash/mount");
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .WillOnce(DoAll(SetArgPointee<5>(result), Return(true)));
  EXPECT_CALL(service, GetMountPointForUser(StrEq(user), _))
    .WillOnce(DoAll(SetArgPointee<1>(mount_path), Return(true)));
  EXPECT_CALL(platform, Copy(
        mount_path,
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(service, Unmount(_, _))
    .WillOnce(DoAll(SetArgPointee<0>(result), Return(true)));

  EXPECT_CALL(service, IsOwner(_))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV2BadUser) {
  MockPlatform platform;
  MockService service;
  gboolean result = true;
  std::string user = "user@example.com";
  std::string passkey = "abcd1234";
  std::string flag_content = "2\n" + user + "\n" + passkey;
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .WillOnce(DoAll(SetArgPointee<5>(result), Return(false)));

  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, ValidRequestV2BadUserNotWriteProtected) {
  MockPlatform platform;
  MockService service;
  gboolean result = true;
  std::string user = "user@example.com";
  std::string passkey = "abcd1234";
  std::string flag_content = "2\n" + user + "\n" + passkey;
  FilePath mount_path("/home/.shadow/hashhashash/mount");
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .WillOnce(DoAll(SetArgPointee<5>(result), Return(false)));

  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(false));

  // CopyPartitionInfo
  EXPECT_CALL(platform, StatVFS(FilePath(StatefulRecovery::kRecoverSource), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      WriteStringToFile(FilePath(StatefulRecovery::kRecoverBlockUsage), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportFilesystemDetails(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverFilesystemDetails)))
    .WillOnce(Return(true));

  // CopyPartitionContents
  EXPECT_CALL(platform, Copy(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

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
  FilePath mount_path("/home/.shadow/hashhashash/mount");
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  // CopyUserContents
  EXPECT_CALL(service, Mount(StrEq(user), StrEq(passkey), false, false,
                             _, _, _))
    .WillOnce(DoAll(SetArgPointee<5>(result), Return(true)));
  EXPECT_CALL(service, GetMountPointForUser(StrEq(user), _))
    .WillOnce(DoAll(SetArgPointee<1>(mount_path), Return(true)));
  EXPECT_CALL(platform,
      Copy(mount_path, FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(service, Unmount(_, _))
    .WillOnce(DoAll(SetArgPointee<0>(result), Return(true)));

  EXPECT_CALL(service, IsOwner(_))
    .WillOnce(Return(false));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(false));

  // CopyPartitionInfo
  EXPECT_CALL(platform, StatVFS(FilePath(StatefulRecovery::kRecoverSource), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, WriteStringToFile(
        FilePath(StatefulRecovery::kRecoverBlockUsage), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportFilesystemDetails(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverFilesystemDetails)))
    .WillOnce(Return(true));

  // CopyPartitionContents
  EXPECT_CALL(platform, Copy(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_TRUE(recovery.Recover());
}

TEST(StatefulRecovery, InvalidFlagFileContents) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "0 hello";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  StatefulRecovery recovery(&platform, &service);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UnreadableFlagFile) {
  MockPlatform platform;
  MockService service;
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(Return(false));
  StatefulRecovery recovery(&platform, &service);
  EXPECT_FALSE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, UncopyableData) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, CreateDirectory(
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(false));
  EXPECT_CALL(platform, Copy(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, DirectoryCreationFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, CreateDirectory(
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, StatVFSFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, CreateDirectory(
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(false));
  EXPECT_CALL(platform, Copy(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, StatVFS(FilePath(StatefulRecovery::kRecoverSource), _))
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, FilesystemDetailsFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, FirmwareWriteProtected())
    .WillOnce(Return(false));
  EXPECT_CALL(platform,
      Copy(FilePath(StatefulRecovery::kRecoverSource),
            FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, StatVFS(FilePath(StatefulRecovery::kRecoverSource), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      WriteStringToFile(FilePath(StatefulRecovery::kRecoverBlockUsage), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform, ReportFilesystemDetails(
        FilePath(StatefulRecovery::kRecoverSource),
        FilePath(StatefulRecovery::kRecoverFilesystemDetails)))
    .WillOnce(Return(false));

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

TEST(StatefulRecovery, MountsParseOk) {
  Platform platform;
  FilePath mount_info;
  FILE *fp;
  std::string device_in = "/dev/pan", device_out, mount_info_contents;

  mount_info_contents.append("84 24 0:29 / ");
  FilePath filesystem = FilePath("/second/star/to/the/right");
  mount_info_contents.append(filesystem.value());
  mount_info_contents.append(" rw,nosuid,nodev,noexec,relatime - fairyfs ");
  mount_info_contents.append(device_in);
  mount_info_contents.append(" rw,ecryp...");

  fp = base::CreateAndOpenTemporaryFile(&mount_info);
  ASSERT_TRUE(fp != NULL);
  EXPECT_EQ(fwrite(mount_info_contents.c_str(),
                   mount_info_contents.length(), 1, fp), 1);
  EXPECT_EQ(fclose(fp), 0);

  platform.set_mount_info_path(mount_info);

  /* Fails if item is missing. */
  EXPECT_FALSE(platform.FindFilesystemDevice(
        FilePath("monkey"), &device_out));

  /* Works normally. */
  device_out.clear();
  EXPECT_TRUE(platform.FindFilesystemDevice(filesystem, &device_out));
  EXPECT_TRUE(device_out == device_in);

  /* Clean up. */
  EXPECT_TRUE(base::DeleteFile(mount_info, false));
}

TEST(StatefulRecovery, UsageReportOk) {
  Platform platform;

  struct statvfs vfs;
  /* Reporting on a valid location produces output. */
  EXPECT_TRUE(platform.StatVFS(FilePath("/"), &vfs));
  EXPECT_NE(vfs.f_blocks, 0);

  /* Reporting on an invalid location fails. */
  EXPECT_FALSE(platform.StatVFS(FilePath("/this/is/very/wrong"), &vfs));

  /* TODO(keescook): mockable tune2fs, since it's not installed in chroot. */
}

TEST(StatefulRecovery, DestinationRecreateFailure) {
  MockPlatform platform;
  MockService service;
  std::string flag_content = "1";
  EXPECT_CALL(platform,
      ReadFileToString(FilePath(StatefulRecovery::kFlagFile), _))
    .WillOnce(DoAll(SetArgPointee<1>(flag_content), Return(true)));
  EXPECT_CALL(platform,
      DeleteFile(FilePath(StatefulRecovery::kRecoverDestination), _))
    .WillOnce(Return(true));
  EXPECT_CALL(platform,
      CreateDirectory(FilePath(StatefulRecovery::kRecoverDestination)))
    .WillOnce(Return(false));
  EXPECT_CALL(platform,
      Copy(_, FilePath(StatefulRecovery::kRecoverDestination)))
    .Times(0);

  StatefulRecovery recovery(&platform, &service);
  EXPECT_TRUE(recovery.Requested());
  EXPECT_FALSE(recovery.Recover());
}

}  // namespace cryptohome
