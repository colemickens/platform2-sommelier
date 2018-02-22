// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUN_OCI_RUN_OCI_UTILS_H_
#define RUN_OCI_RUN_OCI_UTILS_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

namespace run_oci {

struct Mountpoint {
  base::FilePath path;
  int mountflags;
  std::string data_string;

  bool operator==(const Mountpoint&) const;
};

// Parses the mount(8) options into mount flags and data string that can be
// understood by mount(2).
std::string ParseMountOptions(const std::vector<std::string>& options,
                              int* mount_flags_out,
                              int* negated_mount_flags_out,
                              int* bind_mount_flags_out,
                              int* mount_propagation_flags_out,
                              bool* loopback_out,
                              std::string* verity_options);

// Returns all mountpoints under |root|.
std::vector<Mountpoint> GetMountpointsUnder(
    const base::FilePath& root, const base::FilePath& procSelfMountsPath);

// Returns true if the process has the CAP_SYS_ADMIN capability.
bool HasCapSysAdmin();

// Redirects all logging and stdout/stdio to |log_file|.
bool RedirectLoggingAndStdio(const base::FilePath& log_file);

}  // namespace run_oci

#endif  // RUN_OCI_RUN_OCI_UTILS_H_
