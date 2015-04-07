// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/sysfs_filter.h"

#include <base/files/file_path.h>

namespace soma {

SysfsFilter::SysfsFilter(const base::FilePath& path) : filter_(path) {
}

SysfsFilter::~SysfsFilter() {}

}  // namespace soma
