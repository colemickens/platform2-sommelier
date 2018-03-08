// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_ITERATOR_DIRECTORY_ITERATOR_H_
#define SMBPROVIDER_ITERATOR_DIRECTORY_ITERATOR_H_

#include <string>
#include <vector>

#include "smbprovider/proto.h"
#include "smbprovider/samba_interface.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

// BaseDirectoryIterator is a class that handles iterating over the DirEnts of
// an SMB directory. It must be subclassed and ShouldIncludeEntryType() have to
// be defined by the derived classes.
//
// Example:
//    DirectoryIterator it("smb://testShare/test/dogs",
//    SambaInterface.get()); result = it.Init(); while (result == 0)  {
//      if it.IsDone: return 0
//      // Do something with it.Get();
//      result = it.Next();
//    }
//    return result;
class BaseDirectoryIterator {
 public:
  BaseDirectoryIterator(const std::string& dir_path,
                        SambaInterface* samba_interface,
                        size_t buffer_size);

  BaseDirectoryIterator(const std::string& dir_path,
                        SambaInterface* samba_interface);

  BaseDirectoryIterator(BaseDirectoryIterator&& other);

  // Initializes the iterator, setting the first value of current. Returns 0 on
  // success, error on failure. Must be called before any other operation.
  int32_t Init() WARN_UNUSED_RESULT;

  // Advances current to the next entry. Returns 0 on success,
  // error on failure.
  int32_t Next() WARN_UNUSED_RESULT;

  // Returns the current DirectoryEntry.
  const DirectoryEntry& Get();

  // Returns true if there is nothing left to iterate over.
  bool IsDone() WARN_UNUSED_RESULT;

  virtual ~BaseDirectoryIterator();

 protected:
  // Returns true on the entry types that should be included.
  virtual bool ShouldIncludeEntryType(uint32_t smbc_type) const = 0;

 private:
  // Fetches the next chunk of DirEntries into entries_ and resets
  // |current_entry_index|. Sets |is_done_| if there are no more entries to
  // fetch. Returns 0 on success.
  int32_t FillBuffer();

  // Converts the buffer into the vector of entries. Also resets
  // |current_entry_index_|.
  void ConvertBufferToVector(int32_t bytes_read);

  // Reads the next batch of entries for |dir_id_| into the buffer. Returns 0
  // on success and errno on failure.
  int32_t ReadEntriesToBuffer(int32_t* bytes_read);

  // Opens the directory at |dir_path_|, setting |dir_id|. Returns 0 on success
  // and errno on failure.
  int32_t OpenDirectory();

  // Attempts to Close the directory with |dir_id_|. Logs on failure.
  void CloseDirectory();

  // Helper method to transform and add |dirent| to a DirectoryEntryListProto.
  void AddEntryIfValid(const smbc_dirent& dirent,
                       std::vector<DirectoryEntry>* directory_entries,
                       const std::string& full_path);

  const std::string dir_path_;
  // |dir_buf_| is used as the buffer for reading directory entries from Samba
  // interface. Its initial capacity is specified in the BaseDirectoryIterator
  // constructor.
  std::vector<uint8_t> dir_buf_;
  std::vector<DirectoryEntry> entries_;
  uint32_t current_entry_index_ = 0;
  // |dir_id_| represents the fd for the open directory at |dir_path_|.
  int32_t dir_id_ = -1;
  // |is_done_| is set to true when no entries left to read.
  bool is_done_ = false;
  // |is_initialized_| is set to true once Init() executes successfully.
  bool is_initialized_ = false;

  SambaInterface* samba_interface_;  // not owned.

  DISALLOW_COPY_AND_ASSIGN(BaseDirectoryIterator);
};

// DirectoryIterator is an implementation of BaseDirectoryIterator that only
// iterates through files and directories.
class DirectoryIterator : public BaseDirectoryIterator {
  using BaseDirectoryIterator::BaseDirectoryIterator;

 public:
  DirectoryIterator(DirectoryIterator&& other) = default;

 protected:
  bool ShouldIncludeEntryType(uint32_t smbc_type) const override {
    return IsFileOrDir(smbc_type);
  }

  DISALLOW_COPY_AND_ASSIGN(DirectoryIterator);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_ITERATOR_DIRECTORY_ITERATOR_H_
