// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_HELPER_H_
#define SMBPROVIDER_SMBPROVIDER_HELPER_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <libsmbclient.h>

#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

// Helper method to transform and add |dirent| to a DirectoryEntryListProto.
void AddEntryIfValid(const smbc_dirent& dirent,
                     DirectoryEntryListProto* entries);

// Helper method to advance |dirp| to the next entry.
smbc_dirent* AdvanceDirEnt(smbc_dirent* dirp);

// Helper method that appends a |base_path| to a |relative_path|. Base path may
// or may not contain a trailing separator ('/'). If |relative_path| starts with
// a leading "/", it is stripped before being appended to |base_path|.
std::string AppendPath(const std::string& base_path,
                       const std::string& relative_path);

// Helper method to calculate entry size based on |entry_name|.
size_t CalculateEntrySize(const std::string& entry_name);

// Helper method to get |smbc_dirent| struct from a |buffer|.
smbc_dirent* GetDirentFromBuffer(uint8_t* buffer);

// Helper method to check whether an entry is self (".") or parent ("..").
bool IsSelfOrParentDir(const std::string& entry_name);

// Helper method to check whether or not the entry should be processed on
// GetDirectoryEntries().
bool ShouldProcessEntryType(uint32_t smbc_type);

// Helper method to write the contents of |entry| into the buffer |dirp|.
// |dirlen| is the size of the smbc_dirent entry.
bool WriteEntry(const std::string& entry_name,
                int32_t entry_type,
                size_t buffer_size,
                smbc_dirent* dirp);

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_HELPER_H_
