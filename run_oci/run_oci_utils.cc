// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_oci/run_oci_utils.h"

#include <fcntl.h>
#include <mntent.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <type_traits>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/syslog_logging.h>
#include <libmount/libmount.h>

namespace run_oci {

bool Mountpoint::operator==(const Mountpoint& other) const {
  return path == other.path && mountflags == other.mountflags &&
         data_string == other.data_string;
}

std::string ParseMountOptions(const std::vector<std::string>& options,
                              int* mount_flags_out,
                              int* negated_mount_flags_out,
                              int* bind_flags_out,
                              int* mount_propagation_flags_out,
                              bool* loopback_out,
                              std::string* verity_options) {
  std::string option_string_out;
  *mount_flags_out = 0;
  *negated_mount_flags_out = 0;
  *bind_flags_out = 0;
  *mount_propagation_flags_out = 0;
  *loopback_out = false;

  const struct libmnt_optmap* linux_option_map =
      mnt_get_builtin_optmap(MNT_LINUX_MAP);

  constexpr int kMountPropagationFlagsMask =
      MS_PRIVATE | MS_SLAVE | MS_SHARED | MS_UNBINDABLE;

  for (const auto& option : options) {
    const struct libmnt_optmap* map_entry = nullptr;

    for (const struct libmnt_optmap* it = linux_option_map; it->name; ++it) {
      if (option == it->name && it->id) {
        map_entry = it;
        break;
      }
    }

    if (map_entry) {
      // This is a known flag name.
      if (map_entry->id & MS_BIND) {
        *bind_flags_out |= map_entry->id;
      } else if (map_entry->id & kMountPropagationFlagsMask) {
        *mount_propagation_flags_out |= map_entry->id;
      } else if (map_entry->mask & MNT_INVERT) {
        *negated_mount_flags_out |= map_entry->id;
      } else {
        *mount_flags_out |= map_entry->id;
      }
    } else if (option == "loop") {
      *loopback_out = true;
    } else if (base::StartsWith(option, "dm=", base::CompareCase::SENSITIVE)) {
      *verity_options = option.substr(3, std::string::npos);
    } else {
      // Unknown options get appended to the string passed to mount data.
      if (!option_string_out.empty())
        option_string_out += ",";
      option_string_out += option;
    }
  }

  return option_string_out;
}

std::vector<Mountpoint> GetMountpointsUnder(
    const base::FilePath& root, const base::FilePath& procSelfMountsPath) {
  base::ScopedFILE mountinfo(fopen(procSelfMountsPath.value().c_str(), "r"));
  if (!mountinfo) {
    PLOG(ERROR) << "Failed to open " << procSelfMountsPath.value();
    return std::vector<Mountpoint>();
  }

  struct mntent mount_entry;

  std::string line;
  char buffer[1024];
  std::vector<Mountpoint> mountpoints;
  while (getmntent_r(mountinfo.get(), &mount_entry, buffer, sizeof(buffer))) {
    // Only return paths that are under |root|.
    const std::string path = mount_entry.mnt_dir;
    if (path.compare(0, root.value().size(), root.value()) != 0)
      continue;

    int mount_flags, negated_mount_flags, bind_mount_flags,
        mount_propagation_flags;
    bool loopback;
    std::string verity_options;
    std::string options = ParseMountOptions(
        base::SplitString(mount_entry.mnt_opts, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY),
        &mount_flags, &negated_mount_flags, &bind_mount_flags,
        &mount_propagation_flags, &loopback, &verity_options);
    mountpoints.emplace_back(
        Mountpoint{base::FilePath(path), mount_flags, options});
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
