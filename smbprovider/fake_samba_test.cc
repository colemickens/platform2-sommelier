// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "smbprovider/fake_samba_interface.h"

namespace smbprovider {

namespace {

constexpr uint64_t kFileDate = 42;

std::string GetDefaultServer() {
  return "smb://wdshare";
}

std::string GetDefaultMountRoot() {
  return "smb://wdshare/test";
}

std::string GetDefaultDirectoryPath() {
  return "smb://wdshare/test/path";
}

std::string GetDefaultFilePath() {
  return "smb://wdshare/test/dog.jpg";
}

}  // namespace

class FakeSambaTest : public testing::Test {
 public:
  FakeSambaTest() {
    fake_samba_.AddDirectory(GetDefaultServer());
    fake_samba_.AddDirectory(GetDefaultMountRoot());
  }
  ~FakeSambaTest() = default;

  FakeSambaInterface fake_samba_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSambaTest);
};

TEST_F(FakeSambaTest, FileEqualReturnsFalseOnFileThatDoesntExist) {
  EXPECT_FALSE(fake_samba_.IsFileDataEqual("smb://wdshare/invalid.jpg",
                                           std::vector<uint8_t>()));
}

TEST_F(FakeSambaTest, FileEqualReturnsFalseOnDirectory) {
  fake_samba_.AddDirectory(GetDefaultDirectoryPath());
  EXPECT_FALSE(fake_samba_.IsFileDataEqual(GetDefaultDirectoryPath(),
                                           std::vector<uint8_t>()));
}

TEST_F(FakeSambaTest, FileEqualReturnsFalseOnFileWithNoData) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Should be false even if we pass in an empty vector.
  EXPECT_FALSE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(),
                                           std::vector<uint8_t>()));
}

TEST_F(FakeSambaTest, FileEqualReturnsFalseOnUnequalData) {
  const std::vector<uint8_t> file_data = {0};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);

  // Provide vector with different data.
  const std::vector<uint8_t> other_data = {1};
  EXPECT_FALSE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), other_data));
}

TEST_F(FakeSambaTest, FileEqualReturnsFalseOnSamePrefix) {
  const std::vector<uint8_t> file_data = {0, 1, 2};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);

  // Provide vector with different data but same prefix.
  const std::vector<uint8_t> other_data = {0, 1, 2, 3};
  EXPECT_FALSE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), other_data));
}

TEST_F(FakeSambaTest, FileEqualReturnsFalseOnSamePrefix2) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);

  // Provide vector with different data but same prefix.
  const std::vector<uint8_t> other_data = {0, 1, 2};
  EXPECT_FALSE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), other_data));
}

TEST_F(FakeSambaTest, FileEqualReturnsTrueOnEmptyData) {
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, std::vector<uint8_t>());

  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(),
                                          std::vector<uint8_t>()));
}

TEST_F(FakeSambaTest, FileEqualReturnsTrueOnEqualData) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);

  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), file_data));
}

TEST_F(FakeSambaTest, OpenFileOpensFileWithZeroSizeAndZeroOffset) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Verify that the offset and size is zero.
  EXPECT_EQ(0, fake_samba_.GetFileSize(GetDefaultFilePath()));
  EXPECT_EQ(0, fake_samba_.GetFileOffset(file_id));
}

TEST_F(FakeSambaTest, SeekCorrectlyChangesOffset) {
  std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Change the offset and verify it changed.
  int64_t new_offset = 2;
  EXPECT_EQ(0, fake_samba_.Seek(file_id, new_offset));
  EXPECT_EQ(new_offset, fake_samba_.GetFileOffset(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldFailIfDirectory) {
  fake_samba_.AddDirectory(GetDefaultDirectoryPath());

  // Open directory.
  int32_t dir_id;
  EXPECT_EQ(0, fake_samba_.OpenDirectory(GetDefaultDirectoryPath(), &dir_id));

  // Should fail writing any data to the directory.
  const std::vector<uint8_t> new_data = {'x'};
  EXPECT_EQ(EISDIR,
            fake_samba_.WriteFile(dir_id, new_data.data(), new_data.size()));

  EXPECT_EQ(0, fake_samba_.CloseDirectory(dir_id));
}

TEST_F(FakeSambaTest, WriteFileShouldFailIfNotWriteable) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Open the file with READ_ONLY permissions.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDONLY, &file_id));

  // Should fail writing any data to the file.
  const std::vector<uint8_t> new_data = {'x'};
  EXPECT_EQ(EINVAL,
            fake_samba_.WriteFile(file_id, new_data.data(), new_data.size()));

  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldChangeOffset) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Write the data into the file.
  const std::vector<uint8_t> new_data = {0, 1, 2, 3, 4, 5};
  EXPECT_EQ(0,
            fake_samba_.WriteFile(file_id, new_data.data(), new_data.size()));

  // Offset should be at the end of the written data + offset (which in this
  // case is 0).
  EXPECT_EQ(new_data.size(), fake_samba_.GetFileOffset(file_id));

  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldWriteCorrectDataWithReadWrite) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Write the data into the file.
  const std::vector<uint8_t> new_data = {0, 1, 2, 3, 4, 5};
  EXPECT_EQ(0,
            fake_samba_.WriteFile(file_id, new_data.data(), new_data.size()));

  // Read the contents of the file.
  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), new_data));
  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldWriteCorrectDataWithWriteOnly) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_WRONLY, &file_id));

  // Write the data into the file.
  const std::vector<uint8_t> new_data = {0, 1, 2, 3, 4, 5};
  EXPECT_EQ(0,
            fake_samba_.WriteFile(file_id, new_data.data(), new_data.size()));

  // Read the contents of the file.
  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), new_data));
  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldWriteFromOffset) {
  std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Change the offset to 1.
  EXPECT_EQ(0, fake_samba_.Seek(file_id, 1));

  // Write the data into the file.
  const std::vector<uint8_t> new_data = {'a', 'b'};
  EXPECT_EQ(0,
            fake_samba_.WriteFile(file_id, new_data.data(), new_data.size()));

  // Validate that the data read is the same as expected.
  const std::vector<uint8_t> expected = {0, 'a', 'b', 3, 4, 5};
  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), expected));

  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldWriteToLargerSize) {
  std::vector<uint8_t> file_data = {0, 1, 2, 3};
  fake_samba_.AddFile(GetDefaultFilePath(), kFileDate, file_data);
  EXPECT_EQ(file_data.size(), fake_samba_.GetFileSize(GetDefaultFilePath()));

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Write the data into the file.
  const std::vector<uint8_t> new_data = {5, 6, 7, 8, 9, 9, 9, 9};
  EXPECT_EQ(0, fake_samba_.GetFileOffset(file_id));
  EXPECT_EQ(0,
            fake_samba_.WriteFile(file_id, new_data.data(), new_data.size()));

  // Validate that the data read is the same as expected.
  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), new_data));

  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

TEST_F(FakeSambaTest, WriteFileShouldWriteTwice) {
  fake_samba_.AddFile(GetDefaultFilePath());

  // Open the file to get a file_id.
  int32_t file_id;
  EXPECT_EQ(0, fake_samba_.OpenFile(GetDefaultFilePath(), O_RDWR, &file_id));

  // Do the first write.
  const std::vector<uint8_t> data1 = {1, 2, 3, 4};
  EXPECT_EQ(0, fake_samba_.GetFileOffset(file_id));
  EXPECT_EQ(0, fake_samba_.WriteFile(file_id, data1.data(), data1.size()));

  // Do the second write.
  const std::vector<uint8_t> data2 = {'a', 'b', 'c', 'd'};
  EXPECT_EQ(data1.size(), fake_samba_.GetFileOffset(file_id));
  EXPECT_EQ(0, fake_samba_.WriteFile(file_id, data2.data(), data2.size()));

  // Size of the data should be equal to the expected data.
  const std::vector<uint8_t> expected_data = {1, 2, 3, 4, 'a', 'b', 'c', 'd'};

  // Validate that the data read is the same as expected.
  EXPECT_TRUE(fake_samba_.IsFileDataEqual(GetDefaultFilePath(), expected_data));

  EXPECT_EQ(0, fake_samba_.CloseFile(file_id));
}

}  // namespace smbprovider
