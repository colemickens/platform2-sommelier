// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SAMBA_INTERFACE_H_
#define SMBPROVIDER_SAMBA_INTERFACE_H_

#include <base/macros.h>

#include <libsmbclient.h>
#include <string>

namespace smbprovider {

// Interface for interacting with Samba. The actual implementation calls smbc_*
// methods 1:1, while the fake implementation deals with fake directories and
// fake entries. All paths that are passed to the methods in this interface are
// smb:// urls. The methods will return errno on failure.
class SambaInterface {
 public:
  SambaInterface() {}
  virtual ~SambaInterface() {}

  // Opens a file at a given |file_path|.
  // |file_id| is the file id of the opened file. This will be -1 on failure.
  // Flags should be either O_RDONLY or O_RDWR.
  // Returns 0 on success, and errno on failure.
  virtual int32_t OpenFile(const std::string& file_path,
                           int32_t flags,
                           int32_t* file_id) WARN_UNUSED_RESULT = 0;

  // Closes file from a given |file_id|, which is from OpenFile().
  // Returns 0 on success, and errno on failure.
  virtual int32_t CloseFile(int32_t file_id) WARN_UNUSED_RESULT = 0;

  // Opens directory in a given |directory_path|.
  // |dir_id| is the directory id of the opened directory. This will be
  // -1 on failure.
  // Returns 0 on success, and errno on failure.
  virtual int32_t OpenDirectory(const std::string& directory_path,
                                int32_t* dir_id) WARN_UNUSED_RESULT = 0;

  // Closes directory from a given |dir_id|, which is from OpenDirectory().
  // Returns 0 on success, and errno on failure.
  virtual int32_t CloseDirectory(int32_t dir_id) WARN_UNUSED_RESULT = 0;

  // Gets multiple directory entries.
  // Returns 0 on success, and errno on failure.
  // |dir_id| is the directory_id from OpenDirectory().
  // |dirp| is the pointer to a buffer that will receive the directory entries.
  // |dirp_buffer_size| is the size of the |dirp| buffer in bytes.
  // |bytes_read| is the number of bytes read, this will be -1 on failure.
  virtual int32_t GetDirectoryEntries(int32_t dir_id,
                                      smbc_dirent* dirp,
                                      int32_t dirp_buffer_size,
                                      int32_t* bytes_read)
      WARN_UNUSED_RESULT = 0;

  // Gets information about a file or directory.
  // Returns 0 on success, and errno on failure.
  // |full_path| is the smb url to get information for.
  // |stat| is the pointer to a buffer that will be filled with standard Unix
  // struct stat information.
  virtual int32_t GetEntryStatus(const std::string& full_path,
                                 struct stat* stat) WARN_UNUSED_RESULT = 0;

 private:
  static_assert(sizeof(int32_t) == sizeof(int),
                "Ensure that int32_t is same as int, due to casting of int to "
                "int32_t in samba interface");
  DISALLOW_COPY_AND_ASSIGN(SambaInterface);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SAMBA_INTERFACE_H_
