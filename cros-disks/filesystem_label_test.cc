// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/filesystem_label.h"

#include <string>

#include <gtest/gtest.h>

namespace cros_disks {

namespace {

// Forbidden characters in volume name
const char kForbiddenCharacters[] = "*?.,;:/\\|+=<>[]\"'\t";

};  // namespace

TEST(FilesystemLabelTest, ValidateVolumeLabel) {
  // Test long volume names
  EXPECT_EQ(LabelErrorType::kLabelErrorLongName,
            ValidateVolumeLabel("ABCDEFGHIJKL", "vfat"));
  EXPECT_EQ(LabelErrorType::kLabelErrorLongName,
            ValidateVolumeLabel("ABCDEFGHIJKLMNOP", "exfat"));

  // Test volume name length limits
  EXPECT_EQ(LabelErrorType::kLabelErrorNone,
            ValidateVolumeLabel("ABCDEFGHIJK", "vfat"));
  EXPECT_EQ(LabelErrorType::kLabelErrorNone,
            ValidateVolumeLabel("ABCDEFGHIJKLMNO", "exfat"));

  // Test unsupported file system type
  EXPECT_EQ(LabelErrorType::kLabelErrorUnsupportedFilesystem,
            ValidateVolumeLabel("ABC", "nonexistent-fs"));
}

class FilesystemLabelCharacterTest : public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_CASE_P(AsciiRange,
                        FilesystemLabelCharacterTest,
                        testing::Range(0, 256));

TEST_P(FilesystemLabelCharacterTest, ValidateVolumeLabelCharacters) {
  char value = GetParam();
  std::string volume_name = std::string("ABC") + value;

  if (!std::isprint(value) || strchr(kForbiddenCharacters, value)) {
    // Test forbidden characters in volume name
    EXPECT_EQ(LabelErrorType::kLabelErrorInvalidCharacter,
              ValidateVolumeLabel(volume_name, "vfat"));
    EXPECT_EQ(LabelErrorType::kLabelErrorInvalidCharacter,
              ValidateVolumeLabel(volume_name, "exfat"));
  } else {
    // Test allowed characters in volume name
    EXPECT_EQ(LabelErrorType::kLabelErrorNone,
              ValidateVolumeLabel(volume_name, "vfat"));
    EXPECT_EQ(LabelErrorType::kLabelErrorNone,
              ValidateVolumeLabel(volume_name, "exfat"));
  }
}

}  // namespace cros_disks
