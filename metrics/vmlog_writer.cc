// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/vmlog_writer.h"

#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/daemons/daemon.h>

namespace chromeos_metrics {
namespace {

constexpr char kVmlogHeader[] =
  "time pgmajfault pgmajfault_f pgmajfault_a pswpin pswpout\n";

// We limit the size of vmlog log files to keep frequent logging from wasting
// disk space.
constexpr int kMaxVmlogFileSize = 256 * 1024;

}  // namespace

bool VmStatsParseStats(const std::string& stats, struct VmstatRecord* record) {
  // a mapping of string name to field in VmstatRecord and whether we found it
  struct Mapping {
    const std::string name;
    uint64_t* value_p;
    bool found;
    bool optional;
  } map[] = {
      {.name = "pgmajfault",
       .value_p = &record->page_faults_,
       .found = false,
       .optional = false},
      // pgmajfault_f and pgmajfault_a may not be present in all kernels.
      // Don't fuss if they are not.
      {.name = "pgmajfault_f",
       .value_p = &record->file_page_faults_,
       .found = false,
       .optional = true},
      {.name = "pgmajfault_a",
       .value_p = &record->anon_page_faults_,
       .found = false,
       .optional = true},
      {.name = "pswpin",
       .value_p = &record->swap_in_,
       .found = false,
       .optional = false},
      {.name = "pswpout",
       .value_p = &record->swap_out_,
       .found = false,
       .optional = false},
  };

  // Each line in the file has the form
  // <ID> <VALUE>
  // for instance:
  // nr_free_pages 213427
  std::vector<std::string> lines = base::SplitString(
      stats, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    std::vector<std::string> tokens = base::SplitString(
        line, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (tokens.size() != 2u) {
      LOG(WARNING) << "Unexpected vmstat format in line: " << line;
      continue;
    }
    for (auto& mapping : map) {
      if (!tokens[0].compare(mapping.name)) {
        if (!base::StringToUint64(tokens[1], mapping.value_p))
          return false;
        mapping.found = true;
      }
    }
  }
  // Make sure we got all the stats, except the optional ones.
  for (const auto& mapping : map) {
    if (!mapping.found) {
      if (mapping.optional) {
        *mapping.value_p = 0;
      } else {
        LOG(WARNING) << "vmstat missing " << mapping.name;
        return false;
      }
    }
  }
  return true;
}

VmlogFile::VmlogFile(const base::FilePath& live_path,
                     const base::FilePath& rotated_path,
                     const uint64_t max_size,
                     const std::string& header)
    : live_path_(live_path),
      rotated_path_(rotated_path),
      max_size_(max_size),
      header_(header) {
  fd_ = open(live_path_.value().c_str(), O_CREAT | O_RDWR | O_EXCL, 0644);
  if (fd_ != -1) {
    Write(header_);
  } else {
    PLOG(ERROR) << "Failed to open file: " << live_path_.value();
  }
}

VmlogFile::~VmlogFile() = default;

bool VmlogFile::Write(const std::string& data) {
  if (fd_ == -1)
    return false;

  if (cur_size_ + data.size() > max_size_) {
    if (!base::CopyFile(live_path_, rotated_path_)) {
      PLOG(ERROR) << "Could not copy vmlog to: " << rotated_path_.value();
    }
    base::FilePath rotated_path_dir = rotated_path_.DirName();
    base::FilePath rotated_symlink = rotated_path_dir.Append("vmlog.1.LATEST");
    if (!base::PathExists(rotated_symlink)) {
      if (!base::CreateSymbolicLink(rotated_path_, rotated_symlink)) {
        PLOG(ERROR) << "Unable to create symbolic link from "
                    << rotated_symlink.value() << " to "
                    << rotated_path_.value();
      }
    }

    if (HANDLE_EINTR(ftruncate(fd_, 0)) != 0) {
      PLOG(ERROR) << "Could not ftruncate() file";
      return false;
    }
    if (HANDLE_EINTR(lseek(fd_, 0, SEEK_SET)) != 0) {
      PLOG(ERROR) << "Could not lseek() file";
      return false;
    }
    cur_size_ = 0;
    if (!Write(header_)) {
      return false;
    }
  }

  if (!base::WriteFileDescriptor(fd_, data.c_str(), data.size())) {
    return false;
  }
  cur_size_ += data.size();
  return true;
}

VmlogWriter::VmlogWriter(const base::FilePath& vmlog_dir,
                         const base::TimeDelta& log_interval) {
  if (!base::DirectoryExists(vmlog_dir)) {
    if (!base::CreateDirectory(vmlog_dir)) {
      PLOG(ERROR) << "Couldn't create " << vmlog_dir.value();
      return;
    }
  }
  if (!base::SetPosixFilePermissions(vmlog_dir, 0755)) {
    PLOG(ERROR) << "Couldn't set permissions for " << vmlog_dir.value();
  }
  Init(vmlog_dir, log_interval);
}

void VmlogWriter::Init(const base::FilePath& vmlog_dir,
                       const base::TimeDelta& log_interval) {
  base::Time now = base::Time::Now();

  // If the current time is within a day of the epoch, we probably don't have a
  // good time set for naming files. Wait 5 minutes.
  //
  // See crbug.com/724175 for details.
  if (now - base::Time::UnixEpoch() < base::TimeDelta::FromDays(1)) {
    LOG(WARNING) << "Time seems incorrect, too close to epoch: " << now;
    valid_time_delay_timer_.Start(FROM_HERE,
                                  base::TimeDelta::FromMinutes(5),
                                  base::Bind(&VmlogWriter::Init,
                                             base::Unretained(this),
                                             vmlog_dir,
                                             log_interval));
    return;
  }

  base::FilePath vmlog_current_path =
      vmlog_dir.Append("vmlog." + brillo::GetTimeAsLogString(now));
  base::FilePath vmlog_rotated_path =
      vmlog_dir.Append("vmlog.1." + brillo::GetTimeAsLogString(now));

  brillo::UpdateLogSymlinks(vmlog_dir.Append("vmlog.LATEST"),
                            vmlog_dir.Append("vmlog.PREVIOUS"),
                            vmlog_current_path);

  base::DeleteFile(vmlog_dir.Append("vmlog.1.PREVIOUS"), false);
  if (base::PathExists(vmlog_dir.Append("vmlog.1.LATEST"))) {
    base::Move(vmlog_dir.Append("vmlog.1.LATEST"),
               vmlog_dir.Append("vmlog.1.PREVIOUS"));
  }

  vmlog_.reset(new VmlogFile(
      vmlog_current_path, vmlog_rotated_path, kMaxVmlogFileSize, kVmlogHeader));

  vmstat_fd_ = open("/proc/vmstat", O_RDONLY);
  if (vmstat_fd_ < 0) {
    PLOG(ERROR) << "Couldn't open /proc/vmstat";
    return;
  }

  if (!log_interval.is_zero()) {
    timer_.Start(FROM_HERE, log_interval, this, &VmlogWriter::WriteCallback);
  }
}

VmlogWriter::~VmlogWriter() = default;

void VmlogWriter::WriteCallback() {
  if (lseek(vmstat_fd_, 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Unable to lseek() /proc/vmstat";
    timer_.Stop();
    return;
  }

  // Assume that vmstat output will be no larger than 8K-1. Poking around some
  // chromebooks, the size seems to generally be around 2K.
  char buf[8192];
  int len = HANDLE_EINTR(read(vmstat_fd_, buf, arraysize(buf) - 1));
  if (len < 0) {
    PLOG(ERROR) << "Unable to read() /proc/vmstat";
    timer_.Stop();
    return;
  }

  // Null terminate the data we've read
  buf[len] = 0;

  VmstatRecord r;
  if (!VmStatsParseStats(buf, &r)) {
    LOG(ERROR) << "Unable to parse vmstat data";
    timer_.Stop();
    return;
  }

  uint64_t delta_page_faults = r.page_faults_ - previous_record_.page_faults_;
  uint64_t delta_file_page_faults =
      r.file_page_faults_ - previous_record_.file_page_faults_;
  uint64_t delta_anon_page_faults =
      r.anon_page_faults_ - previous_record_.anon_page_faults_;
  uint64_t delta_swap_in = r.swap_in_ - previous_record_.swap_in_;
  uint64_t delta_swap_out = r.swap_out_ - previous_record_.swap_out_;

  timeval tv;
  gettimeofday(&tv, nullptr);
  struct tm tm_time;
  localtime_r(&tv.tv_sec, &tm_time);
  std::string out_line = base::StringPrintf(
      "[%02d%02d/%02d%02d%02d]"
      " %" PRIu64 " %" PRIu64" %" PRIu64" %" PRIu64 " %" PRIu64 "\n",
      tm_time.tm_mon + 1,
      tm_time.tm_mday,
      tm_time.tm_hour,
      tm_time.tm_min,
      tm_time.tm_sec,
      delta_page_faults,
      delta_file_page_faults,
      delta_anon_page_faults,
      delta_swap_in,
      delta_swap_out);

  if (!vmlog_->Write(out_line)) {
    LOG(ERROR) << "Writing to vmlog failed.";
    timer_.Stop();
    return;
  }
  previous_record_ = r;
}

}  // namespace chromeos_metrics
