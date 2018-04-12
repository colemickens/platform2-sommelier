// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/persistent_integer.h"

#include <fcntl.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_util.h>

#include "metrics/metrics_library.h"

namespace {

// The directory for the persistent storage.
constexpr char kBackingFilesDirectory[] = "/var/lib/metrics/";
// Valid characters in the name of a persistent integer.
constexpr char kValidPersistentIntegerNameCharacters[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789_.";

}  // namespace

namespace chromeos_metrics {

// Static class member instantiation.
bool PersistentInteger::testing_ = false;

PersistentInteger::PersistentInteger(const std::string& name)
    : value_(0), version_(kVersion), name_(name), synced_(false) {
  if (!base::ContainsOnlyChars(name, kValidPersistentIntegerNameCharacters))
      LOG(FATAL) << "invalid persistent integer name \"" << name << "\"";
  backing_file_name_ = GetBackingFilesDirectory() + name_;
}

PersistentInteger::~PersistentInteger() {}

// static
std::string PersistentInteger::GetBackingFilesDirectory() {
  std::string empty_name("");
  return GetAndMaybeSetBackingFilesDirectory(empty_name);
}

// static
std::string PersistentInteger::GetAndMaybeSetBackingFilesDirectory(
    const std::string& dir_name) {
  static std::string backing_files_dir = std::string(kBackingFilesDirectory);
  if (!dir_name.empty()) {
    CHECK(testing_) << "can only set the backing files directory when testing";
    backing_files_dir = dir_name;
  }
  return backing_files_dir;
}

void PersistentInteger::Set(int64_t value) {
  value_ = value;
  Write();
}

int64_t PersistentInteger::Get() {
  // If not synced, then read.  If the read fails, it's a good idea to write.
  if (!synced_ && !Read())
    Write();
  return value_;
}

int64_t PersistentInteger::GetAndClear() {
  int64_t v = Get();
  Set(0);
  return v;
}

void PersistentInteger::Add(int64_t x) {
  Set(Get() + x);
}

void PersistentInteger::Write() {
  int fd = HANDLE_EINTR(open(backing_file_name_.c_str(),
                             O_WRONLY | O_CREAT | O_TRUNC,
                             S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH));
  PCHECK(fd >= 0) << "cannot open " << backing_file_name_ << " for writing";
  PCHECK((HANDLE_EINTR(write(fd, &version_, sizeof(version_))) ==
          sizeof(version_)) &&
         (HANDLE_EINTR(write(fd, &value_, sizeof(value_))) == sizeof(value_)))
      << "cannot write to " << backing_file_name_;
  close(fd);
  synced_ = true;
}

bool PersistentInteger::Read() {
  int fd = HANDLE_EINTR(open(backing_file_name_.c_str(), O_RDONLY));
  if (fd < 0) {
    PLOG(WARNING) << "cannot open " << backing_file_name_ << " for reading";
    return false;
  }
  int32_t version;
  int64_t value;
  bool read_succeeded = false;
  if (HANDLE_EINTR(read(fd, &version, sizeof(version))) == sizeof(version) &&
      version == version_ &&
      HANDLE_EINTR(read(fd, &value, sizeof(value))) == sizeof(value)) {
    value_ = value;
    read_succeeded = true;
    synced_ = true;
  }
  close(fd);
  return read_succeeded;
}

void PersistentInteger::SetTestingMode(const std::string& backing_files_dir) {
  testing_ = true;
  GetAndMaybeSetBackingFilesDirectory(backing_files_dir);
}

}  // namespace chromeos_metrics
