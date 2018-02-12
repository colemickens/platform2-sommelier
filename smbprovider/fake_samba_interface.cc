// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/fake_samba_interface.h"

#include <errno.h>

#include <algorithm>

#include <base/macros.h>
#include <base/files/file_path.h>
#include <brillo/any.h>

#include "smbprovider/smbprovider.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {
namespace {

constexpr mode_t kFileMode = 33188;  // File entry
constexpr mode_t kDirMode = 16877;   // Dir entry

// Returns if |flag| is set in |flags|.
bool IsFlagSet(int32_t flags, int32_t flag) {
  return (flags & flag) == flag;
}

// Returns true if |target| is inside of |source|.
bool IsTargetInsideSource(const std::string& target,
                          const std::string& source) {
  base::FilePath target_path(target);
  base::FilePath source_path(source);

  return source_path.IsParent(target_path);
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

  if (entry->locked) {
    return EACCES;
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

  FakeFile* file = GetFile(file_path);
  if (!file) {
    return ENOENT;
  }

  if (file->locked) {
    return EACCES;
  }

  DCHECK(IsValidOpenFileFlags(flags));
  bool readable = IsFlagSet(flags, O_RDONLY) || IsFlagSet(flags, O_RDWR);
  bool writeable = IsFlagSet(flags, O_WRONLY) || IsFlagSet(flags, O_RDWR);
  DCHECK(readable || writeable);

  *file_id = AddOpenFile(file_path, readable, writeable);
  return 0;
}

int32_t FakeSambaInterface::CloseFile(int32_t file_id) {
  if (close_file_error_ != 0) {
    return close_file_error_;
  }

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
  FakeFile* file = GetFile(file_path);
  if (!file) {
    return ENOENT;
  }

  if (file->locked) {
    return EACCES;
  }

  RemoveEntryAndResetIndicies(file_path);
  return 0;
}

int32_t FakeSambaInterface::RemoveDirectory(const std::string& dir_path) {
  int32_t error;
  FakeDirectory* directory = GetDirectory(RemoveURLScheme(dir_path), &error);
  if (!directory) {
    return error;
  }
  if (!directory->entries.empty()) {
    return ENOTEMPTY;
  }

  RemoveEntryAndResetIndicies(dir_path);
  return 0;
}

int32_t FakeSambaInterface::CreateFile(const std::string& file_path,
                                       int32_t* file_id) {
  if (EntryExists(file_path)) {
    return EEXIST;
  }
  AddFile(file_path);
  *file_id = AddOpenFile(file_path, false /* readable */, true /* writeable */);
  return 0;
}

int32_t FakeSambaInterface::Truncate(int32_t file_id, size_t size) {
  if (truncate_error_ != 0) {
    return truncate_error_;
  }

  if (!IsFileFDOpen(file_id)) {
    return EBADFD;
  }
  OpenInfo& open_info = FindOpenFD(file_id)->second;
  FakeFile* file = GetFile(open_info.full_path);
  DCHECK(file);
  file->size = size;
  if (file->has_data) {
    file->data.resize(size, 0);
  }
  // Adjust offset to end of file if the previous offset was larger than size.
  open_info.current_index = std::min(open_info.current_index, size);
  return 0;
}

int32_t FakeSambaInterface::WriteFile(int32_t file_id,
                                      const uint8_t* buffer,
                                      size_t buffer_size) {
  DCHECK(buffer);
  OpenInfo& open_info = FindOpenFD(file_id)->second;
  DCHECK(open_info.smbc_type == SMBC_DIR || open_info.smbc_type == SMBC_FILE);
  if (open_info.smbc_type != SMBC_FILE) {
    return EISDIR;
  }

  if (!open_info.writeable) {
    return EINVAL;
  }

  FakeFile* file = GetFile(open_info.full_path);
  DCHECK(file);

  // Write the data into the file.
  file->WriteData(open_info.current_index, buffer, buffer_size);

  // Adjust to the new offset.
  open_info.current_index += buffer_size;

  return 0;
}

int32_t FakeSambaInterface::CreateDirectory(const std::string& directory_path) {
  if (EntryExists(directory_path)) {
    return EEXIST;
  }

  FakeDirectory* parent = GetDirectory(GetDirPath(directory_path));
  if (!parent) {
    return ENOENT;
  }

  AddDirectory(directory_path);
  return 0;
}

int32_t FakeSambaInterface::MoveEntry(const std::string& source_path,
                                      const std::string& target_path) {
  if (IsTargetInsideSource(target_path, source_path)) {
    // MoveEntry fails if |target_path| is a child of source_path.
    return EINVAL;
  }

  if (!EntryExists(source_path)) {
    // MoveEntry fails if |source_path| does not exist.
    return ENOENT;
  }

  FakeEntry* src_entry = GetEntry(source_path);
  if (EntryExists(target_path)) {
    // If |target_path| exits, check that we can continue with the move.
    FakeEntry* target_entry = GetEntry(target_path);
    int32_t result = CheckEntriesValidForMove(src_entry, target_entry);
    if (result != 0) {
      return result;
    }
  }

  if (src_entry->IsDir() && src_entry->locked) {
    // MoveEntry fails to move a locked directory.
    return EACCES;
  }

  return MoveEntryFromSourceToTarget(source_path, target_path);
}

int32_t FakeSambaInterface::CheckEntriesValidForMove(
    FakeEntry* src_entry, FakeEntry* target_entry) const {
  DCHECK(src_entry);
  DCHECK(target_entry);

  if (target_entry->IsFile()) {
    if (src_entry->IsDir()) {
      return ENOTDIR;
    }
    return EEXIST;
  } else {
    if (src_entry->IsFile()) {
      return EISDIR;
    }
    DCHECK(target_entry->IsDir());
    FakeDirectory* target_dir = static_cast<FakeDirectory*>(target_entry);
    if (!target_dir->IsEmpty()) {
      return EEXIST;
    }
    return 0;
  }
}

int32_t FakeSambaInterface::MoveEntryFromSourceToTarget(
    const std::string& source_path, const std::string& target_path) {
  FakeDirectory* source_dir;
  FakeDirectory* target_dir;
  int32_t result = GetSourceAndTargetParentDirectories(
      source_path, target_path, &source_dir, &target_dir);
  if (result != 0) {
    return result;
  }

  FakeSambaInterface::FakeDirectory::EntriesIterator source_it =
      source_dir->GetEntryIt(GetFileName(source_path));
  (*source_it)->name = GetFileName(target_path);

  if (source_dir != target_dir) {
    // Must perform move in addition to rename.
    target_dir->entries.push_back(std::move(*source_it));
    source_dir->entries.erase(source_it);
  }

  return 0;
}

int32_t FakeSambaInterface::GetSourceAndTargetParentDirectories(
    const std::string& source_path,
    const std::string& target_path,
    FakeDirectory** source_parent,
    FakeDirectory** target_parent) const {
  DCHECK(source_parent);
  DCHECK(target_parent);

  int32_t error;
  *source_parent = GetDirectory(GetDirPath(source_path), &error);
  if (!(*source_parent)) {
    return error;
  }

  *target_parent = GetDirectory(GetDirPath(target_path), &error);
  if (!(*target_parent)) {
    return error;
  }

  // FakeSambaInterface does not support moving open entries/parents.
  DCHECK(!IsOpen(GetDirPath(source_path)));
  DCHECK(!IsOpen(GetDirPath(target_path)));
  DCHECK(!IsOpen(source_path));
  DCHECK(!IsOpen(target_path));

  return 0;
}

bool FakeSambaInterface::FakeDirectory::IsEmpty() const {
  return entries.empty();
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

FakeSambaInterface::FakeDirectory::EntriesIterator
FakeSambaInterface::FakeDirectory::GetEntryIt(const std::string& name) {
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    if ((*it)->name == name) {
      return it;
    }
  }

  // This function is only called after verifying that an entry exists, so it
  // should always return from inside the for-loop above.
  NOTREACHED();
  return entries.end();
}

int32_t FakeSambaInterface::FakeDirectory::RemoveEntry(
    const std::string& name) {
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i]->name == name) {
      DCHECK(IsFileOrEmptyDirectory(entries[i].get()));
      entries.erase(entries.begin() + i);
      return i;
    }
  }
  return -1;
}

void FakeSambaInterface::FakeFile::WriteData(size_t offset,
                                             const uint8_t* buffer,
                                             size_t buffer_size) {
  // Ensure that the current size of the file greater than or equal to the
  // offset.
  DCHECK(this->data.size() >= offset);

  // Resize the data to the new length if necessary.
  const size_t new_length = std::max(offset + buffer_size, this->data.size());
  this->data.resize(new_length, 0);
  this->size = new_length;

  // Copy the data from buffer into the vector starting from the offset.
  memcpy(this->data.data() + offset, buffer, buffer_size);

  this->has_data = true;
}

FakeSambaInterface::FakeEntry::FakeEntry(const std::string& full_path,
                                         uint32_t smbc_type,
                                         size_t size,
                                         uint64_t date,
                                         bool locked)
    : name(GetFileName(full_path)),
      smbc_type(smbc_type),
      size(size),
      date(date),
      locked(locked) {}

void FakeSambaInterface::AddDirectory(const std::string& path) {
  AddDirectory(path, false /* locked */);
}

void FakeSambaInterface::AddDirectory(const std::string& path, bool locked) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(
      std::make_unique<FakeDirectory>(path, locked));
}

void FakeSambaInterface::AddLockedDirectory(const std::string& path) {
  AddDirectory(path, true);
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
  AddFile(path, size, date, false /* locked */);
}

void FakeSambaInterface::AddFile(const std::string& path,
                                 size_t size,
                                 uint64_t date,
                                 bool locked) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(
      std::make_unique<FakeFile>(path, size, date, locked));
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

void FakeSambaInterface::AddLockedFile(const std::string& path) {
  AddFile(path, 0 /* size */, 0 /* date */, true /* locked */);
}

void FakeSambaInterface::AddEntry(const std::string& path, uint32_t smbc_type) {
  // Make sure that no entry exists in that path.
  DCHECK(!EntryExists(path));
  DCHECK(!IsOpen(path));
  FakeDirectory* directory = GetDirectory(GetDirPath(path));
  DCHECK(directory);
  directory->entries.emplace_back(std::make_unique<FakeEntry>(
      path, smbc_type, 0 /* size */, 0 /* date */, false /* locked */));
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
    if (entry->locked) {
      *error = EACCES;
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

size_t FakeSambaInterface::GetFileOffset(int32_t fd) const {
  const OpenInfo& open_info = FindOpenFD(fd)->second;
  DCHECK_EQ(open_info.smbc_type, SMBC_FILE);
  return open_info.current_index;
}

size_t FakeSambaInterface::GetFileSize(const std::string& path) const {
  FakeFile* file = GetFile(path);
  DCHECK(file);
  return file->size;
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
  return IsFile() || IsDir();
}

bool FakeSambaInterface::FakeEntry::IsFile() const {
  return smbc_type == SMBC_FILE;
}

bool FakeSambaInterface::FakeEntry::IsDir() const {
  return smbc_type == SMBC_DIR;
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

void FakeSambaInterface::SetCloseFileError(int32_t error) {
  close_file_error_ = error;
}

void FakeSambaInterface::SetTruncateError(int32_t error) {
  truncate_error_ = error;
}

void FakeSambaInterface::SetCurrentEntry(int32_t dir_id, size_t index) {
  DCHECK(IsFDOpen(dir_id));
  OpenInfo& info = FindOpenFD(dir_id)->second;

  FakeDirectory* directory = GetDirectory(RemoveURLScheme(info.full_path));
  DCHECK(directory);

  DCHECK_LE(index, directory->entries.size());
  info.current_index = index;
}

std::string FakeSambaInterface::GetCurrentEntry(int32_t dir_id) {
  DCHECK(IsFDOpen(dir_id));
  OpenInfo& info = FindOpenFD(dir_id)->second;
  const size_t index = info.current_index;

  FakeDirectory* directory = GetDirectory(RemoveURLScheme(info.full_path));
  DCHECK(directory);

  if (index == directory->entries.size()) {
    return "";
  }

  return directory->entries[index]->name;
}

bool FakeSambaInterface::IsFileDataEqual(
    const std::string& path, const std::vector<uint8_t>& expected) const {
  FakeFile* file = GetFile(path);
  if (!file || !file->has_data) {
    return false;
  }

  if (file->size != expected.size()) {
    return false;
  }

  return expected == file->data;
}

void FakeSambaInterface::RewindOpenInfoIndicesIfNeccessary(
    const std::string& dir_path, size_t deleted_index) {
  for (auto& it : open_fds) {
    OpenInfo& info = it.second;
    if (info.IsForDir(dir_path)) {
      if (deleted_index < info.current_index) {
        // By removing an entry from the directory that has already been read,
        // current_index will have been inadvertantly advanced.
        --info.current_index;
      }
    }
  }
}

bool FakeSambaInterface::OpenInfo::IsForDir(const std::string& dir_path) {
  return RemoveURLScheme(full_path) == dir_path;
}

void FakeSambaInterface::RemoveEntryAndResetIndicies(
    const std::string& full_path) {
  FakeDirectory* parent = GetDirectory(GetDirPath(full_path));
  const int32_t deleted_index = parent->RemoveEntry(GetFileName(full_path));
  DCHECK_GE(deleted_index, 0);
  RewindOpenInfoIndicesIfNeccessary(GetDirPath(full_path), deleted_index);
}

}  // namespace smbprovider
