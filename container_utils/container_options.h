// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_CONTAINER_OPTIONS_H_
#define CONTAINER_UTILS_CONTAINER_OPTIONS_H_

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_util.h>

namespace container_utils {

using BindMount = std::pair<base::FilePath, base::FilePath>;
using BindMounts = std::vector<BindMount>;

struct ContainerOptions {
  BindMounts bind_mounts;
  bool use_current_user;

  ContainerOptions() :
    bind_mounts(),
    use_current_user(false) {}
};

}  // namespace container_utils

#endif  // CONTAINER_UTILS_CONTAINER_OPTIONS_H_
