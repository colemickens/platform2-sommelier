// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/modem_flasher.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <gtest/gtest.h>

#include "modemfwd/firmware_directory_stub.h"
#include "modemfwd/mock_modem.h"
#include "modemfwd/mock_modem_helper.h"
#include "modemfwd/modem_helper_directory_stub.h"

using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

namespace modemfwd {

namespace {

constexpr char kDeviceId1[] = "device:id:1";
constexpr char kEquipmentId1[] = "equipment_id_1";

constexpr char kMainFirmware1Path[] = "main_fw_1.fls";
constexpr char kMainFirmware1Version[] = "versionA";

constexpr char kMainFirmware2Path[] = "main_fw_2.fls";
constexpr char kMainFirmware2Version[] = "versionB";

constexpr char kCarrier1[] = "uuid_1";
constexpr char kCarrier1Firmware1Path[] = "carrier_1_fw_1.fls";
constexpr char kCarrier1Firmware1Version[] = "v1.00";
constexpr char kCarrier1Firmware2Path[] = "carrier_1_fw_2.fls";
constexpr char kCarrier1Firmware2Version[] = "v1.10";

constexpr char kCarrier2[] = "uuid_2";
constexpr char kCarrier2Firmware1Path[] = "carrier_2_fw_1.fls";
constexpr char kCarrier2Firmware1Version[] = "4500.15.65";

constexpr char kGenericCarrierFirmware1Path[] = "generic_fw_1.fls";
constexpr char kGenericCarrierFirmware1Version[] = "2017-10-13";
constexpr char kGenericCarrierFirmware2Path[] = "generic_fw_2.fls";
constexpr char kGenericCarrierFirmware2Version[] = "2017-10-14";

}  // namespace

class ModemFlasherTest : public ::testing::Test {
 public:
  ModemFlasherTest() {
    auto helper_directory = std::make_unique<ModemHelperDirectoryStub>();
    helper_directory_ = helper_directory.get();
    auto firmware_directory = std::make_unique<FirmwareDirectoryStub>();
    firmware_directory_ = firmware_directory.get();

    modem_flasher_ = std::make_unique<ModemFlasher>(
        std::move(helper_directory), std::move(firmware_directory));
  }

 protected:
  void AddHelper(const std::string& device_id) {
    auto helper = std::make_unique<MockModemHelper>();
    ON_CALL(*helper.get(), GetCarrierFirmwareInfo(_))
        .WillByDefault(Return(false));
    helper_directory_->AddHelper(device_id, std::move(helper));
  }

  void SetCarrierFirmwareInfo(const std::string& device_id,
                              const std::string& carrier_name,
                              const std::string& version) {
    CarrierFirmwareInfo info(carrier_name, version);
    ON_CALL(*GetHelper(device_id), GetCarrierFirmwareInfo(_))
        .WillByDefault(DoAll(SetArgPointee<0>(info), Return(true)));
  }

  void ExpectNotToReadCarrierFirmwareInfo(const std::string& device_id) {
    EXPECT_CALL(*GetHelper(device_id), GetCarrierFirmwareInfo(_)).Times(0);
  }

  void ExpectNotToFlashMain(const std::string& device_id) {
    EXPECT_CALL(*GetHelper(device_id), FlashMainFirmware(_)).Times(0);
  }

  void ExpectToFlashMain(const std::string& device_id,
                         const base::FilePath& firmware_path,
                         bool success) {
    EXPECT_CALL(*GetHelper(device_id), FlashMainFirmware(firmware_path))
        .WillOnce(Return(success));
  }

  void ExpectNotToFlashCarrier(const std::string& device_id) {
    EXPECT_CALL(*GetHelper(device_id), FlashCarrierFirmware(_)).Times(0);
  }

  void ExpectToFlashCarrier(const std::string& device_id,
                            const base::FilePath& firmware_path,
                            bool success) {
    EXPECT_CALL(*GetHelper(device_id), FlashCarrierFirmware(firmware_path))
        .WillOnce(Return(success));
  }

  bool ClearHelperExpectations(const std::string& device_id) {
    return Mock::VerifyAndClearExpectations(GetHelper(device_id));
  }

  void AddMainFirmwareFile(const std::string& device_id,
                           const base::FilePath& firmware_path,
                           const std::string& version) {
    FirmwareFileInfo firmware_info(firmware_path, version);
    firmware_directory_->AddMainFirmware(kDeviceId1, firmware_info);
  }

  void AddCarrierFirmwareFile(const std::string& device_id,
                              const std::string& carrier_name,
                              const base::FilePath& firmware_path,
                              const std::string& version) {
    FirmwareFileInfo firmware_info(firmware_path, version);
    firmware_directory_->AddCarrierFirmware(
        kDeviceId1, carrier_name, firmware_info);
  }

  std::unique_ptr<MockModem> GetDefaultModem() {
    auto modem = std::make_unique<MockModem>();
    ON_CALL(*modem.get(), GetDeviceId()).WillByDefault(Return(kDeviceId1));
    ON_CALL(*modem.get(), GetEquipmentId())
        .WillByDefault(Return(kEquipmentId1));
    ON_CALL(*modem.get(), GetCarrierId()).WillByDefault(Return(kCarrier1));
    ON_CALL(*modem.get(), GetMainFirmwareVersion())
        .WillByDefault(Return(kMainFirmware1Version));

    // Since the equipment ID is the main identifier we should always expect
    // to want to know what it is.
    EXPECT_CALL(*modem.get(), GetEquipmentId()).Times(AtLeast(1));
    return modem;
  }

  std::unique_ptr<ModemFlasher> modem_flasher_;

 private:
  MockModemHelper* GetHelper(const std::string& device_id) {
    MockModemHelper* helper = static_cast<MockModemHelper*>(
      helper_directory_->GetHelperForDeviceId(device_id));
    CHECK(helper);
    return helper;
  }

  // We pass these off to |modem_flasher_| but keep references to
  // them to ensure we can set up the stub outputs.
  ModemHelperDirectoryStub* helper_directory_;
  FirmwareDirectoryStub* firmware_directory_;
};

TEST_F(ModemFlasherTest, NothingToFlash) {
  AddHelper(kDeviceId1);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, FlashMainFirmware) {
  base::FilePath new_firmware(kMainFirmware2Path);
  AddMainFirmwareFile(kDeviceId1, new_firmware, kMainFirmware2Version);

  AddHelper(kDeviceId1);
  ExpectToFlashMain(kDeviceId1, new_firmware, true);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, SkipSameMainVersion) {
  base::FilePath firmware(kMainFirmware1Path);
  AddMainFirmwareFile(kDeviceId1, firmware, kMainFirmware1Version);

  AddHelper(kDeviceId1);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, UpgradeCarrierFirmware) {
  base::FilePath new_firmware(kCarrier1Firmware2Path);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier1, new_firmware, kCarrier1Firmware2Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, new_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, SwitchCarrierFirmwareForSimHotSwap) {
  base::FilePath original_firmware(kCarrier1Firmware1Path);
  base::FilePath other_firmware(kCarrier2Firmware1Path);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier1, original_firmware, kCarrier1Firmware1Version);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier2, other_firmware, kCarrier2Firmware1Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, other_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId())
      .Times(AtLeast(1)).WillRepeatedly(Return(kCarrier2));
  modem_flasher_->TryFlash(modem.get());

  // After the modem reboots, the helper hopefully reports the new carrier.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier2, kCarrier2Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem_flasher_->TryFlash(modem.get());

  // Suppose we swap the SIM back to the first one. Then we should try to
  // flash the first firmware again.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier2, kCarrier2Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, original_firmware, true);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, BlacklistAfterMainFlashFailure) {
  base::FilePath new_firmware(kMainFirmware2Path);
  AddMainFirmwareFile(kDeviceId1, new_firmware, kMainFirmware2Version);

  AddHelper(kDeviceId1);
  ExpectToFlashMain(kDeviceId1, new_firmware, false);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // Here the modem would reboot, but ModemFlasher should keep track of its
  // IMEI and ensure we don't even check the main firmware version or
  // carrier.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(0);
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(0);
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(0);
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, BlacklistAfterCarrierFlashFailure) {
  base::FilePath new_firmware(kCarrier1Firmware2Path);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier1, new_firmware, kCarrier1Firmware2Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, new_firmware, false);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  ExpectNotToReadCarrierFirmwareInfo(kDeviceId1);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(0);
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(0);
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(0);
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, SuccessThenBlacklist) {
  const base::FilePath new_main_firmware(kMainFirmware2Path);
  const base::FilePath new_carrier_firmware(kCarrier1Firmware2Path);
  AddMainFirmwareFile(kDeviceId1, new_main_firmware, kMainFirmware2Version);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier1, new_carrier_firmware, kCarrier1Firmware2Version);

  // Successfully flash the main firmware.
  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware1Version);
  ExpectToFlashMain(kDeviceId1, new_main_firmware, true);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // Fail to flash the carrier firmware.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, new_carrier_firmware, false);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // Check that we're still blacklisted even though we saw a successful flash
  // earlier.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  ExpectNotToReadCarrierFirmwareInfo(kDeviceId1);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(0);
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(0);
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(0);
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, RefuseToFlashMainFirmwareTwice) {
  base::FilePath new_firmware(kMainFirmware2Path);
  AddMainFirmwareFile(kDeviceId1, new_firmware, kMainFirmware2Version);

  AddHelper(kDeviceId1);
  ExpectToFlashMain(kDeviceId1, new_firmware, true);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // We've had issues in the past where the firmware version is updated
  // but the modem still reports the old version string. Refuse to flash
  // the main firmware twice because that should never be correct behavior
  // in one session and so that we don't constantly try to flash the
  // main firmware over and over.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetMainFirmwareVersion()).Times(0);
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, RefuseToFlashCarrierFirmwareTwice) {
  base::FilePath new_firmware(kCarrier1Firmware2Path);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier1, new_firmware, kCarrier1Firmware2Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, new_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // Assume the carrier firmware doesn't have an updated version string in it,
  // i.e. the ModemHelper will return the old string.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, RefuseToReflashCarrierAcrossHotSwap) {
  // Upgrade carrier firmware.
  base::FilePath new_firmware(kCarrier1Firmware2Path);
  AddCarrierFirmwareFile(
      kDeviceId1, kCarrier1, new_firmware, kCarrier1Firmware2Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, new_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // Switch carriers, but there won't be firmware for the new one.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier1, kCarrier1Firmware2Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId())
      .Times(AtLeast(1)).WillRepeatedly(Return(kCarrier2));
  modem_flasher_->TryFlash(modem.get());

  // Suppose we swap the SIM back to the first one. We should not flash
  // firmware that we already know we successfully flashed.
  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, UpgradeGenericFirmware) {
  base::FilePath new_firmware(kGenericCarrierFirmware2Path);
  AddCarrierFirmwareFile(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         new_firmware,
                         kGenericCarrierFirmware2Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         kGenericCarrierFirmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, new_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, SkipSameGenericFirmware) {
  base::FilePath generic_firmware(kGenericCarrierFirmware1Path);
  AddCarrierFirmwareFile(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         generic_firmware,
                         kGenericCarrierFirmware1Version);

  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         kGenericCarrierFirmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, TwoCarriersUsingGenericFirmware) {
  base::FilePath generic_firmware(kGenericCarrierFirmware1Path);
  AddCarrierFirmwareFile(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         generic_firmware,
                         kGenericCarrierFirmware1Version);

  AddHelper(kDeviceId1);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, generic_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());

  // When we try to flash again and the modem reports a different carrier,
  // we should expect that the ModemFlasher refuses to flash the same firmware,
  // since there is generic firmware and no carrier has its own firmware.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  SetCarrierFirmwareInfo(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         kGenericCarrierFirmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

TEST_F(ModemFlasherTest, HotSwapWithGenericFirmware) {
  base::FilePath original_firmware(kGenericCarrierFirmware1Path);
  base::FilePath other_firmware(kCarrier2Firmware1Path);
  AddCarrierFirmwareFile(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         original_firmware,
                         kGenericCarrierFirmware1Version);
  AddCarrierFirmwareFile(kDeviceId1,
                         kCarrier2,
                         other_firmware,
                         kCarrier2Firmware1Version);

  // Even though there is generic firmware, we should try to use specific
  // ones first if they exist.
  AddHelper(kDeviceId1);
  SetCarrierFirmwareInfo(kDeviceId1,
                         FirmwareDirectory::kGenericCarrierId,
                         kGenericCarrierFirmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, other_firmware, true);

  auto modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId())
      .Times(AtLeast(1)).WillRepeatedly(Return(kCarrier2));
  modem_flasher_->TryFlash(modem.get());

  // Reboot the modem.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier2, kCarrier2Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectNotToFlashCarrier(kDeviceId1);

  modem_flasher_->TryFlash(modem.get());

  // Suppose we swap the SIM back to the first one. Then we should try to
  // flash the generic firmware again.
  ASSERT_TRUE(ClearHelperExpectations(kDeviceId1));
  SetCarrierFirmwareInfo(kDeviceId1, kCarrier2, kCarrier2Firmware1Version);
  ExpectNotToFlashMain(kDeviceId1);
  ExpectToFlashCarrier(kDeviceId1, original_firmware, true);

  modem = GetDefaultModem();
  EXPECT_CALL(*modem.get(), GetDeviceId()).Times(AtLeast(1));
  EXPECT_CALL(*modem.get(), GetCarrierId()).Times(AtLeast(1));
  modem_flasher_->TryFlash(modem.get());
}

}  // namespace modemfwd
