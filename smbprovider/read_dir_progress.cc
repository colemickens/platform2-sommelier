// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/read_dir_progress.h"

#include <base/logging.h>
#include <dbus/smbprovider/dbus-constants.h>

#include "smbprovider/constants.h"
#include "smbprovider/metadata_cache.h"

namespace smbprovider {

ReadDirProgress::ReadDirProgress(SambaInterface* samba_interface)
    : ReadDirProgress(samba_interface, kReadDirectoryInitialBatchSize) {}

ReadDirProgress::ReadDirProgress(SambaInterface* samba_interface,
                                 uint32_t initial_batch_size)
    : samba_interface_(samba_interface), batch_size_(initial_batch_size) {}

ReadDirProgress::~ReadDirProgress() = default;

bool ReadDirProgress::StartReadDir(const std::string& directory_path,
                                   MetadataCache* cache,
                                   int32_t* error,
                                   DirectoryEntryListProto* out_entries) {
  NOTIMPLEMENTED();
  return false;
}

bool ReadDirProgress::ContinueReadDir(int32_t* error,
                                      DirectoryEntryListProto* out_entries) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace smbprovider
