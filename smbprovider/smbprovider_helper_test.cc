// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <libsmbclient.h>
#include <vector>

#include "smbprovider/constants.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

class SmbProviderHelperTest : public testing::Test {
 public:
  SmbProviderHelperTest() = default;
  ~SmbProviderHelperTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbProviderHelperTest);
};

// WriteEntry should fail in case the buffer size provided is too small. Even
// one character filename should not fit if the buffer size is
// sizeof(smbc_dirent).
TEST_F(SmbProviderHelperTest, WriteEntryFailsWithSmallBuffer) {
  smbc_dirent dirent;
  EXPECT_FALSE(WriteEntry("a", SMBC_FILE, sizeof(smbc_dirent), &dirent));
}

// WriteEntry should succeed if given a proper sized buffer.
TEST_F(SmbProviderHelperTest, WriteEntrySucceedsWithProperBufferSize) {
  const std::string name("a");
  std::vector<uint8_t> dir_buf(CalculateEntrySize(name));
  EXPECT_TRUE(WriteEntry(name, SMBC_FILE, dir_buf.size(),
                         GetDirentFromBuffer(dir_buf.data())));
}

// WriteEntry should write the proper fields.
TEST_F(SmbProviderHelperTest, WriteEntrySucceeds) {
  std::vector<uint8_t> dir_buf(kBufferSize);
  smbc_dirent* dirent = GetDirentFromBuffer(dir_buf.data());
  const std::string name("test_folder");
  uint32_t type = SMBC_DIR;

  DCHECK_GE(kBufferSize, CalculateEntrySize(name));
  EXPECT_TRUE(WriteEntry(name, type, kBufferSize, dirent));
  EXPECT_EQ(name, dirent->name);
  EXPECT_EQ(type, dirent->smbc_type);
  EXPECT_EQ(CalculateEntrySize(name), dirent->dirlen);
}

// "." and ".." entries shouldn't be added to DirectoryEntryList.
TEST_F(SmbProviderHelperTest, SelfEntryDoesNotGetAdded) {
  std::vector<uint8_t> dir_buf(kBufferSize);
  smbc_dirent* dirent = GetDirentFromBuffer(dir_buf.data());
  EXPECT_TRUE(WriteEntry(".", SMBC_DIR, dir_buf.size(), dirent));

  DirectoryEntryList entries;
  AddEntryIfValid(*dirent, &entries);
  EXPECT_EQ(0, entries.entries_size());

  EXPECT_TRUE(WriteEntry("..", SMBC_DIR, dir_buf.size(), dirent));
  AddEntryIfValid(*dirent, &entries);
  EXPECT_EQ(0, entries.entries_size());
}

// Incorrect smbc_type should not be added to DirectoryEntryList.
TEST_F(SmbProviderHelperTest, WrongTypeDoesNotGetAdded) {
  std::vector<uint8_t> dir_buf(kBufferSize);
  smbc_dirent* dirent = GetDirentFromBuffer(dir_buf.data());
  EXPECT_TRUE(WriteEntry("printer1", SMBC_PRINTER_SHARE, kBufferSize, dirent));

  DirectoryEntryList entries;
  AddEntryIfValid(*dirent, &entries);
  EXPECT_EQ(0, entries.entries_size());
}

// AddEntryIfValid should properly add proper file and directory entries.
TEST_F(SmbProviderHelperTest, AddEntryProperlyAddsValidEntries) {
  std::vector<uint8_t> dir_buf(kBufferSize);
  smbc_dirent* dirent = GetDirentFromBuffer(dir_buf.data());

  DirectoryEntryList entries;
  const std::string file_name("dog.jpg");
  uint32_t file_type = SMBC_FILE;
  EXPECT_TRUE(WriteEntry(file_name, file_type, kBufferSize, dirent));

  AddEntryIfValid(*dirent, &entries);
  EXPECT_EQ(1, entries.entries_size());
  const DirectoryEntry& file_entry = entries.entries(0);
  EXPECT_FALSE(file_entry.is_directory());
  EXPECT_EQ(file_name, file_entry.name());
  EXPECT_EQ(-1, file_entry.size());
  EXPECT_EQ(-1, file_entry.last_modified_time());

  AdvanceDirEnt(dirent);
  const std::string dir_name("dogs");
  uint32_t dir_type = SMBC_DIR;
  EXPECT_TRUE(WriteEntry(dir_name, dir_type, kBufferSize, dirent));

  AddEntryIfValid(*dirent, &entries);
  EXPECT_EQ(2, entries.entries_size());
  const DirectoryEntry& dir_entry = entries.entries(1);
  EXPECT_TRUE(dir_entry.is_directory());
  EXPECT_EQ(dir_name, dir_entry.name());
  EXPECT_EQ(-1, dir_entry.size());
  EXPECT_EQ(-1, dir_entry.last_modified_time());
}

// Tests that AppendPath properly appends with or without the trailing separator
// "/" on the base path.
TEST_F(SmbProviderHelperTest, AppendPath) {
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare", "/test"));
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare/", "/test"));
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare", "test"));
  EXPECT_EQ("smb://qnap/testshare/test",
            AppendPath("smb://qnap/testshare/", "test"));
  EXPECT_EQ("smb://qnap/testshare", AppendPath("smb://qnap/testshare/", "/"));
  EXPECT_EQ("smb://qnap/testshare", AppendPath("smb://qnap/testshare/", ""));
}

// Should return true on "." and ".." entries.
TEST_F(SmbProviderHelperTest, IsSelfOrParentDir) {
  EXPECT_TRUE(IsSelfOrParentDir("."));
  EXPECT_TRUE(IsSelfOrParentDir(".."));
  EXPECT_FALSE(IsSelfOrParentDir("/"));
  EXPECT_FALSE(IsSelfOrParentDir("test.jpg"));
}

// Only SMBC_DIR and SMBC_FILE should be processed.
TEST_F(SmbProviderHelperTest, ShouldProcessEntryType) {
  EXPECT_TRUE(ShouldProcessEntryType(SMBC_DIR));
  EXPECT_TRUE(ShouldProcessEntryType(SMBC_FILE));

  EXPECT_FALSE(ShouldProcessEntryType(SMBC_WORKGROUP));
  EXPECT_FALSE(ShouldProcessEntryType(SMBC_SERVER));
  EXPECT_FALSE(ShouldProcessEntryType(SMBC_FILE_SHARE));
  EXPECT_FALSE(ShouldProcessEntryType(SMBC_PRINTER_SHARE));
  EXPECT_FALSE(ShouldProcessEntryType(SMBC_COMMS_SHARE));
  EXPECT_FALSE(ShouldProcessEntryType(SMBC_IPC_SHARE));
  EXPECT_FALSE(ShouldProcessEntryType(SMBC_LINK));
}

}  // namespace smbprovider
