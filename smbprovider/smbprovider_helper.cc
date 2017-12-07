// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bits.h>
#include <base/strings/string_piece.h>
#include <libsmbclient.h>

#include "smbprovider/constants.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

void AddEntryIfValid(const smbc_dirent& dirent,
                     DirectoryEntryList* directory_entries) {
  const std::string name(dirent.name);
  // Ignore "." and ".." entries.
  // TODO(allenvic): Handle SMBC_LINK
  if (IsSelfOrParentDir(name) || !ShouldProcessEntryType(dirent.smbc_type)) {
    return;
  }
  DirectoryEntry* entry = directory_entries->add_entries();
  bool is_directory = dirent.smbc_type == SMBC_DIR;
  entry->set_is_directory(is_directory);
  entry->set_name(name);
  entry->set_size(-1);
  entry->set_last_modified_time(-1);
}

smbc_dirent* AdvanceDirEnt(smbc_dirent* dirp) {
  DCHECK(dirp);
  DCHECK_GE(dirp->dirlen, sizeof(smbc_dirent));
  return reinterpret_cast<smbc_dirent*>(reinterpret_cast<uint8_t*>(dirp) +
                                        dirp->dirlen);
}

std::string AppendPath(const std::string& base_path,
                       const std::string& relative_path) {
  const base::FilePath path(base_path);
  base::FilePath relative(relative_path);
  if (relative.IsAbsolute() && relative_path.size() > 0) {
    // Remove the beginning "/" since FilePath#Append() cannot append an
    // 'absolute' path.
    relative = base::FilePath(
        base::StringPiece(relative_path.c_str() + 1, relative_path.size() - 1));
  }
  return path.Append(relative).value();
}

size_t CalculateEntrySize(const std::string& entry_name) {
  return base::bits::Align(sizeof(smbc_dirent) + entry_name.size(),
                           alignof(smbc_dirent));
}

smbc_dirent* GetDirentFromBuffer(uint8_t* buffer) {
  return reinterpret_cast<smbc_dirent*>(buffer);
}

bool IsSelfOrParentDir(const std::string& entry_name) {
  return entry_name == kEntrySelf || entry_name == kEntryParent;
}

bool ShouldProcessEntryType(uint32_t smbc_type) {
  return smbc_type == SMBC_FILE || smbc_type == SMBC_DIR;
}

bool WriteEntry(const std::string& entry_name,
                int32_t entry_type,
                size_t buffer_size,
                smbc_dirent* dirp) {
  DCHECK(dirp);
  size_t entry_size = CalculateEntrySize(entry_name);
  if (entry_size > buffer_size) {
    return false;
  }
  dirp->smbc_type = entry_type;
  dirp->dirlen = entry_size;
  memcpy(dirp->name, entry_name.c_str(), entry_name.size() + 1);
  return true;
}

}  // namespace smbprovider
