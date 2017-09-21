// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles setting basic cgroup properties.  The format of the cgroup files can
// be found in the linux kernel at Documentation/cgroups/.

#ifndef LIBCONTAINER_CGROUP_H_
#define LIBCONTAINER_CGROUP_H_

#include <sys/types.h>

#include <memory>
#include <string>

#include <base/callback_forward.h>
#include <base/files/file_path.h>

namespace libcontainer {

class Cgroup {
 public:
  enum Type : int32_t {
    CPU,
    CPUACCT,
    CPUSET,
    DEVICES,
    FREEZER,
    SCHEDTUNE,

    NUM_TYPES,
  };

  static std::unique_ptr<Cgroup> Create(base::StringPiece name,
                                        const base::FilePath& cgroup_root,
                                        const base::FilePath& cgroup_parent,
                                        uid_t cgroup_owner,
                                        gid_t cgroup_group);
  virtual ~Cgroup();

  virtual int Freeze();
  virtual int Thaw();
  virtual int DenyAllDevices();
  virtual int AddDevice(bool allow,
                        int major,
                        int minor,
                        bool read,
                        bool write,
                        bool modify,
                        char type);
  virtual int SetCpuShares(int shares);
  virtual int SetCpuQuota(int quota);
  virtual int SetCpuPeriod(int period);
  virtual int SetCpuRtRuntime(int rt_runtime);
  virtual int SetCpuRtPeriod(int rt_period);

  bool has_tasks_path(int32_t t) const {
    return !cgroup_tasks_paths_[t].empty();
  }

  const base::FilePath& tasks_path(int32_t t) const {
    return cgroup_tasks_paths_[t];
  }

  // TODO(lhchavez): Move to private when we use gtest.
  using CgroupFactory = decltype(&Cgroup::Create);
  static void SetCgroupFactoryForTesting(CgroupFactory factory);

 protected:
  Cgroup();

 private:
  std::string name_;
  base::FilePath cgroup_paths_[Type::NUM_TYPES];
  base::FilePath cgroup_tasks_paths_[Type::NUM_TYPES];

  DISALLOW_COPY_AND_ASSIGN(Cgroup);
};

}  // namespace libcontainer

#endif  // LIBCONTAINER_CGROUP_H_
