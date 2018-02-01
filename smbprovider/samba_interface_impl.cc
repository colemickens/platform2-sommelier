// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/samba_interface_impl.h"

#include <memory>
#include <string>

#include <base/logging.h>

#include "smbprovider/constants.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

std::unique_ptr<SambaInterfaceImpl> SambaInterfaceImpl::Create() {
  SMBCCTX* context = smbc_new_context();
  if (!context) {
    LOG(ERROR) << "Could not create smbc context";
    return nullptr;
  }
  if (!smbc_init_context(context)) {
    smbc_free_context(context, 0);
    LOG(ERROR) << "Could not initialize smbc context";
    return nullptr;
  }
  smbc_set_context(context);
  return std::unique_ptr<SambaInterfaceImpl>(new SambaInterfaceImpl(context));
}

int32_t SambaInterfaceImpl::CloseFile(int32_t file_id) {
  return smbc_close(file_id) >= 0 ? 0 : errno;
}

int32_t SambaInterfaceImpl::OpenFile(const std::string& file_path,
                                     int32_t flags,
                                     int32_t* file_id) {
  DCHECK(file_id);
  DCHECK(IsValidOpenFileFlags(flags));

  *file_id = smbc_open(file_path.c_str(), flags, 0 /* mode */);
  if (*file_id < 0) {
    *file_id = -1;
    return errno;
  }
  return 0;
}

int32_t SambaInterfaceImpl::OpenDirectory(const std::string& directory_path,
                                          int32_t* dir_id) {
  DCHECK(dir_id);
  *dir_id = smbc_opendir(directory_path.c_str());
  if (*dir_id < 0) {
    *dir_id = -1;
    return errno;
  }
  return 0;
}

int32_t SambaInterfaceImpl::CloseDirectory(int32_t dir_id) {
  return smbc_closedir(dir_id) >= 0 ? 0 : errno;
}

int32_t SambaInterfaceImpl::GetDirectoryEntries(int32_t dir_id,
                                                smbc_dirent* dirp,
                                                int32_t dirp_buffer_size,
                                                int32_t* bytes_read) {
  DCHECK(dirp);
  DCHECK(bytes_read);
  *bytes_read = smbc_getdents(dir_id, dirp, dirp_buffer_size);
  if (*bytes_read < 0) {
    *bytes_read = -1;
    return errno;
  }
  return 0;
}

int32_t SambaInterfaceImpl::GetEntryStatus(const std::string& full_path,
                                           struct stat* stat) {
  DCHECK(stat);
  return smbc_stat(full_path.c_str(), stat) >= 0 ? 0 : errno;
}

int32_t SambaInterfaceImpl::ReadFile(int32_t file_id,
                                     uint8_t* buffer,
                                     size_t buffer_size,
                                     size_t* bytes_read) {
  DCHECK(buffer);
  DCHECK(bytes_read);
  *bytes_read = smbc_read(file_id, buffer, buffer_size);
  return *bytes_read < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::Seek(int32_t file_id, int64_t offset) {
  return smbc_lseek(file_id, offset, SEEK_SET) < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::Unlink(const std::string& file_path) {
  return smbc_unlink(file_path.c_str()) < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::RemoveDirectory(const std::string& dir_path) {
  return smbc_rmdir(dir_path.c_str()) < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::CreateFile(const std::string& file_path,
                                       int32_t* file_id) {
  *file_id =
      smbc_open(file_path.c_str(), kCreateFileFlags, kCreateEntryPermissions);
  if (*file_id < 0) {
    *file_id = -1;
    return errno;
  }
  return 0;
}

int32_t SambaInterfaceImpl::Truncate(int32_t file_id, size_t size) {
  return smbc_ftruncate(file_id, size) < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::WriteFile(int32_t file_id,
                                      const uint8_t* buffer,
                                      size_t buffer_size) {
  return smbc_write(file_id, buffer, buffer_size) < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::CreateDirectory(const std::string& directory_path) {
  int32_t result = smbc_mkdir(directory_path.c_str(), kCreateEntryPermissions);
  return result < 0 ? errno : 0;
}

SambaInterfaceImpl::~SambaInterfaceImpl() {
  smbc_free_context(context_, 0);
}

SambaInterfaceImpl::SambaInterfaceImpl(SMBCCTX* context) : context_(context) {}

}  // namespace smbprovider
