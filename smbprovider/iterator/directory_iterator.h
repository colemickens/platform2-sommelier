// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_ITERATOR_DIRECTORY_ITERATOR_H_
#define SMBPROVIDER_ITERATOR_DIRECTORY_ITERATOR_H_

#include "smbprovider/iterator/file_system_iterator_interface.h"

#include <string>
#include <vector>

#include "smbprovider/proto.h"
#include "smbprovider/samba_interface.h"

namespace smbprovider {

// DirectoryIterator is a class that handles iterating over the DirEnts of
// an SMB directory.
//
// Example:
//    DirectoryIterator it("smb://testShare/test/dogs", SambaInterface.get());
//    result = it.Init();
//    while (result == 0)  {
//      if it.IsDone: return 0
//      // Do something with it.Get();
//      result = it.Next();
//    }
//    return result;
class DirectoryIterator : public FileSystemIteratorInterface {
 public:
  // FileSystemIteratorInterface overrides.
  int32_t Init() override WARN_UNUSED_RESULT;
  int32_t Next() override WARN_UNUSED_RESULT;
  const DirectoryEntry& Get() override;
  bool IsDone() override WARN_UNUSED_RESULT;

  DirectoryIterator(const std::string& dir_path,
                    SambaInterface* samba_interface,
                    size_t buffer_size);
  DirectoryIterator(const std::string& dir_path,
                    SambaInterface* samba_interface);

  DirectoryIterator(DirectoryIterator&& other);

  ~DirectoryIterator() override;

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

  const std::string dir_path_;
  // |dir_buf_| is used as the buffer for reading directory entries from Samba
  // interface. Its initial capacity is specified in the DirectoryIterator
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

  DISALLOW_COPY_AND_ASSIGN(DirectoryIterator);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_ITERATOR_DIRECTORY_ITERATOR_H_
