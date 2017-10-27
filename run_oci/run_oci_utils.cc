// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_oci/run_oci_utils.h"

#include <fcntl.h>
#include <mntent.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <type_traits>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_split.h>
#include <brillo/syslog_logging.h>

namespace run_oci {

std::vector<base::FilePath> GetMountpointsUnder(
    const base::FilePath& root, const base::FilePath& procSelfMountsPath) {
  base::ScopedFILE mountinfo(fopen(procSelfMountsPath.value().c_str(), "r"));
  if (!mountinfo) {
    PLOG(ERROR) << "Failed to open " << procSelfMountsPath.value();
    return std::vector<base::FilePath>();
  }

  struct mntent mount_entry;

  std::string line;
  char buffer[1024];
  std::vector<base::FilePath> mountpoints;
  while (getmntent_r(mountinfo.get(), &mount_entry, buffer, sizeof(buffer))) {
    // Only return paths that are under |root|.
    const std::string mountpoint = mount_entry.mnt_dir;
    if (mountpoint.compare(0, root.value().size(), root.value()) != 0)
      continue;
    mountpoints.emplace_back(mountpoint);
  }

  return mountpoints;
}

bool HasCapSysAdmin() {
  if (!CAP_IS_SUPPORTED(CAP_SYS_ADMIN))
    return false;

  std::unique_ptr<std::remove_pointer_t<cap_t>, decltype(&cap_free)> caps(
      cap_get_proc(), &cap_free);
  if (!caps) {
    PLOG(ERROR) << "Failed to get process' capabilities";
    return false;
  }

  cap_flag_value_t cap_value;
  if (cap_get_flag(caps.get(), CAP_SYS_ADMIN, CAP_EFFECTIVE, &cap_value) != 0) {
    PLOG(ERROR) << "Failed to get the value of CAP_SYS_ADMIN";
    return false;
  }
  return cap_value == CAP_SET;
}

bool RedirectLoggingAndStdio(const base::FilePath& log_file) {
  base::ScopedFD log_fd(HANDLE_EINTR(
      open(log_file.value().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644)));
  if (!log_fd.is_valid()) {
    PLOG(ERROR) << "Failed to open log file '" << log_file.value() << "'";
    return false;
  }
  // Redirecting stdout/stderr for the hooks' benefit.
  if (dup2(log_fd.get(), STDOUT_FILENO) == -1) {
    PLOG(ERROR) << "Failed to redirect stdout";
    return false;
  }
  if (dup2(log_fd.get(), STDERR_FILENO) == -1) {
    PLOG(ERROR) << "Failed to redirect stderr";
    return false;
  }
  brillo::SetLogFlags(brillo::kLogHeader | brillo::kLogToStderr);
  logging::SetLogItems(true /* pid */, false /* tid */, true /* timestamp */,
                       false /* tick_count */);
  return true;
}

}  // namespace run_oci
