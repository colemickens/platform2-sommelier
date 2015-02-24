// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/device_filter.h"

#include <base/files/file_path.h>

namespace soma {

DevicePathFilter::DevicePathFilter(const base::FilePath& path) : filter_(path) {
}

DevicePathFilter::~DevicePathFilter() {}

DeviceNodeFilter::DeviceNodeFilter(int major, int minor)
    : major_(major), minor_(minor) {
}

DeviceNodeFilter::~DeviceNodeFilter() {}

}  // namespace soma
