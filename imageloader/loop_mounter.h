// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_MOUNT_OPS_H_
#define IMAGELOADER_MOUNT_OPS_H_

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace imageloader {

class LoopMounter {
 public:
  LoopMounter() {}
  virtual ~LoopMounter() {}

  virtual bool Mount(const base::ScopedFD& image_fd,
                     const base::FilePath& mount_point);

 private:
  DISALLOW_COPY_AND_ASSIGN(LoopMounter);
};

}  // namespace imageloader

#endif  // IMAGELOADER_MOUNT_OPS_H_
