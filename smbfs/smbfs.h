// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_SMBFS_H_
#define SMBFS_SMBFS_H_

#include <sys/types.h>
#include <string>
#include <type_traits>

namespace smbfs {

struct Options {
  Options() = default;

  int show_help = 0;
  int show_version = 0;
  uid_t uid = 0;
  gid_t gid = 0;
  int use_test = 0;
  int log_level = 1;

  std::string share_path;
  std::string mountpoint;
};
static_assert(std::is_standard_layout<Options>::value,
              "Options struct not compatible with FUSE options parsing");

}  // namespace smbfs

#endif  // SMBFS_SMBFS_H_
