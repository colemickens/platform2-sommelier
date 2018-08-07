// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/async_file_reader.h"

#include <algorithm>
#include <memory>

#include <base/bind.h>
#include <base/callback.h>
#include <base/compiler_specific.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// Used to construct dummy files.
const char kDummyString[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";

// Dummy file name.
const char kDummyFileName[] = "dummy_file";

// Creates |path| and writes |total_size| bytes to it.
void CreateFile(const base::FilePath& path, size_t total_size) {
  std::string file_contents;

  // Add |kDummyString| repeatedly to the file contents.
  // If total_size % kDummyString.length() > 0, the last instance of
  // |kDummyString| will only be partially written to it.
  const std::string dummy_string = kDummyString;
  size_t remaining_size = total_size;
  while (remaining_size > 0) {
    size_t size_to_write = std::min(remaining_size, dummy_string.length());
    file_contents += dummy_string.substr(0, size_to_write);
    remaining_size -= size_to_write;
  }
  EXPECT_EQ(total_size, file_contents.length());

  // Write the contents to file and make sure the right size was written.
  EXPECT_EQ(total_size,
            base::WriteFile(path, file_contents.data(), total_size));
}

// When testing a file read where |file size| > |initial read size|, this is the
// number of block read iterations required to read the file.
// AsyncFileReader doubles the read block size each iteration, so the total file
// size it attempts to read (as a multiple of initial read block size) is:
//
//   1 + 2 + 4 + ... + 2^(N-1) = 2^N - 1
//
// where N is the number of reads.
int GetMultipleReadFactor(int num_multiple_reads) {
  return (1 << num_multiple_reads) - 1;
}

// Maximum time allowed for file read in milliseconds.
const int kMaxFileReadTimeMs = 60000;

}  // namespace

class AsyncFileReaderTest : public ::testing::Test {
 public:
  AsyncFileReaderTest()
      : temp_dir_(new base::ScopedTempDir()),
        file_reader_(new AsyncFileReader()),
        got_error_(false) {
    CHECK(temp_dir_->CreateUniqueTempDir());
    CHECK(temp_dir_->IsValid());
    path_ = temp_dir_->GetPath().Append(kDummyFileName);
  }
  ~AsyncFileReaderTest() override {}

 protected:
  // Creates a file containing |file_size| bytes and uses AsyncFileReader to
  // read from it, starting with an |initial_read_size|-byte chunk. Returns
  // false if initialization failed or if the reader timed out.
  bool WriteAndReadData(size_t file_size,
                        size_t initial_read_size) WARN_UNUSED_RESULT {
    data_.clear();
    got_error_ = false;

    CreateFile(path_, file_size);
    file_reader_->set_initial_read_size_for_testing(initial_read_size);
    if (!file_reader_->Init(path_))
      return false;
    file_reader_->StartRead(
        base::Bind(&AsyncFileReaderTest::ReadCallback, base::Unretained(this)),
        base::Bind(&AsyncFileReaderTest::ErrorCallback,
                   base::Unretained(this)));
    return loop_runner_.StartLoop(
        base::TimeDelta::FromMilliseconds(kMaxFileReadTimeMs));
  }

  // Returns the contents of |path_|.
  std::string ReadFile() {
    std::string data;
    CHECK(base::ReadFileToString(path_, &data));
    return data;
  }

  // Callback for successful reads.
  void ReadCallback(const std::string& data) {
    loop_runner_.StopLoop();
    data_ = data;
  }

  // Callback for read errors.
  void ErrorCallback() {
    loop_runner_.StopLoop();
    got_error_ = true;
  }

  TestMainLoopRunner loop_runner_;

  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  std::unique_ptr<AsyncFileReader> file_reader_;

  // Test file.
  base::FilePath path_;

  // Data read from |path_| by |file_reader_|.
  std::string data_;

  // True if |file_reader_| reported an error.
  bool got_error_;
};

// Read an empty file.
TEST_F(AsyncFileReaderTest, EmptyFileRead) {
  ASSERT_TRUE(WriteAndReadData(0, 32));
  EXPECT_FALSE(got_error_);
  EXPECT_EQ("", data_);
}

// Read a file with one block read, with the file size being less than the block
// size (partial block read).
TEST_F(AsyncFileReaderTest, SingleBlockReadPartial) {
  ASSERT_TRUE(WriteAndReadData(31, 32));
  EXPECT_FALSE(got_error_);
  EXPECT_EQ(ReadFile(), data_);
}

// Read a file with one block read, with the file size being equal to the block
// size (full block read).
TEST_F(AsyncFileReaderTest, SingleBlockReadFull) {
  ASSERT_TRUE(WriteAndReadData(32, 32));
  EXPECT_FALSE(got_error_);
  EXPECT_EQ(ReadFile(), data_);
}

// Read a file with multiple block reads, with the last block being a partial
// read.
TEST_F(AsyncFileReaderTest, MultipleBlockReadPartial) {
  ASSERT_TRUE(WriteAndReadData(32 * GetMultipleReadFactor(5) - 1, 32));
  EXPECT_FALSE(got_error_);
  EXPECT_EQ(ReadFile(), data_);
}

// Read a file with multiple block reads, with the last block being a full read.
TEST_F(AsyncFileReaderTest, MultipleBlockReadFull) {
  ASSERT_TRUE(WriteAndReadData(32 * GetMultipleReadFactor(5), 32));
  EXPECT_FALSE(got_error_);
  EXPECT_EQ(ReadFile(), data_);
}

// Initializing the reader with a nonexistent file should fail.
TEST_F(AsyncFileReaderTest, InitWithMissingFile) {
  EXPECT_FALSE(file_reader_->Init(path_));
}

}  // namespace system
}  // namespace power_manager
