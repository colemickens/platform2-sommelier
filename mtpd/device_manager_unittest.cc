// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/device_manager.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace mtpd {
namespace {

struct PathToFileIdHelperTestCase {
  const char* filename;
  bool is_file;
};

const PathToFileIdHelperTestCase kFolderPathTestCase[] = {
  { "/", false },
  { "valid", false },
  { "path", false },
};

const PathToFileIdHelperTestCase kFolderPathWithExtraSlashesTestCase[] = {
  { "/", false },
  { "still", false },
  { "valid", false },
  { "/", false },
  { "/", false },
  { "path", false },
};

const PathToFileIdHelperTestCase kFilePathTestCase[] = {
  { "/", false },
  { "path", false },
  { "to", false },
  { "file", true },
};

const PathToFileIdHelperTestCase kFilePathWithExtraSlashesTestCase[] = {
  { "/", false },
  { "path", false },
  { "/", false },
  { "/", false },
  { "to", false },
  { "file2", true },
};

const PathToFileIdHelperTestCase kInvalidFilePathTestCase1[] = {
  { "/", false },
  { "invalid", false },
  { "test", true },
  { "path", false },
};

const PathToFileIdHelperTestCase kInvalidFilePathTestCase2[] = {
  { "/", false },
  { "also", false },
  { "invalid", false },
  { "test", true },
  { "path", true },
};

bool TestPathToId(DeviceManager::ProcessPathComponentFunc test_func,
                  const PathToFileIdHelperTestCase* test_case,
                  size_t test_case_size) {
  for (size_t i = 0; i < test_case_size; ++i) {
    LIBMTP_file_t dummy_file;
    dummy_file.filename = const_cast<char*>(test_case[i].filename);
    dummy_file.filetype = test_case[i].is_file ? LIBMTP_FILETYPE_JPEG :
                                                 LIBMTP_FILETYPE_FOLDER;
    uint32_t id;
    if (!test_func(&dummy_file, i, test_case_size, &id))
      return false;
  }
  return true;
}

TEST(DeviceManagerTest, ParseStorageName) {
  struct ParseStorageNameTestCase {
    const char* input;
    bool expected_result;
    const char* expected_bus;
    uint32_t expected_storage_id;
  } test_cases[] = {
    { "usb:123:4", true, "usb:123", 4 },
    { "usb:1,2,3:4", true, "usb:1,2,3", 4 },
    { "notusb:123:4", false, "", 0 },
    { "usb:123:4:badfield", false, "", 0 },
    { "usb:123:not_number", false, "", 0 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::string bus;
    uint32_t storage_id = static_cast<uint32_t>(-1);
    bool result =
        DeviceManager::ParseStorageName(test_cases[i].input, &bus, &storage_id);
    EXPECT_EQ(test_cases[i].expected_result, result);
    if (test_cases[i].expected_result) {
      EXPECT_EQ(test_cases[i].expected_bus, bus);
      EXPECT_EQ(test_cases[i].expected_storage_id, storage_id);
    }
  }
}

TEST(DeviceManager, IsFolder) {
  EXPECT_TRUE(TestPathToId(DeviceManager::IsFolder,
                           kFolderPathTestCase,
                           arraysize(kFolderPathTestCase)));
  EXPECT_TRUE(TestPathToId(DeviceManager::IsFolder,
                           kFolderPathWithExtraSlashesTestCase,
                           arraysize(kFolderPathWithExtraSlashesTestCase)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsFolder,
                            kFilePathTestCase,
                            arraysize(kFilePathTestCase)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsFolder,
                            kFilePathWithExtraSlashesTestCase,
                            arraysize(kFilePathWithExtraSlashesTestCase)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsFolder,
                            kInvalidFilePathTestCase1,
                            arraysize(kInvalidFilePathTestCase1)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsFolder,
                            kInvalidFilePathTestCase2,
                            arraysize(kInvalidFilePathTestCase2)));
}

TEST(DeviceManager, IsValidComponentInFilePath) {
  EXPECT_FALSE(TestPathToId(DeviceManager::IsValidComponentInFilePath,
                            kFolderPathTestCase,
                            arraysize(kFolderPathTestCase)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsValidComponentInFilePath,
                            kFolderPathWithExtraSlashesTestCase,
                            arraysize(kFolderPathWithExtraSlashesTestCase)));
  EXPECT_TRUE(TestPathToId(DeviceManager::IsValidComponentInFilePath,
                           kFilePathTestCase,
                           arraysize(kFilePathTestCase)));
  EXPECT_TRUE(TestPathToId(DeviceManager::IsValidComponentInFilePath,
                           kFilePathWithExtraSlashesTestCase,
                           arraysize(kFilePathWithExtraSlashesTestCase)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsValidComponentInFilePath,
                            kInvalidFilePathTestCase1,
                            arraysize(kInvalidFilePathTestCase1)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsValidComponentInFilePath,
                            kInvalidFilePathTestCase2,
                            arraysize(kInvalidFilePathTestCase2)));
}

TEST(DeviceManager, IsValidComponentInFileOrFolderPath) {
  EXPECT_TRUE(TestPathToId(DeviceManager::IsValidComponentInFileOrFolderPath,
                           kFolderPathTestCase,
                           arraysize(kFolderPathTestCase)));
  EXPECT_TRUE(TestPathToId(DeviceManager::IsValidComponentInFileOrFolderPath,
                           kFolderPathWithExtraSlashesTestCase,
                           arraysize(kFolderPathWithExtraSlashesTestCase)));
  EXPECT_TRUE(TestPathToId(DeviceManager::IsValidComponentInFileOrFolderPath,
                           kFilePathTestCase,
                           arraysize(kFilePathTestCase)));
  EXPECT_TRUE(TestPathToId(DeviceManager::IsValidComponentInFileOrFolderPath,
                           kFilePathWithExtraSlashesTestCase,
                           arraysize(kFilePathWithExtraSlashesTestCase)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsValidComponentInFileOrFolderPath,
                            kInvalidFilePathTestCase1,
                            arraysize(kInvalidFilePathTestCase1)));
  EXPECT_FALSE(TestPathToId(DeviceManager::IsValidComponentInFileOrFolderPath,
                            kInvalidFilePathTestCase2,
                            arraysize(kInvalidFilePathTestCase2)));
}

}  // namespace
}  // namespace mtp
