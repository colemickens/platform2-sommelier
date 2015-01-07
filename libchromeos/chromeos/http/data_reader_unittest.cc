// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/data_reader.h>

#include <gtest/gtest.h>

namespace chromeos {
namespace http {

//////////////////////////////////////////////////////////////////////////////
class MemoryDataReaderTest : public testing::Test {
 public:
  std::string GetData() const {
    return std::string{reinterpret_cast<const char*>(reader_.data_.data()),
                       reader_.data_.size()};
  }
  size_t GetReadPointer() const { return reader_.read_pointer_; }

  MemoryDataReader reader_{std::string{"abcdefgh"}};
};

TEST_F(MemoryDataReaderTest, Init) {
  EXPECT_EQ(0, GetReadPointer());
  EXPECT_EQ("abcdefgh", GetData());
}

TEST_F(MemoryDataReaderTest, ReadData) {
  const size_t kBufSize = 5;
  char buffer[kBufSize];
  size_t size = 0;
  ASSERT_TRUE(reader_.ReadData(buffer, kBufSize, &size, nullptr));
  EXPECT_EQ(kBufSize, size);
  EXPECT_EQ(kBufSize, GetReadPointer());
  EXPECT_EQ("abcde", (std::string{buffer, size}));
  ASSERT_TRUE(reader_.ReadData(buffer, kBufSize, &size, nullptr));
  EXPECT_EQ(3, size);
  EXPECT_EQ(8, GetReadPointer());
  EXPECT_EQ("fgh", (std::string{buffer, size}));
  ASSERT_TRUE(reader_.ReadData(buffer, kBufSize, &size, nullptr));
  EXPECT_EQ(0, size);
  EXPECT_EQ(8, GetReadPointer());
}

TEST_F(MemoryDataReaderTest, Reset) {
  char buffer[5];
  size_t size = 0;
  ASSERT_TRUE(reader_.ReadData(buffer, sizeof(buffer), &size, nullptr));
  reader_.SetData("012345");
  EXPECT_EQ(0, GetReadPointer());
  EXPECT_EQ("012345", GetData());
}

}  // namespace http
}  // namespace chromeos
