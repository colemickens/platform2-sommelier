// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/memory_log.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctime>
#include <iomanip>
#include <string>

#include <base/file_path.h>
#include <base/file_util.h>

#include "shill/logging.h"
#include "shill/shill_time.h"

namespace shill {

namespace {

// MemoryLog needs to be a 'leaky' singleton as it needs to survive to
// handle logging till the very end of the shill process. Making MemoryLog
// leaky is fine as it does not need to clean up or release any resource at
// destruction.
base::LazyInstance<MemoryLog>::Leaky g_memory_log = LAZY_INSTANCE_INITIALIZER;

// Can't get to these in base/logging.c.
const char *const kLogSeverityNames[logging::LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "ERROR_REPORT", "FATAL"
};

const char kLoggedInUserName[] = "chronos";
}  // namespace

const char MemoryLog::kDefaultLoggedInDumpPath[] =
    "/home/chronos/user/log/connectivity.log";

const char MemoryLog::kDefaultLoggedOutDumpPath[] =
    "/var/log/connectivity.log";

const char MemoryLog::kLoggedInTokenPath[] =
    "/var/run/state/logged-in";

// static
MemoryLog *MemoryLog::GetInstance() {
  return g_memory_log.Pointer();
}

MemoryLog::MemoryLog()
    : maximum_size_bytes_(kDefaultMaximumMemoryLogSizeInBytes),
      current_size_bytes_(0),
      maximum_disk_log_size_bytes_(kDefaultMaxDiskLogSizeInBytes) { }

void MemoryLog::Append(const std::string &msg) {
  current_size_bytes_ += msg.size();
  log_.push_back(msg);
  ShrinkToTargetSize(maximum_size_bytes_);
}

void MemoryLog::Clear() {
  current_size_bytes_ = 0;
  log_.clear();
}

void MemoryLog::FlushToDisk() {
  if (file_util::PathExists(FilePath(kLoggedInTokenPath))) {
    FlushToDiskImpl(FilePath(kDefaultLoggedInDumpPath));
  } else {
    FlushToDiskImpl(FilePath(kDefaultLoggedOutDumpPath));
  }
}

void MemoryLog::FlushToDiskImpl(const FilePath &file_path) {
  do {
    // If the file exists, lets make sure it is of reasonable size before
    // writing to it, and roll it over if it's too big.
    if (!file_util::PathExists(file_path)) {
      // No existing file means we can write without worry to a new file.
      continue;
    }
    int64_t file_size = -1;
    if (!file_util::GetFileSize(file_path, &file_size) || (file_size < 0)) {
      LOG(ERROR) << "Failed to get size of existing memory log dump.";
      return;
    }
    FilePath backup_path = file_path.ReplaceExtension(".bak");
    if (static_cast<uint64_t>(file_size) < maximum_disk_log_size_bytes_) {
      // File existed, but was below our threshold.
      continue;
    }
    if (!file_util::Move(file_path, backup_path)) {
      LOG(ERROR) << "Failed to move overly large memory log on disk from "
                 << file_path.value() << " to " << backup_path.value();
      return;
    }
  } while (false);

  if (FlushToFile(file_path) < 0) {
    LOG(ERROR) << "Failed to flush memory log to disk";
  }
  // We don't want to see messages twice.
  Clear();
}

ssize_t MemoryLog::FlushToFile(const FilePath &file_path) {
  FILE *f = file_util::OpenFile(file_path, "a");
  if (!f) {
    LOG(ERROR) << "Failed to open file for dumping memory log to disk.";
    return -1;
  }
  file_util::ScopedFILE file_closer(f);
  long maximum_pw_string_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (maximum_pw_string_size < 0) {
    LOG(ERROR) << "Setup for changing ownership of memory log file failed";
    return -1;
  }
  struct passwd chronos;
  struct passwd *pwresult = NULL;
  scoped_array<char> buf(new char[maximum_pw_string_size]);
  if (getpwnam_r(kLoggedInUserName,
                 &chronos,
                 buf.get(),
                 maximum_pw_string_size,
                 &pwresult) || pwresult == NULL) {
    PLOG(ERROR) << "Failed to find user " << kLoggedInUserName
                << " for memory log dump.";
    return -1;
  }
  if (chown(file_path.value().c_str(), chronos.pw_uid, chronos.pw_gid)) {
    PLOG(WARNING) << "Failed to change ownership of memory log file.";
  }
  if (chmod(file_path.value().c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) {
    PLOG(WARNING) << "Failed to change permissions of memory log file.  Error: "
                 << strerror(errno);
  }
  ssize_t bytes_written = 0;
  std::deque<std::string>::iterator it;
  for (it = log_.begin(); it != log_.end(); it++) {
    bytes_written += fwrite(it->c_str(), 1, it->size(), f);
    if (ferror(f)) {
      LOG(ERROR) << "Write to memory log dump file failed.";
      return -1;
    }
  }

  return bytes_written;
}

void MemoryLog::SetMaximumSize(size_t size_in_bytes) {
  ShrinkToTargetSize(size_in_bytes);
  maximum_size_bytes_ = size_in_bytes;
}

void MemoryLog::ShrinkToTargetSize(size_t number_bytes) {
  while (current_size_bytes_ > number_bytes) {
    const std::string &front = log_.front();
    current_size_bytes_ -= front.size();
    log_.pop_front();
  }
}

bool MemoryLog::TestContainsMessageWithText(const char *msg) {
  std::deque<std::string>::const_iterator it;
  for (it = log_.begin(); it != log_.end(); ++it) {
    if (it->find(msg) != std::string::npos) {
      return true;
    }
  }
  return false;
}

MemoryLogMessage::MemoryLogMessage(const char *file,
                                   int line,
                                   logging::LogSeverity severity,
                                   bool propagate_down)
    : file_(file),
      line_(line),
      severity_(severity),
      propagate_down_(propagate_down),
      message_start_(0) {
  Init();
}

MemoryLogMessage::~MemoryLogMessage() {
  if (propagate_down_) {
    ::logging::LogMessage(file_, line_, severity_).stream()
        << &(stream_.str()[message_start_]);
  }
  stream_ << std::endl;
  MemoryLog::GetInstance()->Append(stream_.str());
}

// This owes heavily to the base/logging.cc:LogMessage implementation, but
// without as much customization.  Unfortunately, there isn't a good way to
// get into that code without drastically changing how base/logging works. It
// isn't exactly rocket science in any case.
void MemoryLogMessage::Init() {
  const char *filename = strrchr(file_, '/');
  if (!filename) {
    filename = file_;
  }

  // Just log a timestamp, number of ticks, severity, and a file name.
  struct timeval tv = {0};
  struct tm local_time = {0};
  Time::GetInstance()->GetTimeOfDay(&tv, NULL);
  localtime_r(&tv.tv_sec, &local_time);
  stream_ << std::setfill('0')
          << local_time.tm_year + 1900
          << '-' << std::setw(2) << local_time.tm_mon + 1
          << '-' << std::setw(2) << local_time.tm_mday
          << 'T' << std::setw(2) << local_time.tm_hour
          << ':' << std::setw(2) << local_time.tm_min
          << ':' << std::setw(2) << local_time.tm_sec
          << '.' << tv.tv_usec << ' ';

  if (severity_ >= 0)
    stream_ << kLogSeverityNames[severity_];
  else
    stream_ << "VERBOSE" << -severity_;
  stream_ << ":" << filename << "(" << line_ << ") ";

  message_start_ = stream_.tellp();
}

}  // namespace shill
