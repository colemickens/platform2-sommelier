// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <gtest/gtest.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/async_file_reader.h"

namespace {

// Used to construct dummy files.
const char kDummyString[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";

// Dummy file name.
const char kDummyFileName[] = "dummy_file";

void CreateFile(const FilePath& path, size_t total_size) {
  // Container for the string that will be written to the file.
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
            file_util::WriteFile(path, file_contents.data(), total_size));
}

// When testing a file read where |file size| > |initial read size|, this is the
// number of block read iterations required to read the file.
// AsyncFileReader doubles the read block size each iteration, so the total file
// size it attempts to read (as a multiple of initial read block size) is:
//   1 + 2 + 4 + ... + 2^(N-1) = 2^N - 1
// where N = |kNumMultipleReads|;
const int kNumMultipleReads = 5;

int GetMultipleReadFactor(int num_multiple_reads) {
  return (1 << num_multiple_reads) - 1;
}

// Maximum time allowed for file read in milliseconds.
const int kMaxFileReadTimeMs = 60000;

// Poll interval when waiting for AsyncFileReader to finish.
const int kWaitPollMs = 200;

}

namespace power_manager {

class AsyncFileReaderTest : public ::testing::Test {
 public:
  AsyncFileReaderTest() {}

  virtual void SetUp() {
    // Create a temporary directory for the file to read.
    temp_dir_generator_.reset(new ScopedTempDir());
    ASSERT_TRUE(temp_dir_generator_->CreateUniqueTempDir());
    EXPECT_TRUE(temp_dir_generator_->IsValid());

    dummy_file_path_ = temp_dir_generator_->path().Append(kDummyFileName);

    // Initialize the file reader.
    file_reader_.reset(new AsyncFileReader());
    initial_read_size_ = file_reader_->initial_read_size_;

    error_found_ = false;
    done_reading_ = false;
  }

  // Creates a file containing |size| bytes and uses AsyncFileReader to read
  // from it.  To skip creating a file, pass in size = -1.
  void StartReadTest(int size) {
    // This traps multiple calls to this function without calling Setup().
    ASSERT_FALSE(done_reading_);

    if (size >= 0) {
      CreateFile(dummy_file_path_, size);
      CHECK(file_reader_->Init(dummy_file_path_.value()));
    }
    read_cb_ =
        base::Bind(&AsyncFileReaderTest::ReadCallback, base::Unretained(this));
    error_cb_ =
        base::Bind(&AsyncFileReaderTest::ErrorCallback, base::Unretained(this));
    file_reader_->StartRead(&read_cb_, &error_cb_);

    start_time_ = base::TimeTicks::Now();

    loop_ = g_main_loop_new(NULL, false);
    g_timeout_add(kWaitPollMs, CheckForReadCompletionThunk, this);
    g_main_loop_run(loop_);

    ASSERT_LE((base::TimeTicks::Now() - start_time_).InMilliseconds(),
              kMaxFileReadTimeMs);
  }

 protected:
  scoped_ptr<AsyncFileReader> file_reader_;
  scoped_ptr<ScopedTempDir> temp_dir_generator_;
  FilePath dummy_file_path_;
  int initial_read_size_;

  // Called to indicate a successful read.
  void ReadCallback(const std::string&);

  // Called to indicate a read error.
  void ErrorCallback();

  // Called to indicate a read error.
  SIGNAL_CALLBACK_0(AsyncFileReaderTest, gboolean, CheckForReadCompletion);

  // For passing callbacks to AsyncFileReader.
  base::Callback<void(const std::string&)> read_cb_;
  base::Callback<void()> error_cb_;

  // Indicates file read status.
  bool done_reading_;
  bool error_found_;

  // When a file read started.
  base::TimeTicks start_time_;

  // Main loop for glib.
  GMainLoop* loop_;
};


void AsyncFileReaderTest::ReadCallback(const std::string& data) {
  error_found_ = false;
  done_reading_ = true;

  std::string actual_file_data;
  ASSERT_TRUE(file_util::ReadFileToString(dummy_file_path_, &actual_file_data));
  EXPECT_EQ(actual_file_data, data);
}

void AsyncFileReaderTest::ErrorCallback() {
  error_found_ = true;
  done_reading_ = true;
}

gboolean AsyncFileReaderTest::CheckForReadCompletion() {
  int64 duration_ms = (base::TimeTicks::Now() - start_time_).InMilliseconds();
  if (!done_reading_ && duration_ms <= kMaxFileReadTimeMs)
    return TRUE;
  g_main_loop_quit(loop_);
  return FALSE;
}

// Read an empty file.
TEST_F(AsyncFileReaderTest, EmptyFileRead) {
  StartReadTest(0);
  EXPECT_TRUE(done_reading_);
  EXPECT_FALSE(error_found_);
}

// Read a file with one block read, with the file size being less than the block
// size (partial block read).
TEST_F(AsyncFileReaderTest, SingleBlockReadPartial) {
  StartReadTest(initial_read_size_ - 1);
  EXPECT_TRUE(done_reading_);
  EXPECT_FALSE(error_found_);
}

// Read a file with one block read, with the file size being equal to the block
// size (full block read).
TEST_F(AsyncFileReaderTest, SingleBlockReadFull) {
  StartReadTest(initial_read_size_);
  EXPECT_TRUE(done_reading_);
  EXPECT_FALSE(error_found_);
}

// Read a file with multiple block reads, with the last block being a partial
// read.
TEST_F(AsyncFileReaderTest, MultipleBlockReadPartial) {
  StartReadTest(
      initial_read_size_ * GetMultipleReadFactor(kNumMultipleReads) - 1);
  EXPECT_TRUE(done_reading_);
  EXPECT_FALSE(error_found_);
}

// Read a file with multiple block reads, with the last block being a full read.
TEST_F(AsyncFileReaderTest, MultipleBlockReadFull) {
  StartReadTest(initial_read_size_ * GetMultipleReadFactor(kNumMultipleReads));
  EXPECT_TRUE(done_reading_);
  EXPECT_FALSE(error_found_);
}

// Read a file that doesn't exist.  Should end up with an error.
TEST_F(AsyncFileReaderTest, ReadNonexistentFile) {
  StartReadTest(-1);
  EXPECT_TRUE(done_reading_);
  EXPECT_TRUE(error_found_);
}

}  // namespace power_manager
