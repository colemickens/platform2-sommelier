// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_CROSVM_PROCESS_LIST_H_
#define METRICS_CROSVM_PROCESS_LIST_H_

#include <set>
#include <unordered_map>

#include <base/files/file_path.h>

namespace chromeos_metrics {

// Returns all processes of crosvm.
// |slash_proc| is "/proc" for production and is only changed for tests.
std::set<pid_t> GetCrosvmPids(
    base::FilePath slash_proc = base::FilePath("/proc"));

// Reads a proc status file whose pid is |pid|;
// inserts into |ppid_pids| the mapping from its parent pid to its pid;
// assigns pid to |vm_concierge_pid| if this process is vm_concierge.
// |slash_proc| is "/proc" for production and is only changed for tests.
// This is exposed for tests only.
void ProcessProcStatusFile(
    pid_t pid,
    std::unordered_map<pid_t, std::set<pid_t>>* ppid_pids,
    pid_t* vm_concierge_pid_out,
    base::FilePath slash_proc = base::FilePath("/proc"));

// Recursively insert |pid| and its children according to |ppid_pids| into
// |crosvm_pids|.
// This is exposed for tests only.
void InsertPid(pid_t pid,
               const std::unordered_map<pid_t, std::set<pid_t>>& ppid_pids,
               std::set<pid_t>* crosvm_pids);

}  // namespace chromeos_metrics

#endif  // METRICS_CROSVM_PROCESS_LIST_H_
