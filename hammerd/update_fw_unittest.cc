// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/rand_util.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vboot/vb21_struct.h>

#include "hammerd/fmap_utils.h"
#include "hammerd/mock_fmap_utils.h"
#include "hammerd/mock_usb_utils.h"
#include "hammerd/update_fw.h"
#include "hammerd/usb_utils.h"

using testing::_;
using testing::Invoke;
using testing::InSequence;
using testing::AtLeast;
using testing::AnyNumber;
using testing::Return;
using testing::ReturnArg;

namespace hammerd {

class FirmwareUpdaterTest : public testing::Test {
 public:
  void SetUp() override {
    fw_updater_.reset(
        new FirmwareUpdater{
            std::unique_ptr<UsbEndpoint>(new MockUsbEndpoint()),
            std::unique_ptr<FmapInterface>(new MockFmap())});
    uep_ = static_cast<MockUsbEndpoint*>(fw_updater_->uep_.get());
    fmap_ = static_cast<MockFmap*>(fw_updater_->fmap_.get());

    good_rpdu_.return_value = htobe32(0);
    good_rpdu_.header_type =
        htobe16(static_cast<int>(FirstResponsePDUHeaderType::kCommon));
    good_rpdu_.protocol_version = htobe16(6);
    good_rpdu_.maximum_pdu_size = htobe32(128);
    good_rpdu_.flash_protection = htobe32(0);
    good_rpdu_.offset = htobe32(0x11000);
    snprintf(good_rpdu_.version, sizeof(good_rpdu_.version), "MOCK VERSION");
    good_rpdu_.min_rollback = htobe32(0);
    good_rpdu_.key_version = htobe32(1);

    first_header_ = BuildHeaderData(sizeof(UpdateFrameHeader), 0, 0);
    uint32_t cmd = htobe32(kUpdateDoneCmd);
    done_cmd_ =
        ConvertData(reinterpret_cast<uint8_t*>(&cmd), sizeof(kUpdateDoneCmd));
  }

  std::vector<uint8_t> BuildHeaderData(uint32_t size,
                                       uint32_t digest,
                                       uint32_t base) {
    UpdateFrameHeader ufh{size, digest, base};
    uint8_t* ufh_ptr = reinterpret_cast<uint8_t*>(&ufh);
    return std::vector<uint8_t>(ufh_ptr, ufh_ptr + sizeof(UpdateFrameHeader));
  }

  std::vector<uint8_t> ConvertData(uint8_t* ptr, size_t len) {
    return std::vector<uint8_t>(ptr, ptr + len);
  }

 protected:
  std::unique_ptr<FirmwareUpdater> fw_updater_;
  MockUsbEndpoint* uep_;
  MockFmap* fmap_;

  // Good response of first header.
  FirstResponsePDU good_rpdu_;
  // Zero-size header:
  std::vector<uint8_t> first_header_;
  std::vector<uint8_t> done_cmd_;
};

// Load a fake image with each value. The image contains:
// - Fake header: 5 bytes
// - mock fmap: sizeof(fmap) bytes
// - RO version string: 32 bytes
// - RW version string: 32 bytes
// - RW rollback version: 4 bytes
// - RO key: sizeof(vb21_packed_key) bytes
TEST_F(FirmwareUpdaterTest, LoadImage) {
  // Build a fake image.
  std::string image("12345");
  int64_t mock_offset = image.size();
  fmap mock_fmap;
  mock_fmap.size = 5 + sizeof(fmap) + 32 + 32 + 4 + sizeof(vb21_packed_key);
  image.append(reinterpret_cast<char*>(&mock_fmap), sizeof(mock_fmap));

  int64_t ro_version_offset = image.size();
  char ro_version[32] = "RO MOCK VERSION";
  image.append(ro_version, 32);

  int64_t rw_version_offset = image.size();
  char rw_version[32] = "RW MOCK VERSION";
  image.append(rw_version, 32);

  int64_t rw_rollback_offset = image.size();
  int32_t rw_rollback = 35;
  image.append(reinterpret_cast<char*>(&rw_rollback), sizeof(rw_rollback));

  int64_t ro_key_offset = image.size();
  vb21_packed_key ro_key;
  ro_key.key_version = 1;
  image.append(reinterpret_cast<char*>(&ro_key), sizeof(ro_key));

  size_t image_len = image.size();
  ASSERT_EQ(image_len, mock_fmap.size);

  const fmap* mock_fmap_ptr = reinterpret_cast<const fmap*>(
      reinterpret_cast<const uint8_t*>(image.data()) + mock_offset);

  EXPECT_CALL(*fmap_, Find(_, image_len)).WillOnce(Return(mock_offset));
  // Find RO section.
  fmap_area ro_section_area;
  ro_section_area.offset = 0x0;
  ro_section_area.size = 0x10000;
  EXPECT_CALL(*fmap_, FindArea(mock_fmap_ptr, "EC_RO"))
      .WillOnce(Return(&ro_section_area));
  // Find RO version.
  fmap_area ro_version_area;
  ro_version_area.offset = ro_version_offset;
  ro_version_area.size = 32;
  EXPECT_CALL(*fmap_, FindArea(mock_fmap_ptr, "RO_FRID"))
      .WillOnce(Return(&ro_version_area));
  // Find RO key.
  fmap_area ro_key_area;
  ro_key_area.offset = ro_key_offset;
  EXPECT_CALL(*fmap_, FindArea(mock_fmap_ptr, "KEY_RO"))
      .WillOnce(Return(&ro_key_area));
  // Find RW section.
  fmap_area rw_section_area;
  rw_section_area.offset = 0x11000;
  rw_section_area.size = 0xA0;
  EXPECT_CALL(*fmap_, FindArea(mock_fmap_ptr, "EC_RW"))
      .WillOnce(Return(&rw_section_area));
  // Find RW version.
  fmap_area rw_version_area;
  rw_version_area.offset = rw_version_offset;
  rw_version_area.size = 32;
  EXPECT_CALL(*fmap_, FindArea(mock_fmap_ptr, "RW_FWID"))
      .WillOnce(Return(&rw_version_area));
  // Find RW rollback version.
  fmap_area rw_rollback_area;
  rw_rollback_area.offset = rw_rollback_offset;
  rw_rollback_area.size = 4;
  EXPECT_CALL(*fmap_, FindArea(mock_fmap_ptr, "RW_RBVER"))
      .WillOnce(Return(&rw_rollback_area));

  ASSERT_EQ(fw_updater_->LoadImage(image), true);
  ASSERT_EQ(fw_updater_->image_, image);
  ASSERT_EQ(
      fw_updater_->sections_[0],
      SectionInfo(SectionName::RO, 0x0, 0x10000, "RO MOCK VERSION", -1, -1));
  ASSERT_EQ(
      fw_updater_->sections_[1],
      SectionInfo(SectionName::RW, 0x11000, 0xA0, "RW MOCK VERSION", 35, 1));
}

// Returns a helper function that returns true after a period of time.
std::function<bool()> TrueAfterPeriod(base::Time start, int64_t period_ms) {
  return [=]() {
    auto diff = (base::Time::Now() - start).InMilliseconds();
    return (diff >= period_ms);
  };
}

// USB endpoint is ready to connect after 500 ms.
TEST_F(FirmwareUpdaterTest, TryConnectUSB_OK) {
  InSequence dummy;
  auto now = base::Time::Now();
  ON_CALL(*uep_, Connect()).WillByDefault(Invoke(TrueAfterPeriod(now, 500)));
  EXPECT_CALL(*uep_, Connect()).Times(AtLeast(1));
  EXPECT_CALL(*uep_, GetChunkLength()).WillOnce(Return(0x40));
  EXPECT_CALL(*uep_, Receive(_, 0x40, true, _)).WillOnce(Return(-1));
  EXPECT_CALL(*uep_, GetConfigurationString())
      .WillOnce(Return("RO:version_string"));
  ASSERT_EQ(fw_updater_->TryConnectUSB(), true);
  ASSERT_EQ(fw_updater_->version_, "version_string");
}

// USB endpoint is ready to connect after 5000 ms, which is longer than timeout.
TEST_F(FirmwareUpdaterTest, TryConnectUSB_FAIL) {
  InSequence dummy;
  auto now = base::Time::Now();
  ON_CALL(*uep_, Connect()).WillByDefault(Invoke(TrueAfterPeriod(now, 5000)));
  EXPECT_CALL(*uep_, Connect()).Times(AtLeast(1));
  EXPECT_CALL(*uep_, GetConfigurationString()).Times(0);
  ASSERT_EQ(fw_updater_->TryConnectUSB(), false);
}

// Test legacy-style version string.
TEST_F(FirmwareUpdaterTest, TryConnectUSB_FetchVersion_Legacy) {
  InSequence dummy;
  EXPECT_CALL(*uep_, Connect()).WillOnce(Return(true));
  EXPECT_CALL(*uep_, GetConfigurationString()).WillOnce(Return("version_string"));
  ASSERT_EQ(fw_updater_->TryConnectUSB(), true);
  ASSERT_EQ(fw_updater_->version_, "version_string");
}

// Parse the given invalid configuration string descriptor.
TEST_F(FirmwareUpdaterTest, TryConnectUSB_FetchVersion_FAIL) {
  InSequence dummy;
  EXPECT_CALL(*uep_, Connect()).WillOnce(Return(true));
  EXPECT_CALL(*uep_, GetChunkLength()).WillOnce(Return(0x40));
  EXPECT_CALL(*uep_, Receive(_, 0x40, true, _)).WillOnce(Return(-1));
  EXPECT_CALL(*uep_, GetConfigurationString()).WillOnce(Return(""));
  ASSERT_EQ(fw_updater_->TryConnectUSB(), false);
}

// Simulate leftover data on the EC's "out" buffer.
TEST_F(FirmwareUpdaterTest, TryConnectUSB_LeftoverData) {
  EXPECT_CALL(*uep_, Connect()).WillOnce(Return(true));
  EXPECT_CALL(*uep_, GetChunkLength()).WillOnce(Return(10));
  EXPECT_CALL(*uep_, Receive(_, 10, true, _)).Times(3)
                                             .WillOnce(Return(10))
                                             .WillOnce(Return(10))
                                             .WillOnce(Return(0));
  EXPECT_CALL(*uep_, GetConfigurationString())
      .WillOnce(Return("RO:version_string"));
  ASSERT_EQ(fw_updater_->TryConnectUSB(), true);
}

// Send done command.
TEST_F(FirmwareUpdaterTest, SendDone) {
  InSequence dummy;
  EXPECT_CALL(*uep_, SendHelper(done_cmd_, _, _)).WillOnce(ReturnArg<2>());
  EXPECT_CALL(*uep_, Receive(_, 1, false, _)).WillOnce(Return(1));
  fw_updater_->SendDone();
}

// Send first PDU and get the good response.
TEST_F(FirmwareUpdaterTest, SendFirstPDU) {
  InSequence dummy;
  EXPECT_CALL(*uep_, SendHelper(first_header_, _, _)).WillOnce(ReturnArg<2>());
  EXPECT_CALL(*uep_, Receive(_, sizeof(good_rpdu_), true, _))
      .WillOnce(WriteBuf(&good_rpdu_));

  ASSERT_EQ(fw_updater_->SendFirstPDU(), true);
}

// Send the kInjectEntropy subcommand.
TEST_F(FirmwareUpdaterTest, SendSubcommand_InjectEntropy) {
  // Build the header data.
  uint16_t subcommand =
      htobe16(static_cast<uint16_t>(UpdateExtraCommand::kInjectEntropy));
  std::string fake_entropy = base::RandBytesAsString(32);
  std::vector<uint8_t> sub_cmd_data =
      ConvertData(reinterpret_cast<uint8_t*>(&subcommand), sizeof(subcommand));
  std::vector<uint8_t> ufh_data;
  ufh_data = BuildHeaderData(
      sizeof(UpdateFrameHeader) + sizeof(subcommand) + fake_entropy.size(),
      0,
      kUpdateExtraCmd);
  ufh_data.insert(ufh_data.end(), sub_cmd_data.begin(), sub_cmd_data.end());
  ufh_data.insert(ufh_data.end(), fake_entropy.begin(), fake_entropy.end());

  ON_CALL(*uep_, SendHelper(_, _, _)).WillByDefault(ReturnArg<2>());
  ON_CALL(*uep_, Receive(_, 1, false, _)).WillByDefault(Return(1));
  {
    InSequence dummy;
    // Send the subcommand.
    EXPECT_CALL(*uep_, SendHelper(ufh_data, _, _));
    EXPECT_CALL(*uep_, Receive(_, 1, false, _));
  }

  ASSERT_EQ(fw_updater_->SendSubcommand(UpdateExtraCommand::kInjectEntropy,
                                        fake_entropy),
            true);
}

// Send the kImmediateReset subcommand.
TEST_F(FirmwareUpdaterTest, SendSubcommand_Reset) {
  // Build the header data.
  uint16_t subcommand =
      htobe16(static_cast<uint16_t>(UpdateExtraCommand::kImmediateReset));
  std::vector<uint8_t> sub_cmd_data =
      ConvertData(reinterpret_cast<uint8_t*>(&subcommand), sizeof(subcommand));
  std::vector<uint8_t> ufh_data;
  ufh_data = BuildHeaderData(
      sizeof(UpdateFrameHeader) + sizeof(subcommand), 0, kUpdateExtraCmd);
  ufh_data.insert(ufh_data.end(), sub_cmd_data.begin(), sub_cmd_data.end());

  ON_CALL(*uep_, SendHelper(_, _, _)).WillByDefault(ReturnArg<2>());
  ON_CALL(*uep_, Receive(_, 1, false, _)).WillByDefault(Return(1));
  {
    InSequence dummy;
    // Send subcommand. Because the hammer is reset after sending the command,
    // it won't reply the response.
    EXPECT_CALL(*uep_, SendHelper(ufh_data, _, _));
  }

  ASSERT_EQ(fw_updater_->SendSubcommand(UpdateExtraCommand::kImmediateReset),
            true);
}

TEST_F(FirmwareUpdaterTest, CurrentSection) {
  fw_updater_->sections_ = {
      SectionInfo(SectionName::RO, 0x0, 0x10000, "RO MOCK VERSION", -1, -1),
      SectionInfo(SectionName::RW, 0x11000, 0xA0, "RW MOCK VERSION", 35, 1)};

  // Writable offset is at RW, so current section is RO.
  fw_updater_->targ_.offset = 0x11000;
  ASSERT_EQ(fw_updater_->CurrentSection(), SectionName::RO);

  // Writable offset is at RO, so current section is RW.
  fw_updater_->targ_.offset = 0x0;
  ASSERT_EQ(fw_updater_->CurrentSection(), SectionName::RW);

  // Writable offset is not at RO nor RW, return Invalid.
  fw_updater_->targ_.offset = 0xffff;
  ASSERT_EQ(fw_updater_->CurrentSection(), SectionName::Invalid);
}

TEST_F(FirmwareUpdaterTest, NeedsUpdate) {
  fw_updater_->sections_ = {
      SectionInfo(SectionName::RO, 0x0, 0x10000, "RO MOCK VERSION", -1, -1),
      SectionInfo(SectionName::RW, 0x11000, 0xA0, "RW MOCK VERSION", 35, 1)};
  // Writable offset is at RW, so current section is RO.
  fw_updater_->targ_.offset = 0x11000;

  // A case that needs to update RW.
  snprintf(fw_updater_->targ_.version,
           sizeof(fw_updater_->targ_.version),
           "ANOTHER VERSION");
  fw_updater_->targ_.min_rollback = 35;
  fw_updater_->targ_.key_version = 1;
  ASSERT_EQ(fw_updater_->NeedsUpdate(SectionName::RW), true);

  // The key version is not the same.
  snprintf(fw_updater_->targ_.version,
           sizeof(fw_updater_->targ_.version),
           "ANOTHER VERSION");
  fw_updater_->targ_.min_rollback = 35;
  fw_updater_->targ_.key_version = 2;
  ASSERT_EQ(fw_updater_->NeedsUpdate(SectionName::RW), false);

  // Minimum rollback is larger than the updated image.
  snprintf(fw_updater_->targ_.version,
           sizeof(fw_updater_->targ_.version),
           "ANOTHER VERSION");
  fw_updater_->targ_.min_rollback = 40;
  fw_updater_->targ_.key_version = 1;
  ASSERT_EQ(fw_updater_->NeedsUpdate(SectionName::RW), false);

  // The version is the same.
  snprintf(fw_updater_->targ_.version,
           sizeof(fw_updater_->targ_.version),
           "%s",
           fw_updater_->sections_[1].version);
  fw_updater_->targ_.min_rollback = 35;
  fw_updater_->targ_.key_version = 1;
  ASSERT_EQ(fw_updater_->NeedsUpdate(SectionName::RW), false);
}

// Test to transfer RW section.
//   USB chunk size: 0x40
//   Maximum PDU size: 0x80
//   RW size: 0xA0
// Therefore it should send 3 packets with 0x40, 0x40, 0x20 bytes.
TEST_F(FirmwareUpdaterTest, TransferImage) {
  // Set the default action of mock USB endpoint.
  ON_CALL(*uep_, Connect()).WillByDefault(Return(true));
  ON_CALL(*uep_, GetChunkLength()).WillByDefault(Return(0x40));
  ON_CALL(*uep_, SendHelper(_, _, _)).WillByDefault(ReturnArg<2>());
  ON_CALL(*uep_, Receive(_, _, _, _)).WillByDefault(ReturnArg<1>());

  // Set the mock image data and section info.
  fw_updater_->image_ = std::string(0x11000 + 0xA0, 0);
  fw_updater_->sections_ = {
      SectionInfo(SectionName::RO, 0x0, 0x10000, "RO MOCK VERSION", -1, -1),
      SectionInfo(SectionName::RW, 0x11000, 0xA0, "RW MOCK VERSION", 35, 1)};
  // Writable offset is at RW, so current section is RO.
  fw_updater_->targ_.offset = 0x11000;
  const uint8_t* image_ptr =
      reinterpret_cast<const uint8_t*>(fw_updater_->image_.data());

  std::vector<uint8_t> ufh_data;
  uint32_t good_reply = 0;

  EXPECT_CALL(*uep_, GetChunkLength()).Times(AnyNumber());
  {
    InSequence dummy;

    // Send first PDU and get a valid response.
    EXPECT_CALL(*uep_, SendHelper(first_header_, _, _));
    EXPECT_CALL(*uep_, Receive(_, sizeof(good_rpdu_), true, _))
        .WillOnce(WriteBuf(&good_rpdu_));

    // Send first section with 2 blocks. (0x40 bytes, 0x40 bytes)
    ufh_data = BuildHeaderData(sizeof(UpdateFrameHeader) + 0x80, 0, 0x11000);
    EXPECT_CALL(*uep_, SendHelper(ufh_data, _, _));
    EXPECT_CALL(*uep_, SendHelper(_, image_ptr + 0x11000, 0x40));
    EXPECT_CALL(*uep_, SendHelper(_, image_ptr + 0x11040, 0x40));
    EXPECT_CALL(*uep_, Receive(_, sizeof(good_reply), true, _))
        .WillOnce(WriteBuf(&good_reply));

    // Send second section with 1 block. (0x20 bytes)
    ufh_data = BuildHeaderData(sizeof(UpdateFrameHeader) + 0x20, 0, 0x11080);
    EXPECT_CALL(*uep_, SendHelper(ufh_data, _, _));
    EXPECT_CALL(*uep_, SendHelper(_, image_ptr + 0x11080, 0x20));
    EXPECT_CALL(*uep_, Receive(_, sizeof(good_reply), true, _))
        .WillOnce(WriteBuf(&good_reply));

    // Send done command.
    EXPECT_CALL(*uep_, SendHelper(done_cmd_, _, _));
    EXPECT_CALL(*uep_, Receive(_, 1, false, _)).WillOnce(Return(1));
  }

  // TransferImage takes care of running SendFirstPDU, which sets maximum
  // PDU size to 0x80.
  ASSERT_EQ(fw_updater_->TransferImage(SectionName::RW), true);
}

}  // namespace hammerd
