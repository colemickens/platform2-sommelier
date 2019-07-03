// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_VMLOG_WRITER_H_
#define METRICS_VMLOG_WRITER_H_

#include <fstream>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/optional.h>
#include <base/time/time.h>
#include <base/timer/timer.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <pcrecpp.h>

namespace chromeos_metrics {

// Record for retrieving and reporting values from /proc/vmstat
struct VmstatRecord {
  uint64_t page_faults_ = 0;       // major faults
  uint64_t file_page_faults_ = 0;  // major faults for file-backed pages
  uint64_t anon_page_faults_ = 0;  // major faults for anonymous pages
  uint64_t swap_in_ = 0;           // pages swapped in
  uint64_t swap_out_ = 0;          // pages swapped out
};

struct CpuTimeRecord {
  uint64_t non_idle_time_ = 0;
  uint64_t total_time_ = 0;
};

// Parse cumulative vm statistics from data read from /proc/vmstat.  Returns
// true for success.
bool VmStatsParseStats(std::istream* input, struct VmstatRecord* record);

// Parse cpu time from /proc/stat. Returns true for success.
bool ParseCpuTime(std::istream* input, CpuTimeRecord* record);

// Parse online CPU IDs from /proc/cpuinfo. Returns a vector of CPU ID on
// success or base::nullopt on failure.
base::Optional<std::vector<int>> GetOnlineCpus(std::istream& proc_cpuinfo);

// Read the GPU frequency.  Returns true on success or an expected failure.
// @param out: A stream to output the discovered frequency, in MHz.
// @param sclk_stream: A stream from which to read the GPU shader clock
// frequency.
bool ParseAmdgpuFrequency(std::ostream& out, std::istream& sclk_stream);

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

  // Called by the constructor to initialize internals. May schedule itself on
  // valid_time_delay_timer_ if system clock doesn't look correct.
  void Init(const base::FilePath& vmlog_dir,
            const base::TimeDelta& log_interval);

  // Invoked every log_interval by timer_, this callback parses the contents of
  // /proc/vmstat and writes results to vmlog_.
  void WriteCallback();

  // Calculate the difference of Vmstat between two consecutive calls to this
  // function.
  bool GetDeltaVmStat(VmstatRecord* delta_out);

  // Calculate the CPU usage (a percentage of total CPU used) during consecutive
  // calls to this function.
  bool GetCpuUsage(double* cpu_usage_out);

  // Read the CPU frequencies.  Returns true on success.
  bool GetCpuFrequencies(std::ostream& out);

  // Read the GPU frequency.  Returns true on success or an expected failure.
  bool GetAmdgpuFrequency(std::ostream& out);

  std::unique_ptr<VmlogFile> vmlog_;
  // Stream used to read content in /proc/vmstat.
  std::ifstream vmstat_stream_;
  // Stream used to read content in /proc/stat.
  std::ifstream proc_stat_stream_;
  // Record (partial) content read in from /proc/vmstat last time.
  VmstatRecord prev_vmstat_record_;
  // Record cpu stat info read in from /proc/stat last time.
  CpuTimeRecord prev_cputime_record_;

  // A set of open entries to sysfs for cpu frequency information.  Each one
  // contains a single integer, in kHz.
  std::vector<std::ifstream> cpufreq_streams_;

  // A (possibly invalid) stream to the amdgpu shader clock frequency
  std::ifstream amdgpu_sclk_stream_;

  base::RepeatingTimer timer_;
  base::OneShotTimer valid_time_delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(VmlogWriter);
};

}  // namespace chromeos_metrics

#endif  // METRICS_VMLOG_WRITER_H_
