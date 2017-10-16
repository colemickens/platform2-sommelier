// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUN_OCI_RUN_OCI_UTILS_H_
#define RUN_OCI_RUN_OCI_UTILS_H_

#include <vector>

#include <base/files/file_path.h>

namespace run_oci {

// Returns all mountpoints under |root|.
std::vector<base::FilePath> GetMountpointsUnder(
    const base::FilePath& root, const base::FilePath& procSelfMountsPath);

// Returns true if the process has the CAP_SYS_ADMIN capability.
bool HasCapSysAdmin();

}  // namespace run_oci

#endif  // RUN_OCI_RUN_OCI_UTILS_H_
