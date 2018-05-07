// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SAMBA_INTERFACE_IMPL_H_
#define SMBPROVIDER_SAMBA_INTERFACE_IMPL_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <dbus/smbprovider/dbus-constants.h>

#include "smbprovider/samba_interface.h"

namespace smbprovider {

// Implements SambaInterface and calls libsmbclient's smbc_* methods 1:1.
class SambaInterfaceImpl : public SambaInterface {
 public:
  // SMB authentication callback.
  using AuthCallback = base::Callback<void(const std::string& share_path,
                                           char* workgroup,
                                           int32_t workgroup_length,
                                           char* username,
                                           int32_t username_length,
                                           char* password,
                                           int32_t password_length)>;

  ~SambaInterfaceImpl() override;

  // This should be called instead of constructor.
  template <typename T = SambaInterfaceImpl::AuthCallback>
  static std::unique_ptr<SambaInterfaceImpl> Create(T auth_callback);

  int32_t OpenFile(const std::string& file_path,
                   int32_t flags,
                   int32_t* file_id) override;

  int32_t CloseFile(int32_t file_id) override;

  int32_t OpenDirectory(const std::string& directory_path,
                        int32_t* dir_id) override;

  int32_t CloseDirectory(int32_t dir_id) override;

  int32_t GetDirectoryEntries(int32_t dir_id,
                              smbc_dirent* dirp,
                              int32_t dirp_buffer_size,
                              int32_t* bytes_read) override;

  int32_t GetEntryStatus(const std::string& full_path,
                         struct stat* stat) override;

  int32_t ReadFile(int32_t file_id,
                   uint8_t* buffer,
                   size_t buffer_size,
                   size_t* bytes_read) override;

  int32_t Seek(int32_t file_id, int64_t offset) override;

  int32_t Unlink(const std::string& file_path) override;

  int32_t RemoveDirectory(const std::string& dir_path) override;

  int32_t CreateFile(const std::string& file_path, int32_t* file_id) override;

  int32_t Truncate(int32_t file_id, size_t size) override;

  int32_t WriteFile(int32_t file_id,
                    const uint8_t* buffer,
                    size_t buffer_size) override;

  int32_t CreateDirectory(const std::string& directory_path) override;

  int32_t MoveEntry(const std::string& source_path,
                    const std::string& target_path) override;

  int32_t CopyFile(const std::string& source_path,
                   const std::string& target_path) override;

 private:
  explicit SambaInterfaceImpl(SMBCCTX* context);
  SMBCCTX* context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SambaInterfaceImpl);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SAMBA_INTERFACE_IMPL_H_
