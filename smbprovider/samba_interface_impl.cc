// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/samba_interface_impl.h"

#include <memory>
#include <string>
#include <utility>

#include <base/logging.h>
#include <base/memory/ptr_util.h>

#include "smbprovider/constants.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

namespace {

// Returns the mount root by appending 'smb://' to |server| and |share|.
std::string GetMountRoot(const char* server, const char* share) {
  DCHECK(server);
  DCHECK(share);

  return std::string(kSmbUrlScheme) + server + "/" + share;
}

// Default handler for server side copy progress. Since nothing can make use
// of this callback yet, it remains an implementation detail.
int CopyProgressHandler(off_t upto, void* callback_context) {
  // Return non-zero to indicate that copy should continue.
  return 1;
}

}  // namespace

template <typename T>
std::unique_ptr<SambaInterfaceImpl> SambaInterfaceImpl::Create(
    T auth_callback) {
  SMBCCTX* context = smbc_new_context();
  if (!context) {
    LOG(ERROR) << "Could not create smbc context";
    return nullptr;
  }

  smbc_setOptionUseKerberos(context, 1);
  smbc_setOptionFallbackAfterKerberos(context, 1);

  if (!smbc_init_context(context)) {
    smbc_free_context(context, 0);
    LOG(ERROR) << "Could not initialize smbc context";
    return nullptr;
  }

  smbc_set_context(context);

  // Change auth_callback to a static pointer.
  static uint8_t static_auth_callback_memory[sizeof(T)];
  static T* static_auth_callback =
      new (static_auth_callback_memory) T(std::move(auth_callback));

  // Use static_auth_callback as a C function pointer by converting it to a
  // lambda.
  smbc_setFunctionAuthData(
      context, +[](const char* srv, const char* shr, char* wg, int32_t wglen,
                   char* un, int32_t unlen, char* pw, int32_t pwlen) {
        static_auth_callback->Run(GetMountRoot(srv, shr), wg, wglen, un, unlen,
                                  pw, pwlen);
      });

  return base::WrapUnique(new SambaInterfaceImpl(context));
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

int32_t SambaInterfaceImpl::MoveEntry(const std::string& source_path,
                                      const std::string& target_path) {
  int32_t result = smbc_rename(source_path.c_str(), target_path.c_str());
  return result < 0 ? errno : 0;
}

int32_t SambaInterfaceImpl::CopyFile(const std::string& source_path,
                                     const std::string& target_path) {
  return CopyFile(source_path, target_path, CopyProgressHandler, nullptr);
}

int32_t SambaInterfaceImpl::CopyFile(const std::string& source_path,
                                     const std::string& target_path,
                                     CopyProgressCallback progress_callback,
                                     void* callback_context) {
  SMBCFILE* source = nullptr;
  SMBCFILE* target = nullptr;
  int32_t result = OpenCopySource(source_path, &source);
  if (result != 0) {
    return result;
  }

  result = OpenCopyTarget(target_path, &target);
  if (result != 0) {
    CloseCopySourceAndTarget(source, target);
    return result;
  }

  struct stat source_stat;
  result = GetEntryStatus(source_path, &source_stat);
  if (result != 0) {
    CloseCopySourceAndTarget(source, target);
    return result;
  }

  if (smbc_splice_ctx_(context_, source, target, source_stat.st_size,
                       progress_callback, callback_context) == -1) {
    result = errno;
    CloseCopySourceAndTarget(source, target);
    return result;
  }

  CloseCopySourceAndTarget(source, target);
  return 0;
}

int32_t SambaInterfaceImpl::OpenCopySource(const std::string& file_path,
                                           SMBCFILE** source) {
  DCHECK(source);
  *source = smbc_open_ctx_(context_, file_path.c_str(), O_RDONLY, 0 /* mode */);
  if (!(*source)) {
    return errno;
  }

  return 0;
}

int32_t SambaInterfaceImpl::OpenCopyTarget(const std::string& file_path,
                                           SMBCFILE** target) {
  DCHECK(target);
  *target = smbc_open_ctx_(context_, file_path.c_str(), kCreateFileFlags,
                           0 /* mode */);
  if (!(*target)) {
    return errno;
  }

  return 0;
}

void SambaInterfaceImpl::CloseCopySourceAndTarget(SMBCFILE* source,
                                                  SMBCFILE* target) {
  if (source) {
    smbc_close_ctx_(context_, source);
  }

  if (target) {
    smbc_close_ctx_(context_, target);
  }
}

SambaInterfaceImpl::~SambaInterfaceImpl() {
  smbc_free_context(context_, 0);
}

SambaInterfaceImpl::SambaInterfaceImpl(SMBCCTX* context) : context_(context) {
  DCHECK(context);

  // Load the context functions required for smbc_splice.
  smbc_splice_ctx_ = smbc_getFunctionSplice(context);
  smbc_open_ctx_ = smbc_getFunctionOpen(context);
  smbc_close_ctx_ = smbc_getFunctionClose(context);
}

// This is required to explicitly instantiate the template function.
template std::unique_ptr<SambaInterfaceImpl> SambaInterfaceImpl::Create<
    SambaInterfaceImpl::AuthCallback>(SambaInterfaceImpl::AuthCallback);

}  // namespace smbprovider
