// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_VMLOG_WRITER_H_
#define METRICS_VMLOG_WRITER_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/time/time.h>
#include <base/timer/timer.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace chromeos_metrics {

// Record for retrieving and reporting values from /proc/vmstat
struct VmstatRecord {
  uint64_t page_faults_;    // major faults
  uint64_t swap_in_;        // pages swapped in
  uint64_t swap_out_;       // pages swapped out
};

// Parse cumulative vm statistics from data read from /proc/vmstat.  Returns
// true for success.
bool VmStatsParseStats(const std::string& stats,
                       struct VmstatRecord* record);

// Encapsulates the logic for writing to vmlog and rotating log files when
// necessary.
class VmlogFile {
 public:
  // Creates a new VmlogFile to manage vmlog logging. Output is written to
  // live_path, and rotated to rotated_path when the file would exceed max_size.
  // Output files always begin with the contents of header.
  VmlogFile(const base::FilePath& live_path,
            const base::FilePath& rotated_path,
            const uint64_t max_size,
            const std::string& header);
  ~VmlogFile();

  // Writes the requested data to the vmlog log file. Returns false on failure.
  bool Write(const std::string& data);

 private:
  friend class VmlogWriterTest;
  FRIEND_TEST(VmlogWriterTest, WriteCallbackSuccess);

  base::FilePath live_path_;
  base::FilePath rotated_path_;
  uint64_t max_size_;
  std::string header_;
  uint64_t cur_size_ = 0;
  int fd_ = -1;

  DISALLOW_COPY_AND_ASSIGN(VmlogFile);
};

// Reads information from /proc/vmstat periodically and writes summary data to
// vmlog. VmlogWriter manages output file and symlink creation and automatically
// rotates the underlying files to keep data fresh while keeping a small disk
// footprint.
class VmlogWriter {
 public:
  VmlogWriter(const base::FilePath& vmlog_dir,
              const base::TimeDelta& log_interval);
  ~VmlogWriter();

 private:
  friend class VmlogWriterTest;
  FRIEND_TEST(VmlogWriterTest, WriteCallbackSuccess);

  // Invoked every log_interval by timer_, this callback parses the contents of
  // /proc/vmstat and writes results to vmlog_.
  void WriteCallback();

  std::unique_ptr<VmlogFile> vmlog_;
  int vmstat_fd_;
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(VmlogWriter);
};

}  // namespace chromeos_metrics

#endif  // METRICS_VMLOG_WRITER_H_
