// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/helpers/drm_display_info_reader.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <gtest/gtest.h>

namespace debugd {

namespace {

// The following are partial EDID info blobs taken from two displays. They were
// generated with the command:
//
// hexdump -e '16/1 "_x%02X" "\n"' edid | sed 's/_/\\/g; s/\\x //g; s/.*/ "&"/'
//
// Only the first 16 bytes are provided, because DRMDisplayInfoReader does not
// read anything past that offset.

const char kEDIDBlobFromNEC[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x38\xA3\x18\x2C\x01\x01\x01\x01";

const char kEDIDBlobFromCMN[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x0D\xAE\x41\x10\x00\x00\x00\x00";

// Writes |contents| to file at |path|, creating a new file or overwriting old
// file contents if necessary. Returns true if successfully written.
bool WriteToFile(const base::FilePath& path, const std::string& contents) {
  return base::WriteFile(path, contents.c_str(), contents.size()) ==
         static_cast<int>(contents.size());
}

}  // namespace

class DRMDisplayInfoReaderTest : public ::testing::Test {
 public:
  DRMDisplayInfoReaderTest() { CreateDRMDir(); }

  ~DRMDisplayInfoReaderTest() override = default;

  const base::FilePath& GetDRMPath() const { return drm_dir_.path(); }

 private:
  // Populates |drm_dir_| with dummy contents that are take from a real system.
  void CreateDRMDir() {
    EXPECT_TRUE(drm_dir_.CreateUniqueTempDir());
    const base::FilePath& drm_path = drm_dir_.path();

    // Create card0/.
    base::FilePath card0_path = drm_path.Append("card0");
    EXPECT_TRUE(base::CreateDirectory(card0_path));

    // Create card0/card0-DP-1 and populate it.
    base::FilePath card0_dp1_path = card0_path.Append("card0-DP-1");
    EXPECT_TRUE(base::CreateDirectory(card0_dp1_path));
    EXPECT_TRUE(WriteToFile(card0_dp1_path.Append("status"), "disconnected\n"));
    EXPECT_TRUE(WriteToFile(card0_dp1_path.Append("edid"), ""));
    // Create card0/card0-HDMI-A-1 and populate it.
    base::FilePath card0_hdmi_a1_path = card0_path.Append("card0-HDMI-A-1");
    EXPECT_TRUE(base::CreateDirectory(card0_hdmi_a1_path));
    EXPECT_TRUE(
        WriteToFile(card0_hdmi_a1_path.Append("status"), "connected\n"));
    EXPECT_EQ(sizeof(kEDIDBlobFromNEC),
              base::WriteFile(card0_hdmi_a1_path.Append("edid"),
                              kEDIDBlobFromNEC,
                              sizeof(kEDIDBlobFromNEC)));
    // Create card0/card0-DP-1 and populate it.
    base::FilePath card0_hdmi_a2_path = card0_path.Append("card0-HDMI-A-2");
    EXPECT_TRUE(base::CreateDirectory(card0_hdmi_a2_path));
    EXPECT_TRUE(
        WriteToFile(card0_hdmi_a2_path.Append("status"), "disconnected\n"));
    EXPECT_TRUE(WriteToFile(card0_hdmi_a2_path.Append("edid"), ""));

    // Create card1, which is empty.
    base::FilePath card1_path = drm_path.Append("card1");
    EXPECT_TRUE(base::CreateDirectory(card1_path));

    // Create card2/.
    base::FilePath card2_path = drm_path.Append("card2");
    EXPECT_TRUE(base::CreateDirectory(card2_path));

    // Create card2/card2-DVI-I-2 and populate it.
    base::FilePath card2_dvi_i2_path = card2_path.Append("card2-DVI-I-2");
    EXPECT_TRUE(base::CreateDirectory(card2_dvi_i2_path));
    EXPECT_TRUE(WriteToFile(card2_dvi_i2_path.Append("status"), "connected\n"));
    EXPECT_EQ(sizeof(kEDIDBlobFromCMN),
              base::WriteFile(card2_dvi_i2_path.Append("edid"),
                              kEDIDBlobFromCMN,
                              sizeof(kEDIDBlobFromCMN)));
  }

  base::ScopedTempDir drm_dir_;

  DISALLOW_COPY_AND_ASSIGN(DRMDisplayInfoReaderTest);
};

TEST_F(DRMDisplayInfoReaderTest, ReadDisplayInfo) {
  DRMDisplayInfoReader reader;
  std::unique_ptr<base::DictionaryValue> result =
      reader.GetDisplayInfo(GetDRMPath());

  // The contents of |result| should match what was created in CreateDRMDir().
  EXPECT_EQ(3U, result->size());
  const base::DictionaryValue* card0_info = nullptr;
  const base::DictionaryValue* card1_info = nullptr;
  const base::DictionaryValue* card2_info = nullptr;
  ASSERT_TRUE(result->GetDictionary("card0", &card0_info));
  ASSERT_TRUE(result->GetDictionary("card1", &card1_info));
  ASSERT_TRUE(result->GetDictionary("card2", &card2_info));

  // Check card0/.
  EXPECT_EQ(3U, card0_info->size());
  const base::DictionaryValue* dp1_info = nullptr;
  const base::DictionaryValue* hdmi_a1_info = nullptr;
  const base::DictionaryValue* hdmi_a2_info = nullptr;
  ASSERT_TRUE(card0_info->GetDictionary("DP-1", &dp1_info));
  ASSERT_TRUE(card0_info->GetDictionary("HDMI-A-1", &hdmi_a1_info));
  ASSERT_TRUE(card0_info->GetDictionary("HDMI-A-2", &hdmi_a2_info));

  EXPECT_EQ(1U, dp1_info->size());
  bool dp1_connected = false;
  EXPECT_TRUE(dp1_info->GetBoolean("is_connected", &dp1_connected));
  EXPECT_FALSE(dp1_connected);

  EXPECT_EQ(3U, hdmi_a1_info->size());
  bool hdmi_a1_connected = false;
  std::string hdmi_a1_manufacturer;
  int hdmi_a1_model = 0;
  EXPECT_TRUE(hdmi_a1_info->GetBoolean("is_connected", &hdmi_a1_connected));
  EXPECT_TRUE(hdmi_a1_connected);
  EXPECT_TRUE(hdmi_a1_info->GetString("manufacturer", &hdmi_a1_manufacturer));
  EXPECT_EQ("NEC", hdmi_a1_manufacturer);
  EXPECT_TRUE(hdmi_a1_info->GetInteger("model", &hdmi_a1_model));
  EXPECT_EQ(11288, hdmi_a1_model);

  EXPECT_EQ(1U, hdmi_a2_info->size());
  bool hdmi_a2_connected = false;
  EXPECT_TRUE(hdmi_a2_info->GetBoolean("is_connected", &hdmi_a2_connected));
  EXPECT_FALSE(hdmi_a2_connected);

  // Check card1/.
  EXPECT_EQ(0U, card1_info->size());

  // Check card2/.
  EXPECT_EQ(1U, card2_info->size());
  const base::DictionaryValue* dvi_i2_info = nullptr;
  ASSERT_TRUE(card2_info->GetDictionary("DVI-I-2", &dvi_i2_info));

  EXPECT_EQ(3U, dvi_i2_info->size());
  bool dvi_i2_connected = false;
  std::string dvi_i2_manufacturer;
  int dvi_i2_model = 0;
  EXPECT_TRUE(dvi_i2_info->GetBoolean("is_connected", &dvi_i2_connected));
  EXPECT_TRUE(dvi_i2_connected);
  EXPECT_TRUE(dvi_i2_info->GetString("manufacturer", &dvi_i2_manufacturer));
  EXPECT_EQ("CMN", dvi_i2_manufacturer);
  EXPECT_TRUE(dvi_i2_info->GetInteger("model", &dvi_i2_model));
  EXPECT_EQ(4161, dvi_i2_model);
}

}  // namespace debugd
