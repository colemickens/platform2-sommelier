// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/iterator/directory_iterator.h"

#include <utility>

#include <base/logging.h>

#include "smbprovider/constants.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {
namespace {

constexpr int32_t kNoMoreEntriesError = -1;

}  // namespace

BaseDirectoryIterator::BaseDirectoryIterator(const std::string& dir_path,
                                             SambaInterface* samba_interface)
    : BaseDirectoryIterator(dir_path, samba_interface, kDirEntBufferSize) {}

BaseDirectoryIterator::BaseDirectoryIterator(const std::string& dir_path,
                                             SambaInterface* samba_interface,
                                             size_t buffer_size)
    : dir_path_(dir_path),
      dir_buf_(buffer_size),
      samba_interface_(samba_interface) {}

BaseDirectoryIterator::BaseDirectoryIterator(BaseDirectoryIterator&& other)
    : dir_path_(std::move(other.dir_path_)),
      dir_buf_(std::move(other.dir_buf_)),
      entries_(std::move(other.entries_)),
      current_entry_index_(other.current_entry_index_),
      dir_id_(other.dir_id_),
      is_done_(other.is_done_),
      is_initialized_(other.is_initialized_),
      samba_interface_(other.samba_interface_) {
  other.dir_id_ = -1;
  other.is_initialized_ = false;
  other.samba_interface_ = nullptr;
}

BaseDirectoryIterator::~BaseDirectoryIterator() {
  if (dir_id_ != -1) {
    CloseDirectory();
  }
}

int32_t BaseDirectoryIterator::Init() {
  DCHECK(!is_initialized_);

  int32_t open_dir_error = OpenDirectory();
  if (open_dir_error != 0) {
    return open_dir_error;
  }
  is_initialized_ = true;
  return Next();
}

int32_t BaseDirectoryIterator::Next() {
  DCHECK(is_initialized_);
  DCHECK(!is_done_);

  ++current_entry_index_;
  if (current_entry_index_ >= entries_.size()) {
    int32_t result = FillBuffer();
    if (result != 0 && result != kNoMoreEntriesError) {
      return result;
    }
  }
  return 0;
}

const DirectoryEntry& BaseDirectoryIterator::Get() {
  DCHECK(is_initialized_);
  DCHECK(!is_done_);
  DCHECK_LT(current_entry_index_, entries_.size());

  return entries_[current_entry_index_];
}

bool BaseDirectoryIterator::IsDone() {
  DCHECK(is_initialized_);
  return is_done_;
}

int32_t BaseDirectoryIterator::OpenDirectory() {
  DCHECK_EQ(-1, dir_id_);
  return samba_interface_->OpenDirectory(dir_path_, &dir_id_);
}

void BaseDirectoryIterator::CloseDirectory() {
  DCHECK_NE(-1, dir_id_);
  int32_t result = samba_interface_->CloseDirectory(dir_id_);
  if (result != 0) {
    LOG(ERROR) << "BaseDirectoryIterator: CloseDirectory failed with error: "
               << GetErrorFromErrno(result);
  }
  dir_id_ = -1;
}

int32_t BaseDirectoryIterator::FillBuffer() {
  int32_t bytes_read;
  int32_t fetch_error = ReadEntriesToBuffer(&bytes_read);
  if (fetch_error != 0) {
    return fetch_error;
  }

  ConvertBufferToVector(bytes_read);

  if (entries_.empty()) {
    // Succeeded but nothing valid left to read.
    is_done_ = true;
    return kNoMoreEntriesError;
  }

  return 0;
}

int32_t BaseDirectoryIterator::ReadEntriesToBuffer(int32_t* bytes_read) {
  return samba_interface_->GetDirectoryEntries(
      dir_id_, GetDirentFromBuffer(dir_buf_.data()), dir_buf_.size(),
      bytes_read);
}

void BaseDirectoryIterator::ConvertBufferToVector(int32_t bytes_read) {
  entries_.clear();
  current_entry_index_ = 0;
  const smbc_dirent* dirent = GetDirentFromBuffer(dir_buf_.data());

  int32_t bytes_left = bytes_read;
  while (bytes_left > 0) {
    AddEntryIfValid(*dirent);
    DCHECK_GT(dirent->dirlen, 0);
    DCHECK_GE(bytes_left, dirent->dirlen);

    bytes_left -= dirent->dirlen;
    dirent = AdvanceConstDirEnt(dirent);
    DCHECK(dirent);
  }
  DCHECK_EQ(bytes_left, 0);
}

void BaseDirectoryIterator::AddEntryIfValid(const smbc_dirent& dirent) {
  const std::string name(dirent.name);
  // Ignore "." and ".." entries.
  // TODO(allenvic): Handle SMBC_LINK
  if (IsSelfOrParentDir(name) || !ShouldIncludeEntryType(dirent.smbc_type)) {
    return;
  }

  bool is_directory =
      dirent.smbc_type == SMBC_DIR || dirent.smbc_type == SMBC_FILE_SHARE;
  entries_.emplace_back(is_directory, name, AppendPath(dir_path_, name));
}

}  // namespace smbprovider
