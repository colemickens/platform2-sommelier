// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_SYSFS_FILTER_H_
#define SOMA_LIB_SOMA_SYSFS_FILTER_H_

#include <base/files/file_path.h>

namespace soma {

class SysfsFilter {
 public:
  explicit SysfsFilter(const base::FilePath& path);
  virtual ~SysfsFilter();

 private:
  const base::FilePath filter_;
  DISALLOW_COPY_AND_ASSIGN(SysfsFilter);
};

}  // namespace soma
#endif  // SOMA_LIB_SOMA_SYSFS_FILTER_H_
