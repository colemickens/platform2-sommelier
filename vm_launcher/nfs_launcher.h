// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_LAUNCHER_NFS_LAUNCHER_H_
#define VM_LAUNCHER_NFS_LAUNCHER_H_

#include <string>
#include <brillo/process.h>

namespace vm_launcher {

// Name of nfs-ganesha's upstart job.
constexpr char kGaneshaJobName[] = "nfs-ganesha";

// Path to nfs-ganesha seccomp policy file.
constexpr char kGaneshaPolicyFile[] =
  "/usr/share/policy/nfs-ganesha-seccomp.policy";

// Path to ganesha's temporary config and log directory.
constexpr char kGaneshaConfigDirectory[] = "/run/ganesha";

// Path to ganesha's pivot root Directory.
constexpr char kGaneshaChrootDirectory[] = "/var/empty";

// Launches the NFS server before running a VM.
// For multiple VMs, the config file needs to be updated accordingly,
// and overridden. Then the NFS server should be restarted.
class NfsLauncher {
 public:
  ~NfsLauncher();

  // Launch the nfs server.
  bool Launch();

  // Terminates the running nfs server.
  bool Terminate();

 private:
  // Writes the configuration file for the nfs server.
  bool Configure();

  bool running_ = false;
};
}  // namespace vm_launcher

#endif  // VM_LAUNCHER_NFS_LAUNCHER_H_
