// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/fake_samba_interface.h"
#include "smbprovider/smbprovider.h"
#include "smbprovider/smbprovider_helper.h"

#include <base/macros.h>
#include <brillo/any.h>
#include <errno.h>

namespace smbprovider {

namespace {

constexpr mode_t kFileMode = 33188;  // File entry
constexpr time_t kFileModTime = 12345678;

}  // namespace

FakeSambaInterface::EntryMap::const_iterator
FakeSambaInterface::FakeDirectory::FindEntryByFullPath(
    const base::FilePath& full_path) const {
  if (full_path.DirName().value() != path) {
    return entries.end();
  }
  return entries.find(full_path.BaseName().value());
}

FakeSambaInterface::FakeSambaInterface() {}

int32_t FakeSambaInterface::OpenDirectory(const std::string& directory_path,
                                          int32_t* dir_id) {
  DCHECK(dir_id);
  *dir_id = -1;
  for (auto& dir_iter : directory_map_) {
    FakeDirectory& dir = dir_iter.second;
    if (dir.path == directory_path) {
      if (dir.is_open) {
        return ENOENT;
      }
      dir.is_open = true;
      dir.current_entry = dir.entries.empty() ? "" : dir.entries.begin()->first;
      *dir_id = dir.dir_id;
      return 0;
    }
  }
  return ENOENT;
}

int32_t FakeSambaInterface::CloseDirectory(int32_t dir_id) {
  auto dir_iter = directory_map_.find(dir_id);
  if (dir_iter == directory_map_.end()) {
    return EBADF;
  }
  FakeDirectory& dir = dir_iter->second;
  if (!dir.is_open) {
    return EBADF;
  }
  dir.is_open = false;
  dir.current_entry = "";
  return 0;
}

int32_t FakeSambaInterface::GetDirectoryEntries(int32_t dir_id,
                                                smbc_dirent* dirp,
                                                int32_t dirp_buffer_size,
                                                int32_t* bytes_read) {
  DCHECK(dirp);
  DCHECK(bytes_read);
  *bytes_read = 0;
  const auto& dir_iter = directory_map_.find(dir_id);
  if (dir_iter == directory_map_.end()) {
    return EBADF;
  }
  FakeDirectory& dir = dir_iter->second;
  if (!dir.is_open) {
    return EBADF;
  }
  while (dir.HasMoreEntries()) {
    auto iter = dir.entries.find(dir.current_entry);
    FakeEntry& entry = iter->second;
    if (!WriteEntry(entry.name, entry.smbc_type, dirp_buffer_size - *bytes_read,
                    dirp)) {
      // WriteEntry will fail if the buffer size is not large enough to fit the
      // next entry. This is a valid case and will return with no error.
      return 0;
    }
    *bytes_read += dirp->dirlen;
    DCHECK_GE(dirp_buffer_size, *bytes_read);
    dirp = AdvanceDirEnt(dirp);
    iter++;
    dir.current_entry = iter == dir.entries.end() ? "" : iter->first;
  }
  return 0;
}

int32_t FakeSambaInterface::GetEntryStatus(const std::string& entry_path,
                                           struct stat* stat) {
  DCHECK(stat);
  const base::FilePath full_path(entry_path);
  for (const auto& dir_iter : directory_map_) {
    const FakeDirectory& dir = dir_iter.second;
    EntryMap::const_iterator entry_iter = dir.FindEntryByFullPath(full_path);
    if (entry_iter == dir.entries.end()) {
      continue;
    }
    const FakeEntry& entry = entry_iter->second;
    stat->st_size = entry.size;
    stat->st_mode = kFileMode;
    stat->st_mtime = kFileModTime;
    return 0;
  }
  return ENOENT;
}

int32_t FakeSambaInterface::AddDirectory(const std::string& path) {
  int32_t dir_id = dir_id_counter_++;
  directory_map_.emplace(dir_id, FakeDirectory(dir_id, path));
  return dir_id;
}

void FakeSambaInterface::AddEntry(int32_t dir_id,
                                  const std::string& entry_name,
                                  uint32_t type,
                                  size_t size) {
  auto dir_iter = directory_map_.find(dir_id);
  DCHECK(dir_iter != directory_map_.end());
  FakeDirectory& dir = dir_iter->second;
  // Check to make sure we aren't adding an entry to a currently opened
  // directory.
  DCHECK(dir.current_entry.empty());
  DCHECK(dir.entries.find(entry_name) == dir.entries.end());
  dir.entries.emplace(entry_name, FakeEntry(entry_name, type, size));
}

bool FakeSambaInterface::HasOpenDirectories() const {
  for (const auto& dirIter : directory_map_) {
    if (dirIter.second.is_open) {
      return true;
    }
  }
  return false;
}

}  // namespace smbprovider
