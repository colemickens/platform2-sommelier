// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/firmware_directory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace {

constexpr char kDeviceId[] = "device-id";

constexpr char kMainFirmwareFile[] = "NAND_10.20.30.40.fls";
constexpr char kMainFirmwareVersion[] = "10.20.30.40";

constexpr char kCarrierA[] = "CarrierA";
constexpr char kCarrierFirmwareFile1[] = "CarrierA_40.30.20.10.fls";
constexpr char kCarrierFirmwareVersion1[] = "40.30.20.10";

constexpr char kCarrierB[] = "CarrierB";
constexpr char kCarrierFirmwareFile2[] = "Custom_B_50_60.fls";
constexpr char kCarrierFirmwareVersion2[] = "50.60.70.80";

constexpr char kCarrierC[] = "CarrierC";

constexpr char kGenericCarrierFirmwareFile[] = "Generic_V1.59.3.fls";
constexpr char kGenericCarrierFirmwareVersion[] = "V1.59.3";

}  // namespace

namespace modemfwd {

class FirmwareDirectoryTest : public ::testing::Test {
 public:
  FirmwareDirectoryTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }
  ~FirmwareDirectoryTest() override = default;

 protected:
  void SetUpDirectory(const base::FilePath& manifest) {
    base::FilePath manifest_in_dir =
        temp_dir_.GetPath().Append("firmware_manifest.prototxt");
    CHECK(base::CopyFile(manifest, manifest_in_dir));
    firmware_directory_ = CreateFirmwareDirectory(temp_dir_.GetPath());
    CHECK(firmware_directory_);
  }

  std::unique_ptr<FirmwareDirectory> firmware_directory_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(FirmwareDirectoryTest, FindFirmware) {
  const base::FilePath kManifest("test_protos/find_firmware.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  EXPECT_TRUE(firmware_directory_->FindMainFirmware(kDeviceId, &info));
  EXPECT_EQ(kMainFirmwareFile, info.firmware_path.BaseName().value());
  EXPECT_EQ(kMainFirmwareVersion, info.version);
  std::string carrier_a(kCarrierA);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
  EXPECT_EQ(kCarrierA, carrier_a);
  EXPECT_EQ(kCarrierFirmwareFile1, info.firmware_path.BaseName().value());
  EXPECT_EQ(kCarrierFirmwareVersion1, info.version);
}

TEST_F(FirmwareDirectoryTest, NoFirmwareForDevice) {
  const base::FilePath kManifest("/dev/null");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  EXPECT_FALSE(firmware_directory_->FindMainFirmware(kDeviceId, &info));
  std::string carrier_a(kCarrierA);
  EXPECT_FALSE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
}

TEST_F(FirmwareDirectoryTest, FirmwareForDifferentCarrier) {
  const base::FilePath kManifest(
      "test_protos/firmware_for_different_carrier.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  std::string carrier_b(kCarrierB);
  EXPECT_FALSE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_b, &info));
}

TEST_F(FirmwareDirectoryTest, FirmwareForDifferentDevice) {
  const base::FilePath kManifest(
      "test_protos/firmware_for_different_device.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  EXPECT_FALSE(firmware_directory_->FindMainFirmware(kDeviceId, &info));
}

TEST_F(FirmwareDirectoryTest, MultipleCarrierFirmware) {
  const base::FilePath kManifest(
      "test_protos/multiple_carrier_firmware.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  std::string carrier_a(kCarrierA);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
  EXPECT_EQ(kCarrierA, carrier_a);
  EXPECT_EQ(kCarrierFirmwareFile1, info.firmware_path.BaseName().value());
  EXPECT_EQ(kCarrierFirmwareVersion1, info.version);

  std::string carrier_b(kCarrierB);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_b, &info));
  EXPECT_EQ(kCarrierB, carrier_b);
  EXPECT_EQ(kCarrierFirmwareFile2, info.firmware_path.BaseName().value());
  EXPECT_EQ(kCarrierFirmwareVersion2, info.version);
}

TEST_F(FirmwareDirectoryTest, GenericFirmware) {
  const base::FilePath kManifest("test_protos/generic_firmware.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  std::string carrier_a(kCarrierA);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
  EXPECT_EQ(FirmwareDirectory::kGenericCarrierId, carrier_a);
  EXPECT_EQ(kGenericCarrierFirmwareFile, info.firmware_path.BaseName().value());
  EXPECT_EQ(kGenericCarrierFirmwareVersion, info.version);
}

TEST_F(FirmwareDirectoryTest, SpecificBeforeGeneric) {
  const base::FilePath kManifest(
      "test_protos/specific_before_generic.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  std::string carrier_a(kCarrierA);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
  EXPECT_EQ(kCarrierA, carrier_a);
  EXPECT_EQ(kCarrierFirmwareFile1, info.firmware_path.BaseName().value());
  EXPECT_EQ(kCarrierFirmwareVersion1, info.version);
}

TEST_F(FirmwareDirectoryTest, FirmwareSupportsTwoCarriers) {
  const base::FilePath kManifest("test_protos/two_carrier_firmware.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  std::string carrier_a(kCarrierA);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
  EXPECT_EQ(kCarrierA, carrier_a);
  EXPECT_EQ(kCarrierFirmwareFile1, info.firmware_path.BaseName().value());
  EXPECT_EQ(kCarrierFirmwareVersion1, info.version);

  std::string carrier_b(kCarrierB);
  EXPECT_TRUE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_b, &info));
  EXPECT_EQ(kCarrierB, carrier_b);
  EXPECT_EQ(kCarrierFirmwareFile1, info.firmware_path.BaseName().value());
  EXPECT_EQ(kCarrierFirmwareVersion1, info.version);

  std::string carrier_c(kCarrierC);
  EXPECT_FALSE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_c, &info));
}

TEST_F(FirmwareDirectoryTest, MalformedMainEntry) {
  const base::FilePath kManifest(
      "test_protos/malformed_main_firmware.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  EXPECT_FALSE(firmware_directory_->FindMainFirmware(kDeviceId, &info));
}

TEST_F(FirmwareDirectoryTest, MalformedCarrierEntry) {
  const base::FilePath kManifest(
      "test_protos/malformed_carrier_firmware.prototxt");
  SetUpDirectory(kManifest);

  FirmwareFileInfo info;
  std::string carrier_a(kCarrierA);
  EXPECT_FALSE(
      firmware_directory_->FindCarrierFirmware(kDeviceId, &carrier_a, &info));
}

}  // namespace modemfwd
