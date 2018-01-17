// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/fake_samba_interface.h"

#include <errno.h>

#include <algorithm>

#include <base/macros.h>
#include <brillo/any.h>

#include "smbprovider/smbprovider.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {
namespace {

constexpr mode_t kFileMode = 33188;  // File entry
constexpr mode_t kDirMode = 16877;   // Dir entry
constexpr char kSmbUrlScheme[] = "smb://";

using PathParts = const std::vector<std::string>;

// Returns the components of a filepath as a vector<std::string>.
std::vector<std::string> SplitPath(const std::string& full_path) {
  base::FilePath path(full_path);
  std::vector<std::string> result;
  path.GetComponents(&result);
  return result;
}

// Removes smb:// from url.
std::string RemoveURLScheme(const std::string& smb_url) {
  DCHECK_EQ(0, smb_url.compare(0, 6, kSmbUrlScheme));
  return smb_url.substr(5, std::string::npos);
}

// Returns the file component of a path.
std::string GetFileName(const std::string& full_path) {
  base::FilePath file_path(RemoveURLScheme(full_path));
  return file_path.BaseName().value();
}

// Returns a string representing the filepath to the directory above the file.
std::string GetDirPath(const std::string& full_path) {
  std::string path = RemoveURLScheme(full_path);
  return base::FilePath(path).DirName().value();
}

}  // namespace

FakeSambaInterface::FakeSambaInterface()
    : root(std::make_unique<FakeDirectory>("smb://")) {}

FakeSambaInterface::~FakeSambaInterface() = default;

int32_t FakeSambaInterface::OpenDirectory(const std::string& directory_path,
                                          int32_t* dir_id) {
  DCHECK(dir_id);
  *dir_id = -1;

  int32_t error;
  if (!GetDirectory(RemoveURLScheme(directory_path), &error)) {
    return error;
  }

  *dir_id = AddOpenDirectory(directory_path);
  return 0;
}

int32_t FakeSambaInterface::CloseDirectory(int32_t dir_id) {
  if (!IsDirectoryFDOpen(dir_id)) {
    return EBADF;
  }
  auto open_info_iter = FindOpenFD(dir_id);
  open_fds.erase(open_info_iter);
  return 0;
}

int32_t FakeSambaInterface::GetDirectoryEntries(int32_t dir_id,
                                                smbc_dirent* dirp,
                                                int32_t dirp_buffer_size,
                                                int32_t* bytes_read) {
  DCHECK(dirp);
  DCHECK(bytes_read);
  *bytes_read = 0;

  if (!IsDirectoryFDOpen(dir_id)) {
    return EBADF;
  }

  OpenInfo& open_info = FindOpenFD(dir_id)->second;

  FakeDirectory* directory = GetDirectory(RemoveURLScheme(open_info.full_path));
  DCHECK(directory);

  DCHECK(open_info.current_index <= directory->entries.size());
  while (open_info.current_index < directory->entries.size()) {
    FakeEntry* entry = directory->entries[open_info.current_index].get();
    if (!WriteEntry(entry->name, entry->smbc_type,
                    dirp_buffer_size - *bytes_read, dirp)) {
      // WriteEntry will fail if the buffer size is not large enough to fit the
      // next entry. This is a valid case and will return with no error.
      return 0;
    }
    *bytes_read += dirp->dirlen;
    DCHECK_GE(dirp_buffer_size, *bytes_read);
    dirp = AdvanceDirEnt(dirp);
    ++open_info.current_index;
  }
  return 0;
}

int32_t FakeSambaInterface::GetEntryStatus(const std::string& entry_path,
                                           struct stat* stat) {
  DCHECK(stat);

  FakeEntry* entry = GetEntry(entry_path);
  if (!entry || !entry->IsValidEntryType()) {
    return ENOENT;
  }

  stat->st_size = entry->size;
  stat->st_mode = entry->smbc_type == SMBC_FILE ? kFileMode : kDirMode;
  stat->st_mtime = entry->date;
  return 0;
}

int32_t FakeSambaInterface::OpenFile(const std::string& file_path,
                                     int32_t flags,
                                     int32_t* file_id) {
  DCHECK(file_id);
  *file_id = -1;

  if (!GetFile(file_path)) {
    return ENOENT;
  }

  bool readable = (flags == O_RDONLY || flags == O_RDWR) ? true : false;
  bool writeable = flags == O_RDWR ? true : false;

  *file_id = AddOpenFile(file_path, readable, writeable);
  return 0;
}

int32_t FakeSambaInterface::CloseFile(int32_t file_id) {
  DCHECK_GE(file_id, 0);
  if (!IsFileFDOpen(file_id)) {
    return EBADF;
  }
  auto open_info_iter = FindOpenFD(file_id);
  open_fds.erase(open_info_iter);
  return 0;
}

int32_t FakeSambaInterface::ReadFile(int32_t file_id,
                                     uint8_t* buffer,
                                     size_t buffer_size,
                                     size_t* bytes_read) {
  DCHECK(buffer);
  DCHECK(bytes_read);
  if (!IsFileFDOpen(file_id)) {
    return EBADF;
  }

  OpenInfo& open_info = FindOpenFD(file_id)->second;
  FakeFile* file = GetFile(open_info.full_path);
  DCHECK(file);
  DCHECK(file->has_data);
  DCHECK(file->size == file->data.size());
  DCHECK(open_info.current_index <= file->data.size());

  // Only read up to the end of the file.
  *bytes_read =
      std::min(buffer_size, file->data.size() - open_info.current_index);
  if (*bytes_read == 0) {
    // No need for copy or seek when bytes_read is zero.
    return 0;
  }

  // Copy the buffer and update the offset.
  memcpy(buffer, file->data.data() + open_info.current_index, *bytes_read);
  open_info.current_index += *bytes_read;
  DCHECK(open_info.current_index <= file->data.size());

  return 0;
}

int32_t FakeSambaInterface::Seek(int32_t file_id, int64_t offset) {
  if (!IsFileFDOpen(file_id)) {
    return EBADF;
  }

  OpenInfo& open_info = FindOpenFD(file_id)->second;
  if (offset > GetFile(open_info.full_path)->data.size()) {
    // Returning an error when offset is outside the bounds of the file.
    return EINVAL;
  }

  open_info.current_index = offset;
  return 0;
}

int32_t FakeSambaInterface::Unlink(const std::string& file_path) {
  if (!GetFile(file_path)) {
    return ENOENT;
  }

  FakeDirectory* directory = GetDirectory(GetDirPath(file_path));
  bool result = directory->RemoveEntry(GetFileName(file_path));
  DCHECK(result);
  return 0;
}

int32_t FakeSambaInterface::RemoveDirectory(const std::string& dir_path) {
  FakeDirectory* directory = GetDirectory(RemoveURLScheme(dir_path));
  if (!directory) {
    return ENOENT;
  }
  if (!directory->entries.empty()) {
    return ENOTEMPTY;
  }

  FakeDirectory* parent = GetDirectory(GetDirPath(dir_path));

  bool result = parent->RemoveEntry(GetFileName(dir_path));
  DCHECK(result);
  return 0;
}

FakeSambaInterface::FakeEntry* FakeSambaInterface::FakeDirectory::FindEntry(
    const std::string& name) {
  for (auto&& entry : entries) {
    if (entry->name == name) {
      return entry.get();
    }
  }
  return nullptr;
}

bool FakeSambaInterface::FakeDirectory::IsFileOrEmptyDirectory(
    FakeEntry* entry) const {
  DCHECK(entry->smbc_type == SMBC_FILE || entry->smbc_type == SMBC_DIR);
  if (entry->smbc_type == SMBC_FILE) {
    return true;
  }

  FakeDirectory* directory = static_cast<FakeDirectory*>(entry);
  return directory->entries.empty();
}

bool FakeSambaInterface::FakeDirectory::RemoveEntry(const std::string& name) {
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    if ((*it)->name == name) {
      DCHECK(IsFileOrEmptyDirectory(it->get()));
      entries.erase(it);
      return true;
    }
  }
  return false;
}

FakeSambaInterface::FakeEntry::FakeEntry(const std::string& full_path,
                                         uint32_t smbc_type,
                                         size_t size,
                                         uint64_t date)
    : name(GetFileName(full_path)),
      smbc_type(smbc_type),
      size(size),
      date(date) {}

void FakeSambaInterface::AddDirectory(const std::string& path) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(std::make_unique<FakeDirectory>(path));
}

void FakeSambaInterface::AddFile(const std::string& path) {
  AddFile(path, 0 /* size */);
}

void FakeSambaInterface::AddFile(const std::string& path, size_t size) {
  AddFile(path, size, 0 /* date */);
}
void FakeSambaInterface::AddFile(const std::string& path,
                                 size_t size,
                                 uint64_t date) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(std::make_unique<FakeFile>(path, size, date));
}

void FakeSambaInterface::AddFile(const std::string& path,
                                 uint64_t date,
                                 std::vector<uint8_t> file_data) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(
      std::make_unique<FakeFile>(path, date, std::move(file_data)));
}

void FakeSambaInterface::AddEntry(const std::string& path, uint32_t smbc_type) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(
      std::make_unique<FakeEntry>(path, smbc_type, 0 /* size */, 0 /* date */));
}

FakeSambaInterface::FakeDirectory* FakeSambaInterface::GetDirectory(
    const std::string& full_path) const {
  int32_t error;
  return GetDirectory(full_path, &error);
}

FakeSambaInterface::FakeDirectory* FakeSambaInterface::GetDirectory(
    const std::string& full_path, int32_t* error) const {
  PathParts split_path = SplitPath(full_path);

  FakeDirectory* current = root.get();
  DCHECK(current);

  // i = 0 represents the root directory which we already have.
  DCHECK_EQ("/", split_path[0]);
  for (int i = 1; i < split_path.size(); ++i) {
    FakeEntry* entry = current->FindEntry(split_path[i]);
    if (!entry) {
      *error = ENOENT;
      return nullptr;
    }
    if (entry->smbc_type != SMBC_DIR) {
      *error = ENOTDIR;
      return nullptr;
    }
    current = static_cast<FakeDirectory*>(entry);
  }
  return current;
}

FakeSambaInterface::FakeFile* FakeSambaInterface::GetFile(
    const std::string& file_path) const {
  FakeEntry* entry = GetEntry(file_path);
  if (!entry || entry->smbc_type != SMBC_FILE) {
    return nullptr;
  }
  return static_cast<FakeFile*>(entry);
}

FakeSambaInterface::FakeEntry* FakeSambaInterface::GetEntry(
    const std::string& entry_path) const {
  FakeDirectory* directory = GetDirectory(GetDirPath(entry_path));
  if (!directory) {
    return nullptr;
  }
  return directory->FindEntry(GetFileName(entry_path));
}

int32_t FakeSambaInterface::AddOpenDirectory(const std::string& path) {
  DCHECK(!IsFDOpen(next_fd));
  open_fds.emplace(next_fd, OpenInfo(path, SMBC_DIR));
  return next_fd++;
}

int32_t FakeSambaInterface::AddOpenFile(const std::string& path,
                                        bool readable,
                                        bool writeable) {
  DCHECK(!IsFDOpen(next_fd));
  open_fds.emplace(next_fd, OpenInfo(path, SMBC_FILE, readable, writeable));
  return next_fd++;
}

bool FakeSambaInterface::IsOpen(const std::string& full_path) const {
  for (auto const& open_it : open_fds) {
    if (open_it.second.full_path == full_path) {
      return true;
    }
  }
  return false;
}

bool FakeSambaInterface::HasOpenEntries() const {
  return !open_fds.empty();
}

bool FakeSambaInterface::IsFileFDOpen(uint32_t fd) const {
  auto open_iter = open_fds.find(fd);
  return open_iter != open_fds.end() &&
         open_iter->second.smbc_type == SMBC_FILE;
}

bool FakeSambaInterface::IsDirectoryFDOpen(uint32_t fd) const {
  auto open_iter = open_fds.find(fd);
  return open_iter != open_fds.end() && open_iter->second.smbc_type == SMBC_DIR;
}

bool FakeSambaInterface::IsFDOpen(uint32_t fd) const {
  return open_fds.count(fd) != 0;
}

FakeSambaInterface::OpenEntriesIterator FakeSambaInterface::FindOpenFD(
    uint32_t fd) {
  return open_fds.find(fd);
}

FakeSambaInterface::OpenEntriesConstIterator FakeSambaInterface::FindOpenFD(
    uint32_t fd) const {
  return open_fds.find(fd);
}

bool FakeSambaInterface::FakeEntry::IsValidEntryType() const {
  return smbc_type == SMBC_DIR || smbc_type == SMBC_FILE;
}

bool FakeSambaInterface::HasReadSet(int32_t fd) const {
  DCHECK(IsFDOpen(fd));
  return open_fds.at(fd).readable;
}

bool FakeSambaInterface::HasWriteSet(int32_t fd) const {
  DCHECK(IsFDOpen(fd));
  return open_fds.at(fd).writeable;
}

bool FakeSambaInterface::EntryExists(const std::string& path) const {
  return GetEntry(path);
}

}  // namespace smbprovider
