// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_bouncer/rule_db_storage.h"
#include <unistd.h>

#include <string>

#include <base/files/file_util.h>

#include "usb_bouncer/util.h"

namespace usb_bouncer {

namespace {
constexpr size_t kMaxFileSize = 64 * 1024 * 1024;
}  // namespace

RuleDBStorage::RuleDBStorage() {}

RuleDBStorage::RuleDBStorage(const base::FilePath& db_dir) {
  path_ = GetDBPath(db_dir);
  fd_ = OpenPath(path_, true /* lock */);
  Reload();
}

RuleDB& RuleDBStorage::Get() {
  return *val_.get();
}

const RuleDB& RuleDBStorage::Get() const {
  return *val_.get();
}

const base::FilePath& RuleDBStorage::path() const {
  return path_;
}

bool RuleDBStorage::Valid() const {
  return !path_.empty() && fd_.is_valid() && val_ != nullptr;
}

bool RuleDBStorage::Persist() {
  if (!Valid()) {
    LOG(ERROR) << "Called Persist() on invalid RuleDBStorage";
    return false;
  }

  if (lseek(fd_.get(), 0, SEEK_SET) != 0) {
    PLOG(ERROR) << "Failed to rewind DB";
    return false;
  }

  std::string serialized = val_->SerializeAsString();
  if (!base::WriteFileDescriptor(fd_.get(), serialized.data(),
                                 serialized.size())) {
    PLOG(ERROR) << "Failed to write proto to file!";
    return false;
  }

  if (HANDLE_EINTR(ftruncate(fd_.get(), serialized.size())) != 0) {
    PLOG(ERROR) << "Failed to truncate file";
    return false;
  }

  return true;
}

bool RuleDBStorage::Reload() {
  val_ = nullptr;

  // Get the file size.
  off_t file_size = lseek(fd_.get(), 0, SEEK_END);
  if (file_size == -1) {
    PLOG(ERROR) << "Failed to get DB size";
    return false;
  }
  if (file_size > kMaxFileSize) {
    LOG(ERROR) << "DB is too big!";
    return false;
  }

  // Read the file.
  if (lseek(fd_.get(), 0, SEEK_SET) != 0) {
    PLOG(ERROR) << "Failed to rewind DB";
    return false;
  }
  auto buf = std::make_unique<char[]>(file_size);
  if (!base::ReadFromFD(fd_.get(), buf.get(), file_size)) {
    PLOG(ERROR) << "Failed to read DB";
    return false;
  }

  // Parse the results.
  val_ = std::make_unique<RuleDB>();
  if (!val_->ParseFromArray(buf.get(), file_size)) {
    LOG(ERROR) << "Error parsing DB. Regenerating...";
    val_ = std::make_unique<RuleDB>();
  }
  return true;
}

}  // namespace usb_bouncer
