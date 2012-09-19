// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "power_manager/common/util.h"

namespace {
const char kTestFilename[] = "test.file";
}

namespace power_manager {

  class UtilTest : public ::testing::Test {
  public:
    UtilTest() {}

    virtual void SetUp() {
      // Create a temporary directory for the test files
      temp_dir_generator_.reset(new ScopedTempDir());
      ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
      EXPECT_TRUE(temp_dir_generator_->IsValid());
    }

  protected:
    void WriteFileWrapper(const FilePath& file_path, const char* value);
    scoped_ptr<ScopedTempDir> temp_dir_generator_;
};

void UtilTest::WriteFileWrapper(const FilePath &path, const char* value) {
  ASSERT_EQ(file_util::WriteFile(path, value, strlen(value)), strlen(value));
}

TEST_F(UtilTest, GetUintFromFile) {
  unsigned int value;
  FilePath path = temp_dir_generator_->path().Append(kTestFilename);
  std::string path_str = path.value();

  file_util::Delete(path, false);
  EXPECT_FALSE(util::GetUintFromFile(path_str.c_str(), &value));
  WriteFileWrapper(path, "12345");
  EXPECT_TRUE(util::GetUintFromFile(path_str.c_str(), &value));
  EXPECT_EQ(value, 12345);
  WriteFileWrapper(path, "5");
  EXPECT_TRUE(util::GetUintFromFile(path_str.c_str(), &value));
  EXPECT_EQ(value, 5);
  WriteFileWrapper(path, "10foo20");
  EXPECT_FALSE(util::GetUintFromFile(path_str.c_str(), &value));
  WriteFileWrapper(path, "garbage");
  EXPECT_FALSE(util::GetUintFromFile(path_str.c_str(), &value));
  WriteFileWrapper(path, "");
  EXPECT_FALSE(util::GetUintFromFile(path_str.c_str(), &value));
  // The underlying library doesn't properly handle negative values
  // TODO(crbug.com/128596): Uncomment these when base::StringToUint handles
  // negatives properly
  // WriteFileWrapper(path, "-10");
  // EXPECT_TRUE(util::GetUintFromFile(path_str.c_str(), &value));
  // EXPECT_EQ(value, 10);
}

}  // namespace power_manager
