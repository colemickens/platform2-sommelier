// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/vm_support_proper.h"

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "crash-reporter/user_collector.h"

void VmSupportProper::AddMetadata(UserCollector* collector) {
  // TODO(hollingum): implement me.
}

void VmSupportProper::FinishCrash(const base::FilePath& crash_meta_path) {
  LOG(INFO) << "A program crashed in the VM and was logged at: "
            << crash_meta_path.value();
  // TODO(hollingum): implement me.
}

bool VmSupportProper::ShouldDump(pid_t pid, std::string* out_reason) {
  // Namespaces are accessed via the /proc/*/ns/* set of paths. The kernel
  // guarantees that if two processes share a namespace, their corresponding
  // namespace files will have the same inode number, as reported by stat.
  //
  // For now, we are only interested in processes in the root PID
  // namespace. When invoked by the kernel in response to a crash,
  // crash_reporter will be run in the root of all the namespace hierarchies, so
  // we can easily check this by comparing the crashed process PID namespace
  // with our own.
  struct stat st;

  auto namespace_path = base::StringPrintf("/proc/%d/ns/pid", pid);
  if (stat(namespace_path.c_str(), &st) < 0) {
    *out_reason = "failed to get process PID namespace";
    return false;
  }
  ino_t inode = st.st_ino;

  if (stat("/proc/self/ns/pid", &st) < 0) {
    *out_reason = "failed to get own PID namespace";
    return false;
  }
  ino_t self_inode = st.st_ino;

  if (inode != self_inode) {
    *out_reason = "ignoring - process not in root namespace";
    return false;
  }

  return true;
}
