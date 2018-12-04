// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/loop.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <unistd.h>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/stringprintf.h>
#include <brillo/process.h>
#include <gtest/gtest.h>

namespace {

// Size of a disk sector in bytes.
constexpr int kSectorSize = 512;
constexpr int kSectorCount = 14 * 1024;
// How many sectors the start of a partition needs to be aligned to.
constexpr int kSectorAlign = 2048;
constexpr int kNumPartitions = 5;
// Split disk space evenly between partitions (minus |kSectorAlign| chunk at
// beginning needed for alignment).
constexpr int kMaxPartitionSize =
    (kSectorCount - kSectorAlign) / kNumPartitions;
// Largest partition size possible that is a multiple of |kSectorAlign|.
constexpr int kAlignedPartitionSize =
    kMaxPartitionSize - (kMaxPartitionSize % kSectorAlign);
static_assert(kAlignedPartitionSize > 0, "Partition size must be positive");

constexpr char kTestImageFileName[] = "test.img";
constexpr char kLoopbackDeviceFormat[] = "/dev/loop%d";
constexpr char kLoopbackDevicePartitionFormat[] = "/dev/loop%dp%d";
constexpr char kStatefulPath[] = "/mnt/stateful_partition";
constexpr char kMkfsCommandFormat[] = "/sbin/mkfs.%s";

constexpr char kWriteGptPath[] = "/usr/sbin/write_gpt.sh";
// File format for fake write_gpt.sh which will be imported by clobber-state.
constexpr char kFakeWriteGptFormat[] =
    "#!/bin/sh\n"
    "ROOT_DEV=%sp2\n"
    "ROOT_DISK=%s\n"
    "load_base_vars() {\n"
    "  PARTITION_NUM_KERN_A=1\n"
    "  PARTITION_NUM_ROOT_A=2\n"
    "  PARTITION_NUM_KERN_B=3\n"
    "  PARTITION_NUM_ROOT_B=4\n"
    "  PARTITION_NUM_STATE=5\n"
    "}\n";

constexpr char kSfdiskInputName[] = "sfdisk_input";
// Commands for disk formatting utility sfdisk.
// Specify that partition table should use gpt format.
constexpr char kSfdiskPartitionTableTypeCommand[] = "label: gpt\n";
// Template for partition command (size specified in number of sectors).
constexpr char kSfdiskPartitionCommandFormat[] = "size=%d, type=%s\n";
// ChromeOS Kernel Partition Type.
constexpr char kPartitionTypeKernel[] = "FE3A2A5D-4F32-41A7-B725-ACCC3285A309";
// ChromeOS Root Partition Type.
constexpr char kPartitionTypeRoot[] = "3CB8E202-3B7E-47DD-8A3C-7FF2A13CFCEC";
// Linux Data Partition Type.
constexpr char kPartitionTypeData[] = "0FC63DAF-8483-4772-8E79-3D69D8477DE4";

class FileHandler {
 public:
  explicit FileHandler(base::FilePath root_path) : root_path_(root_path) {}

  bool WriteFile(const std::string& path, const char* contents) {
    int size = strlen(contents);
    base::FilePath fp = root_path_.Append(path);
    if (!base::CreateDirectory(fp.DirName())) {
      return false;
    }
    return base::WriteFile(fp, contents, size) == size;
  }

  bool FileExists(const std::string& path) {
    return base::PathExists(root_path_.Append(path));
  }

  bool ContentsEqual(const std::string& path,
                     const std::string& expected_contents) {
    base::FilePath fp = root_path_.Append(path);
    std::string actual_contents;
    if (!base::ReadFileToString(fp, &actual_contents)) {
      return false;
    }
    return actual_contents == expected_contents;
  }

 private:
  base::FilePath root_path_;
};

}  // namespace

class ClobberStateTest : public testing::Test {
 protected:
  ClobberStateTest() : loop_device_number_(-1) {}

  void SetUp() override {
    ASSERT_EQ(getuid(), 0) << "ClobberStateTest requires root permission.";
    base::FilePath temp_directory;
    ASSERT_TRUE(base::CreateNewTempDirectory("", &temp_directory));
    test_image_path_ = temp_directory.Append(kTestImageFileName);
    stateful_mount_path_ = base::FilePath(kStatefulPath);
    write_gpt_path_ = base::FilePath(kWriteGptPath);

    // Create backing file for loopback device.
    base::File test_image(test_image_path_,
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(test_image.IsValid());
    // Resize file to desired image size, and ensure that all underlying blocks
    // are actually allocated.
    ASSERT_GE(posix_fallocate(test_image.GetPlatformFile(), 0,
                              kSectorSize * kSectorCount),
              0);
    test_image.Close();

    // Setup sfdisk partitioning commands.
    // Write to an intermediate file first because writing to sfdisk's
    // stdin caused the loopback device to unmount prematurely.
    base::FilePath sfdisk_input_path = temp_directory.Append(kSfdiskInputName);
    base::File sfdisk_input(sfdisk_input_path,
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(sfdisk_input.IsValid());
    std::vector<std::string> sfdisk_commands{
        kSfdiskPartitionTableTypeCommand,
        base::StringPrintf(kSfdiskPartitionCommandFormat, kAlignedPartitionSize,
                           kPartitionTypeKernel),
        base::StringPrintf(kSfdiskPartitionCommandFormat, kAlignedPartitionSize,
                           kPartitionTypeRoot),
        base::StringPrintf(kSfdiskPartitionCommandFormat, kAlignedPartitionSize,
                           kPartitionTypeKernel),
        base::StringPrintf(kSfdiskPartitionCommandFormat, kAlignedPartitionSize,
                           kPartitionTypeRoot),
        base::StringPrintf(kSfdiskPartitionCommandFormat, kAlignedPartitionSize,
                           kPartitionTypeData)};

    for (const std::string& command_string : sfdisk_commands) {
      const char* command = command_string.c_str();
      EXPECT_EQ(sfdisk_input.WriteAtCurrentPos(command, strlen(command)),
                strlen(command));
    }
    sfdisk_input.Close();

    // Build partition table on backing file.
    brillo::ProcessImpl proc;
    proc.AddArg("/sbin/sfdisk");
    proc.AddArg(test_image_path_.value());
    proc.RedirectInput(sfdisk_input_path.value());
    ASSERT_EQ(proc.Run(), 0);
    ASSERT_TRUE(base::DeleteFile(sfdisk_input_path, false));

    // Mount test image to loopback device.
    loop_control_ = base::File(base::FilePath("/dev/loop-control"),
                               base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(loop_control_.IsValid());
    loop_device_number_ =
        HANDLE_EINTR(ioctl(loop_control_.GetPlatformFile(), LOOP_CTL_GET_FREE));
    ASSERT_GE(loop_device_number_, 0);
    std::string loop_device_path =
        base::StringPrintf(kLoopbackDeviceFormat, loop_device_number_);
    loop_device_ = base::File(
        base::FilePath(loop_device_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
    ASSERT_TRUE(loop_device_.IsValid());
    test_image_ = base::File(test_image_path_, base::File::FLAG_OPEN |
                                                   base::File::FLAG_READ |
                                                   base::File::FLAG_WRITE);
    ASSERT_TRUE(test_image_.IsValid());
    ASSERT_GE(ioctl(loop_device_.GetPlatformFile(), LOOP_SET_FD,
                    test_image_.GetPlatformFile()),
              0);

    // Update loop device properties.
    struct loop_info64 loop_info;
    ASSERT_GE(
        ioctl(loop_device_.GetPlatformFile(), LOOP_GET_STATUS64, &loop_info),
        0);
    // Enable scanning for partitions on loopback device.
    loop_info.lo_flags |= LO_FLAGS_PARTSCAN;
    // Automatically detach loop device when last file is closed (i.e. when
    // kStatefulPath is unmounted).
    loop_info.lo_flags |= LO_FLAGS_AUTOCLEAR;
    ASSERT_GE(
        ioctl(loop_device_.GetPlatformFile(), LOOP_SET_STATUS64, &loop_info),
        0);

    // Make filesystems on root and stateful partitions.
    ASSERT_TRUE(MakeFilesystem("ext2", 2));
    ASSERT_TRUE(MakeFilesystem("ext2", 4));
    ASSERT_TRUE(MakeFilesystem("ext4", 5));

    // Create mount point and mount stateful partition off of loopback device.
    ASSERT_TRUE(base::CreateDirectory(stateful_mount_path_));
    std::string stateful_loopback_device = base::StringPrintf(
        kLoopbackDevicePartitionFormat, loop_device_number_, 5);
    ASSERT_GE(mount(stateful_loopback_device.c_str(),
                    stateful_mount_path_.value().c_str(), "ext4",
                    MS_SYNCHRONOUS | MS_DIRSYNC, NULL),
              0);

    // Create fake gpt file listing partition structure.
    std::string fake_write_gpt =
        base::StringPrintf(kFakeWriteGptFormat, loop_device_path.c_str(),
                           loop_device_path.c_str());
    size_t length = fake_write_gpt.length();
    EXPECT_EQ(base::WriteFile(write_gpt_path_, fake_write_gpt.c_str(), length),
              length);
  }

  ~ClobberStateTest() {
    // umount call will detach loop device as well.
    EXPECT_GE(umount(kStatefulPath), 0);
    test_image_.Close();
    EXPECT_TRUE(base::DeleteFile(write_gpt_path_, false));
    EXPECT_TRUE(base::DeleteFile(test_image_path_, false));
  }

  bool MakeFilesystem(const char* type, int partition_number) {
    brillo::ProcessImpl proc;
    proc.AddArg(base::StringPrintf(kMkfsCommandFormat, type));
    proc.AddArg(base::StringPrintf(kLoopbackDevicePartitionFormat,
                                   loop_device_number_, partition_number));
    return proc.Run() == 0;
  }

  // Path at which the stateful partition should be mounted.
  base::FilePath stateful_mount_path_;

 private:
  // Image file used as backing file for loopback filesystem.
  base::File test_image_;
  base::FilePath test_image_path_;
  // Path to write_gpt.sh file which will be imported by clobber-state.
  base::FilePath write_gpt_path_;
  // Kernel loopback device control file.
  base::File loop_control_;
  // Loop device behind which the test image is mounted.
  base::File loop_device_;
  int loop_device_number_;
};

TEST_F(ClobberStateTest, BasicTest) {
  FileHandler fh(stateful_mount_path_);
  EXPECT_TRUE(fh.WriteFile("unencrypted/preserve/powerwash_count", "5\n"));
  EXPECT_TRUE(
      fh.WriteFile("unencrypted/preserve/tpm_firmware_update_request", ""));
  EXPECT_TRUE(fh.WriteFile(
      "unencrypted/preserve/update_engine/prefs/rollback-happened", ""));
  EXPECT_TRUE(fh.WriteFile(
      "unencrypted/preserve/update_engine/prefs/rollback-version", "3"));
  EXPECT_TRUE(fh.WriteFile("unencrypted/preserve/attestation.epb", "TEST"));
  EXPECT_TRUE(fh.WriteFile("unencrypted/preserve/not_saved", "not_saved"));

  brillo::ProcessImpl proc;
  proc.AddArg("./clobber-state");
  proc.AddArg("rollback");
  proc.AddArg("fast");
  proc.AddArg("keepimg");
  proc.AddArg("safe");
  proc.AddArg("factory");
  EXPECT_EQ(proc.Run(), 0);

  EXPECT_TRUE(fh.ContentsEqual("unencrypted/preserve/powerwash_count", "6\n"));
  EXPECT_TRUE(
      fh.ContentsEqual("unencrypted/preserve/tpm_firmware_update_request", ""));
  EXPECT_TRUE(fh.ContentsEqual(
      "unencrypted/preserve/update_engine/prefs/rollback-happened", ""));
  EXPECT_TRUE(fh.ContentsEqual(
      "unencrypted/preserve/update_engine/prefs/rollback-version", "3"));
  EXPECT_TRUE(fh.ContentsEqual("unencrypted/preserve/attestation.epb", "TEST"));
  EXPECT_FALSE(fh.FileExists("unencrypted/preserve/not_saved"));
}
