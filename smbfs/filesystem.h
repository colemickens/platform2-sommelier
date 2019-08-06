// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_FILESYSTEM_H_
#define SMBFS_FILESYSTEM_H_

#include <fuse_lowlevel.h>

#include <memory>
#include <string>

#include <base/macros.h>

#include "smbfs/request.h"

namespace smbfs {

class Filesystem {
 public:
  Filesystem();
  virtual ~Filesystem();

  virtual void Lookup(std::unique_ptr<EntryRequest> request,
                      fuse_ino_t parent_inode,
                      const std::string& name);
  virtual void GetAttr(std::unique_ptr<AttrRequest> request, fuse_ino_t inode);

 private:
  DISALLOW_COPY_AND_ASSIGN(Filesystem);
};

}  // namespace smbfs

#endif  // SMBFS_FILESYSTEM_H_
