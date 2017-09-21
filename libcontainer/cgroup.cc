// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcontainer/cgroup.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/callback_helpers.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

namespace libcontainer {

namespace {

Cgroup::CgroupFactory g_cgroup_factory = nullptr;

constexpr const char* kCgroupNames[Cgroup::Type::NUM_TYPES] = {
    "cpu", "cpuacct", "cpuset", "devices", "freezer", "schedtune"};

base::ScopedFD OpenCgroupFile(const base::FilePath& cgroup_path,
                              base::StringPiece name,
                              bool write) {
  base::FilePath path = cgroup_path.Append(name);
  int flags = write ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY;
  return base::ScopedFD(open(path.value().c_str(), flags, 0664));
}

int WriteCgroupFile(const base::FilePath& cgroup_path,
                    base::StringPiece name,
                    base::StringPiece str) {
  base::ScopedFD fd = OpenCgroupFile(cgroup_path, name, true);
  if (!fd.is_valid())
    return -errno;
  if (write(fd.get(), str.data(), str.size()) !=
      static_cast<ssize_t>(str.size())) {
    return -errno;
  }
  return 0;
}

int WriteCgroupFileInt(const base::FilePath& cgroup_path,
                       base::StringPiece name,
                       const int value) {
  return WriteCgroupFile(cgroup_path, name, base::IntToString(value));
}

int CopyCgroupParent(const base::FilePath& cgroup_path,
                     base::StringPiece name) {
  base::ScopedFD dest = OpenCgroupFile(cgroup_path, name, true);
  if (!dest.is_valid())
    return -errno;

  base::ScopedFD source = OpenCgroupFile(cgroup_path.DirName(), name, false);
  if (!source.is_valid())
    return -errno;

  char buffer[256];
  ssize_t bytes_read;
  while ((bytes_read = read(source.get(), buffer, sizeof(buffer))) > 0) {
    if (!base::WriteFileDescriptor(dest.get(), buffer, bytes_read))
      return -errno;
  }
  if (bytes_read < 0)
    return -errno;

  return 0;
}

std::string GetDeviceString(const int major, const int minor) {
  if (major >= 0 && minor >= 0)
    return base::StringPrintf("%d:%d", major, minor);
  else if (major >= 0)
    return base::StringPrintf("%d:*", major);
  else if (minor >= 0)
    return base::StringPrintf("*:%d", minor);
  else
    return base::StringPrintf("*:*");
}

int CreateCgroupAsOwner(const base::FilePath& cgroup_path,
                        uid_t cgroup_owner,
                        gid_t cgroup_group) {
  // If running as root and the cgroup owner is a user, create the cgroup
  // as that user.
  base::ScopedClosureRunner reset_setegid, reset_seteuid;
  if (getuid() == 0 && (cgroup_owner != 0 || cgroup_group != 0)) {
    if (setegid(cgroup_group))
      return -errno;
    reset_setegid.Reset(base::Bind(base::IgnoreResult(&setegid), 0));

    if (seteuid(cgroup_owner))
      return -errno;
    reset_seteuid.Reset(base::Bind(base::IgnoreResult(&seteuid), 0));
  }

  if (mkdir(cgroup_path.value().c_str(), S_IRWXU | S_IRWXG) < 0 &&
      errno != EEXIST) {
    return -errno;
  }

  return 0;
}

int CheckCgroupAvailable(const base::FilePath& cgroup_root,
                         base::StringPiece cgroup_name) {
  base::FilePath path = cgroup_root.Append(cgroup_name);

  if (access(path.value().c_str(), F_OK))
    return -errno;
  return 0;
}

}  // namespace

int Cgroup::Freeze() {
  return WriteCgroupFile(
      cgroup_paths_[Type::FREEZER], "freezer.state", "FROZEN\n");
}

int Cgroup::Thaw() {
  return WriteCgroupFile(
      cgroup_paths_[Type::FREEZER], "freezer.state", "THAWED\n");
}

int Cgroup::DenyAllDevices() {
  return WriteCgroupFile(cgroup_paths_[Type::DEVICES], "devices.deny", "a\n");
}

int Cgroup::AddDevice(bool allow,
                      int major,
                      int minor,
                      bool read,
                      bool write,
                      bool modify,
                      char type) {
  if (type != 'b' && type != 'c' && type != 'a')
    return -EINVAL;
  if (!read && !write && !modify)
    return -EINVAL;

  std::string device_string = GetDeviceString(major, minor);

  // The device file format is:
  // <type, c, b, or a> major:minor rmw
  std::string perm_string = base::StringPrintf("%c %s %s%s%s\n",
                                               type,
                                               device_string.c_str(),
                                               read ? "r" : "",
                                               write ? "w" : "",
                                               modify ? "m" : "");
  return WriteCgroupFile(cgroup_paths_[Type::DEVICES],
                         allow ? "devices.allow" : "devices.deny",
                         perm_string);
}

int Cgroup::SetCpuShares(int shares) {
  return WriteCgroupFileInt(cgroup_paths_[Type::CPU], "cpu.shares", shares);
}

int Cgroup::SetCpuQuota(int quota) {
  return WriteCgroupFileInt(
      cgroup_paths_[Type::CPU], "cpu.cfs_quota_us", quota);
}

int Cgroup::SetCpuPeriod(int period) {
  return WriteCgroupFileInt(
      cgroup_paths_[Type::CPU], "cpu.cfs_period_us", period);
}

int Cgroup::SetCpuRtRuntime(int rt_runtime) {
  return WriteCgroupFileInt(
      cgroup_paths_[Type::CPU], "cpu.rt_runtime_us", rt_runtime);
}

int Cgroup::SetCpuRtPeriod(int rt_period) {
  return WriteCgroupFileInt(
      cgroup_paths_[Type::CPU], "cpu.rt_period_us", rt_period);
}

// static
void Cgroup::SetCgroupFactoryForTesting(CgroupFactory factory) {
  g_cgroup_factory = factory;
}

// static
std::unique_ptr<Cgroup> Cgroup::Create(base::StringPiece name,
                                       const base::FilePath& cgroup_root,
                                       const base::FilePath& cgroup_parent,
                                       uid_t cgroup_owner,
                                       gid_t cgroup_group) {
  if (g_cgroup_factory) {
    return g_cgroup_factory(
        name, cgroup_root, cgroup_parent, cgroup_owner, cgroup_group);
  }
  std::unique_ptr<Cgroup> cg(new Cgroup());

  for (int32_t i = 0; i < Type::NUM_TYPES; ++i) {
    int rc = CheckCgroupAvailable(cgroup_root, kCgroupNames[i]);
    if (rc < 0) {
      if (rc == -ENOENT)
        continue;
      PLOG(ERROR) << "Cgroup " << kCgroupNames[i] << " not available";
      return nullptr;
    }

    if (!cgroup_parent.empty()) {
      cg->cgroup_paths_[i] = cgroup_root.Append(kCgroupNames[i])
                                 .Append(cgroup_parent)
                                 .Append(name);
    } else {
      cg->cgroup_paths_[i] = cgroup_root.Append(kCgroupNames[i]).Append(name);
    }

    if (CreateCgroupAsOwner(cg->cgroup_paths_[i], cgroup_owner, cgroup_group)) {
      PLOG(ERROR) << "Failed to create cgroup " << cg->cgroup_paths_[i].value()
                  << " as owner";
      return nullptr;
    }

    cg->cgroup_tasks_paths_[i] = cg->cgroup_paths_[i].Append("tasks");

    // cpuset is special: we need to copy parent's cpus or mems,
    // otherwise we'll start with "empty" cpuset and nothing can
    // run in it/be moved into it.
    if (i == Type::CPUSET) {
      if (CopyCgroupParent(cg->cgroup_paths_[i], "cpus")) {
        PLOG(ERROR) << "Failed to copy " << cg->cgroup_paths_[i].value()
                    << "/cpus from parent";
        return nullptr;
      }
      if (CopyCgroupParent(cg->cgroup_paths_[i], "mems")) {
        PLOG(ERROR) << "Failed to copy " << cg->cgroup_paths_[i].value()
                    << "/mems from parent";
        return nullptr;
      }
    }
  }

  cg->name_ = name.as_string();

  return cg;
}

Cgroup::Cgroup() = default;

Cgroup::~Cgroup() {
  for (int32_t i = 0; i < Type::NUM_TYPES; ++i) {
    if (cgroup_paths_[i].empty())
      continue;
    rmdir(cgroup_paths_[i].value().c_str());
  }
}

}  // namespace libcontainer
