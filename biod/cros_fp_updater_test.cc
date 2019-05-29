// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/cros_fp_updater.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chromeos/ec/ec_commands.h>

#include "biod/cros_fp_firmware.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace {

constexpr char kTestImageROVersion[] = "nocturne_fp_v2.2.64-58cf5974e";
constexpr char kTestImageRWVersion[] = "nocturne_fp_v2.2.110-b936c0a3c";
constexpr char kTestImageFileName[] = "nocturne_fp_v2.2.110-b936c0a3c.bin";
const base::FilePath kInitFilePath("/UNTOUCHED_PATH");

const std::vector<std::string> kValidFirmwareFileNames = {
    kTestImageFileName,
    "unknown_fp_v123.123.123-123456789.bin",
    "0_fp_0.bin",
    "_fp_.bin",
};

const std::vector<std::string> kInvalidFirmwareFileNames = {
    "nocturne_fp_v2.2.110-b936c0a3c.txt",
    "not_fpmcu_firmware.bin",
    "not_fpmcu_firmware.txt",
    "_fp_.txt",
    "file",
};

const std::vector<biod::updater::FindFirmwareFileStatus>
    kFindFirmwareFileStatuses = {
        biod::updater::FindFirmwareFileStatus::kFoundFile,
        biod::updater::FindFirmwareFileStatus::kNoDirectory,
        biod::updater::FindFirmwareFileStatus::kFileNotFound,
        biod::updater::FindFirmwareFileStatus::kMultipleFiles,
};

const std::vector<enum ec_current_image> kEcCurrentImageEnums = {
    EC_IMAGE_UNKNOWN,
    EC_IMAGE_RO,
    EC_IMAGE_RW,
};

class MockCrosFpDeviceUpdate : public biod::CrosFpDeviceUpdate {
 public:
  MOCK_CONST_METHOD1(GetVersion, bool(biod::CrosFpDevice::EcVersion* ecver));
  MOCK_CONST_METHOD1(IsFlashProtectEnabled, bool(bool* status));
  MOCK_CONST_METHOD2(Flash,
                     bool(const biod::CrosFpFirmware& fw,
                          enum ec_current_image image));
};

class MockCrosFpBootUpdateCtrl : public biod::CrosFpBootUpdateCtrl {
 public:
  MOCK_CONST_METHOD0(TriggerBootUpdateSplash, bool());
  MOCK_CONST_METHOD0(ScheduleReboot, bool());
};

class MockCrosFpFirmware : public biod::CrosFpFirmware {
 public:
  MockCrosFpFirmware() { set_status(biod::CrosFpFirmware::Status::kOk); }

  void SetMockFwVersion(const ImageVersion& version) { set_version(version); }
};

}  // namespace

namespace biod {

namespace updater {

class CrosFpUpdaterFindFirmwareTest : public ::testing::Test {
 protected:
  void SetUp() override { CHECK(ResetTestTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  bool ResetTestTempDir() {
    if (temp_dir_.IsValid()) {
      if (!temp_dir_.Delete()) {
        return false;
      }
    }
    return temp_dir_.CreateUniqueTempDir();
  }

  const base::FilePath& GetTestTempDir() const { return temp_dir_.GetPath(); }
  bool TouchFile(const base::FilePath& abspath) const {
    if (!GetTestTempDir().IsParent(abspath)) {
      LOG(ERROR) << "Asked to TouchFile outside test environment.";
      return false;
    }

    base::File file(abspath,
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    EXPECT_TRUE(file.IsValid());
    file.Close();

    EXPECT_TRUE(base::PathExists(abspath));
    return true;
  }

  CrosFpUpdaterFindFirmwareTest() = default;
  ~CrosFpUpdaterFindFirmwareTest() override = default;

  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosFpUpdaterFindFirmwareTest);
};

TEST_F(CrosFpUpdaterFindFirmwareTest, InvalidPathBlank) {
  base::FilePath out_file_path(kInitFilePath);
  // Given an empty directory path, searching for a firmware file
  auto status = FindFirmwareFile(base::FilePath(""), &out_file_path);
  // fails with a no directory error
  EXPECT_EQ(status, FindFirmwareFileStatus::kNoDirectory);
  // without modifying the output file path.
  EXPECT_EQ(out_file_path, kInitFilePath);
}

TEST_F(CrosFpUpdaterFindFirmwareTest, InvalidPathOddChars) {
  base::FilePath out_file_path(kInitFilePath);
  // Given "--" as directory path, searching for a firmware file
  auto status = FindFirmwareFile(base::FilePath("--"), &out_file_path);
  // fails with a no directory error
  EXPECT_EQ(status, FindFirmwareFileStatus::kNoDirectory);
  // without modifying the output file path.
  EXPECT_EQ(out_file_path, kInitFilePath);
}

TEST_F(CrosFpUpdaterFindFirmwareTest, DirectoryWithoutFirmware) {
  base::FilePath out_file_path(kInitFilePath);
  // Given a directory with no firmware files, searching for a firmware file
  auto status = FindFirmwareFile(GetTestTempDir(), &out_file_path);
  // fails with a file not found error
  EXPECT_EQ(status, FindFirmwareFileStatus::kFileNotFound);
  // without modifying the output file path.
  EXPECT_EQ(out_file_path, kInitFilePath);
}

TEST_F(CrosFpUpdaterFindFirmwareTest, OneGoodFirmwareFilePattern) {
  for (const auto& good_fw_name : kValidFirmwareFileNames) {
    base::FilePath fw_file_path, out_file_path;
    CHECK(ResetTestTempDir());

    // Given a directory with one correctly named firmware file,
    fw_file_path = GetTestTempDir().Append(good_fw_name);
    EXPECT_TRUE(TouchFile(fw_file_path));

    // searching for a firmware file
    auto status = FindFirmwareFile(GetTestTempDir(), &out_file_path);
    // succeeds
    EXPECT_EQ(status, FindFirmwareFileStatus::kFoundFile);
    // and returns the path to the original firmware file.
    EXPECT_EQ(out_file_path, fw_file_path);
  }
}

TEST_F(CrosFpUpdaterFindFirmwareTest, OneBadFirmwareFilePattern) {
  for (const auto& bad_fw_name : kInvalidFirmwareFileNames) {
    base::FilePath fw_file_path, out_file_path(kInitFilePath);
    CHECK(ResetTestTempDir());

    // Given a directory with one incorrectly named firmware file,
    fw_file_path = GetTestTempDir().Append(bad_fw_name);
    EXPECT_TRUE(TouchFile(fw_file_path));

    // searching for a firmware file
    auto status = FindFirmwareFile(GetTestTempDir(), &out_file_path);
    // fails with a file not found error
    EXPECT_EQ(status, FindFirmwareFileStatus::kFileNotFound);
    // without modifying the output file path.
    EXPECT_EQ(out_file_path, kInitFilePath);
  }
}

TEST_F(CrosFpUpdaterFindFirmwareTest, MultipleFiles) {
  base::FilePath out_file_path(kInitFilePath);

  // Given a directory with two correctly named firmware files,
  EXPECT_GE(kValidFirmwareFileNames.size(), 2);
  EXPECT_TRUE(TouchFile(GetTestTempDir().Append(kValidFirmwareFileNames[0])));
  EXPECT_TRUE(TouchFile(GetTestTempDir().Append(kValidFirmwareFileNames[1])));

  // searching for a firmware file
  auto status = FindFirmwareFile(GetTestTempDir(), &out_file_path);

  // fails with a multiple files error
  EXPECT_EQ(status, FindFirmwareFileStatus::kMultipleFiles);
  // without modifying the output file path.
  EXPECT_EQ(out_file_path, kInitFilePath);
}

TEST_F(CrosFpUpdaterFindFirmwareTest, OneGoodAndOneBadFirmwareFilePattern) {
  base::FilePath out_file_path, good_file_path, bad_file_path;

  // Given a directory with one correctly named and one incorrectly named
  // firmware file,
  good_file_path = GetTestTempDir().Append(kValidFirmwareFileNames[0]);
  bad_file_path = GetTestTempDir().Append(kInvalidFirmwareFileNames[0]);
  EXPECT_TRUE(TouchFile(good_file_path));
  EXPECT_TRUE(TouchFile(bad_file_path));

  // searching for a firmware file
  auto status = FindFirmwareFile(GetTestTempDir(), &out_file_path);
  // succeeds
  EXPECT_EQ(status, FindFirmwareFileStatus::kFoundFile);
  // and returns the path to the single correctly named firmware file.
  EXPECT_EQ(out_file_path, good_file_path);
}

TEST_F(CrosFpUpdaterFindFirmwareTest, NonblankStatusMessages) {
  // Given a FindFirmwareFile status
  for (auto status : kFindFirmwareFileStatuses) {
    // when we ask for the human readable string
    std::string msg = FindFirmwareFileStatusToString(status);
    // expect it to not be "".
    EXPECT_FALSE(msg.empty()) << "Status " << static_cast<int>(status)
                              << " converts to a blank status string.";
  }
}

TEST_F(CrosFpUpdaterFindFirmwareTest, UniqueStatusMessages) {
  // Given a set of all FindFirmwareFile status messages
  std::unordered_set<std::string> status_msgs;
  for (auto status : kFindFirmwareFileStatuses) {
    status_msgs.insert(FindFirmwareFileStatusToString(status));
  }

  // expect the set to contain the same number of unique messages
  // as there are original statuses.
  EXPECT_EQ(status_msgs.size(), kFindFirmwareFileStatuses.size())
      << "There are one or more non-unique status messages.";
}

class CrosFpUpdaterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    DefaultValue<bool>::Set(true);
    // Lay down default rules to ensure an error is logged if an interface
    // if called without explicitly specifying it.
    EXPECT_CALL(dev_update_, GetVersion(_)).Times(0);
    EXPECT_CALL(dev_update_, IsFlashProtectEnabled(_)).Times(0);
    EXPECT_CALL(dev_update_, Flash(_, _)).Times(0);
    EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash()).Times(0);
    EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  }

  void TearDown() override { DefaultValue<bool>::Clear(); }

  // Setup an environment where a device GetVersion and IsFlashProtected
  // always succeed and report preset values corresponding to a preset
  // mock firmware.
  void SetupEnvironment(bool flash_protect,
                        bool ro_mismatch,
                        bool rw_mismatch,
                        enum ec_current_image ec_image = EC_IMAGE_RW) {
    CrosFpFirmware::ImageVersion img_ver = {kTestImageROVersion,
                                            kTestImageRWVersion};
    biod::CrosFpDevice::EcVersion ec_ver = {kTestImageROVersion,
                                            kTestImageRWVersion, ec_image};

    if (ro_mismatch) {
      img_ver.ro_version += "NEW";
    }
    if (rw_mismatch) {
      img_ver.rw_version += "NEW";
    }
    fw_.SetMockFwVersion(img_ver);

    EXPECT_CALL(dev_update_, GetVersion(NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(ec_ver), Return(true)));
    EXPECT_CALL(dev_update_, IsFlashProtectEnabled(NotNull()))
        .WillOnce(DoAll(SetArgPointee<0>(flash_protect), Return(true)));
  }

  UpdateStatus RunUpdater() { return DoUpdate(dev_update_, boot_ctrl_, fw_); }

  CrosFpUpdaterTest() = default;
  ~CrosFpUpdaterTest() override = default;

  MockCrosFpDeviceUpdate dev_update_;
  MockCrosFpBootUpdateCtrl boot_ctrl_;
  MockCrosFpFirmware fw_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosFpUpdaterTest);
};

// EcCurrentImageToString Tests

TEST(CrosFpDeviceUpdateTest, NonblankEcCurrentImageString) {
  // Given a EC Image enumeration
  for (auto image : kEcCurrentImageEnums) {
    // when we ask for the human readable string
    std::string msg = CrosFpDeviceUpdate::EcCurrentImageToString(image);
    // expect it to not be "".
    EXPECT_FALSE(msg.empty()) << "Status " << static_cast<int>(image)
                              << " converts to a blank status string.";
  }
}

TEST(CrosFpDeviceUpdateTest, UniqueEcCurrentImageString) {
  // Given a set of EC Image enumeration strings
  std::unordered_set<std::string> status_msgs;
  for (auto image : kEcCurrentImageEnums) {
    status_msgs.insert(CrosFpDeviceUpdate::EcCurrentImageToString(image));
  }

  // expect the set to contain the same number of unique strings
  // as there are original ec image enumerations.
  EXPECT_EQ(status_msgs.size(), kEcCurrentImageEnums.size())
      << "There are one or more non-unique ec image strings.";
}

// DoUpdate Tests

// Failure code paths

TEST_F(CrosFpUpdaterTest, GetDeviceVersionFails) {
  // Given a device which fails to report its version,
  EXPECT_CALL(dev_update_, GetVersion(NotNull())).WillOnce(Return(false));

  // expect the updater to report a failure.
  EXPECT_EQ(DoUpdate(dev_update_, boot_ctrl_, fw_),
            UpdateStatus::kUpdateFailed);
}

TEST_F(CrosFpUpdaterTest, GetFlashProtectFails) {
  // Given a device which reports its version, but fails to
  // report its flash protect status,
  EXPECT_CALL(dev_update_, GetVersion(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(dev_update_, IsFlashProtectEnabled(NotNull()))
      .WillOnce(Return(false));

  // expect the updater to report a failure.
  EXPECT_EQ(DoUpdate(dev_update_, boot_ctrl_, fw_),
            UpdateStatus::kUpdateFailed);
}

TEST_F(CrosFpUpdaterTest, FPDisabled_ROMismatch_ROUpdateFail) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is disabled,
      false,
      // RO needs to be updated,
      true, false);
  // and flashing operations fail,
  ON_CALL(dev_update_, Flash(_, _)).WillByDefault(Return(false));

  // expect the boot splash to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash());
  // but no reboot requested (avoid boot loop),
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  // an attempted RO flash,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RO));
  // and the updater to have reported a failure.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateFailed);
}

TEST_F(CrosFpUpdaterTest, FPDisabled_RORWMismatch_ROUpdateFail) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is disabled,
      false,
      // RO needs to be updated,
      true,
      // RW needs to be updated,
      true);
  // flashing operations fail,
  ON_CALL(dev_update_, Flash(_, _)).WillByDefault(Return(false));

  // expect the boot splash to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash());
  // but no reboot requested (avoid boot loop),
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  // an attempted RO flash (but no RW flash),
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RO));
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RW)).Times(0);
  // and the updater to report a failure.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateFailed);
}

TEST_F(CrosFpUpdaterTest, FPEnabled_RWMismatch_RWUpdateFail) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is enabled,
      true, false,
      // RW needs to be updated,
      true);
  // flashing operations fail,
  ON_CALL(dev_update_, Flash(_, _)).WillByDefault(Return(false));

  // expect the boot splash to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash());
  // but no reboot requested (avoid boot loop),
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  // an attempted RW flash,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RW));
  // and the updater to report a failure.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateFailed);
}

TEST_F(CrosFpUpdaterTest, FPDisabled_RORWMismatch_BootCtrlsBothFail) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is disabled,
      false,
      // RO needs to be updated,
      true,
      // RW needs to be updated,
      true);
  // both boot control functions fail,
  ON_CALL(boot_ctrl_, TriggerBootUpdateSplash()).WillByDefault(Return(false));
  ON_CALL(boot_ctrl_, ScheduleReboot()).WillByDefault(Return(false));

  // expect both boot control functions to be attempted,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash()).Times(AtLeast(1));
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(AtLeast(1));
  // both firmware images to be flashed,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RW));
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RO));
  // and the updater to report a success.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateSucceeded);
}

// Abnormal code paths

TEST_F(CrosFpUpdaterTest, CurrentROImage_RORWMatch_UpdateRW) {
  // Given an environment where
  SetupEnvironment(true, false, false,
                   // the current boot is stuck in RO,
                   EC_IMAGE_RO);

  // expect both boot controls to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash());
  EXPECT_CALL(boot_ctrl_, ScheduleReboot());
  // an attempted RW flash,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RW));
  // and the updater to report a success.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateSucceeded);
}

// Normal code paths

TEST_F(CrosFpUpdaterTest, FPDisabled_RORWMatch_NoUpdate) {
  // Given an environment where no updates are necessary
  SetupEnvironment(
      // and flash-protect is disabled,
      false, false, false);

  // expect neither boot control functions to be attempted,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash()).Times(0);
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  // no firmware images flashed,
  EXPECT_CALL(dev_update_, Flash(_, _)).Times(0);
  // and the updater to report an update not necessary.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateNotNecessary);
}

TEST_F(CrosFpUpdaterTest, FPEnabled_RORWMatch_NoUpdate) {
  // Given an environment where no updates are necessary
  SetupEnvironment(
      // and flash-protect is enabled,
      true, false, false);

  // expect neither boot control functions to be attempted,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash()).Times(0);
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  // no firmware images flashed,
  EXPECT_CALL(dev_update_, Flash(_, _)).Times(0);
  // and the updater to report an update not necessary.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateNotNecessary);
}

TEST_F(CrosFpUpdaterTest, FPEnabled_ROMismatch_NoUpdate) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is enabled
      true,
      // and RO needs to be updated,
      true, false);

  // expect neither boot control functions to be attempted,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash()).Times(0);
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(0);
  // no firmware images flashed,
  EXPECT_CALL(dev_update_, Flash(_, _)).Times(0);
  // and the updater to report an update not necessary.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateNotNecessary);
}

TEST_F(CrosFpUpdaterTest, RWMismatch_UpdateRW) {
  // Given an environment where
  SetupEnvironment(true, false,
                   // RW needs to be updated,
                   true);

  // expect both boot control functions to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash());
  EXPECT_CALL(boot_ctrl_, ScheduleReboot());
  // RW to be flashed,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RW));
  // and the updater to report a success.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateSucceeded);
}

TEST_F(CrosFpUpdaterTest, FPDisabled_ROMismatch_UpdateRO) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is disabled
      false,
      // and RO needs to be updated,
      true, false);

  // expect both boot control functions to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash());
  EXPECT_CALL(boot_ctrl_, ScheduleReboot());
  // RO to be flashed,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RO));
  // and the updater to report a success.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateSucceeded);
}

TEST_F(CrosFpUpdaterTest, FPDisabled_RORWMismatch_UpdateRORW) {
  // Given an environment where
  SetupEnvironment(
      // flash-protect is disabled,
      false,
      // RO needs to be updated,
      true,
      // RW needs to be updated,
      true);

  // expect both boot control functions to be triggered,
  EXPECT_CALL(boot_ctrl_, TriggerBootUpdateSplash()).Times(AtLeast(1));
  EXPECT_CALL(boot_ctrl_, ScheduleReboot()).Times(AtLeast(1));
  // both firmware images to be flashed,
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RO));
  EXPECT_CALL(dev_update_, Flash(Ref(fw_), EC_IMAGE_RW));
  // and the updater to report a success.
  EXPECT_EQ(RunUpdater(), UpdateStatus::kUpdateSucceeded);
}

}  // namespace updater

}  // namespace biod
