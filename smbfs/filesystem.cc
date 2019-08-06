// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/filesystem.h"

#include <errno.h>

namespace smbfs {

Filesystem::Filesystem() = default;

Filesystem::~Filesystem() = default;

void Filesystem::Lookup(std::unique_ptr<EntryRequest> request,
                        fuse_ino_t parent_inode,
                        const std::string& name) {
  request->ReplyError(ENOSYS);
}

void Filesystem::GetAttr(std::unique_ptr<AttrRequest> request,
                         fuse_ino_t inode) {
  request->ReplyError(ENOSYS);
}

}  // namespace smbfs
