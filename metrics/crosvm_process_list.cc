// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/crosvm_process_list.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace chromeos_metrics {

namespace {
// Large size for a proc status file.
constexpr size_t kMaxStatusFileSize = 100 * 1024;  // 100KB.

}  // namespace

void ProcessProcStatusFile(
    pid_t pid,
    std::unordered_map<pid_t, std::set<pid_t>>* ppid_pids,
    pid_t* vm_concierge_pid_out,
    base::FilePath slash_proc) {
  base::FilePath file_path =
      slash_proc.Append(base::IntToString(pid)).Append("status");
  std::string status_file_contents;
  if (!ReadFileToStringWithMaxSize(file_path, &status_file_contents,
                                   kMaxStatusFileSize)) {
    PLOG(ERROR) << "Failed reading in status file: " << file_path.value();
    return;
  }

  base::StringPairs key_value_pairs;
  base::SplitStringIntoKeyValuePairs(status_file_contents, ':', '\n',
                                     &key_value_pairs);
  for (auto& key_value_pair : key_value_pairs) {
    if (key_value_pair.first == "Name" &&
        base::TrimWhitespaceASCII(key_value_pair.second, base::TRIM_LEADING) ==
            "vm_concierge") {
      if (*vm_concierge_pid_out != -1) {
        LOG(ERROR) << "More than one vm_concierge process found.";
        *vm_concierge_pid_out = -1;
        return;
      }
      *vm_concierge_pid_out = pid;
    } else if (key_value_pair.first == "PPid") {
      pid_t ppid;
      if (!base::StringToInt(base::TrimWhitespaceASCII(key_value_pair.second,
                                                       base::TRIM_LEADING),
                             &ppid)) {
        LOG(ERROR) << "Failed to parse PPid: " << key_value_pair.second;
        continue;
      }
      auto it = ppid_pids->find(ppid);
      if (it == ppid_pids->end()) {
        std::set<pid_t> pids({pid});
        ppid_pids->emplace(ppid, std::move(pids));
      } else {
        it->second.emplace(pid);
      }
      // In the proc status file, "Name" always comes before "PPid".
      break;
    }
  }
}

void InsertPid(pid_t pid,
               const std::unordered_map<pid_t, std::set<pid_t>>& ppid_pids,
               std::set<pid_t>* crosvm_pids) {
  DCHECK(crosvm_pids);
  crosvm_pids->insert(pid);
  auto it = ppid_pids.find(pid);
  if (it == ppid_pids.end())
    return;
  for (pid_t cpid : it->second) {
    if (crosvm_pids->find(cpid) != crosvm_pids->end())
      continue;
    InsertPid(cpid, ppid_pids, crosvm_pids);
  }
}

// TODO(timzheng): Use cgroup to get the list of processes when it's
// implemented. This currently returns process vm_concierge and all its children
// processes.
std::set<pid_t> GetCrosvmPids(base::FilePath slash_proc) {
  pid_t vm_concierge_pid = -1;
  std::unordered_map<pid_t, std::set<pid_t>> ppid_pids;

  base::FileEnumerator slash_proc_file_enum(slash_proc, false /* recursive */,
                                            base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = slash_proc_file_enum.Next(); !name.empty();
       name = slash_proc_file_enum.Next()) {
    std::string pid_str = name.BaseName().value();
    pid_t pid;
    if (!base::StringToInt(pid_str, &pid)) {
      continue;
    }
    ProcessProcStatusFile(pid, &ppid_pids, &vm_concierge_pid, slash_proc);
  }

  std::set<pid_t> crosvm_pids;
  if (vm_concierge_pid == -1) {
    LOG(ERROR) << "Didn't find vm_concierge process.";
    return crosvm_pids;
  }
  InsertPid(vm_concierge_pid, ppid_pids, &crosvm_pids);
  return crosvm_pids;
}

}  // namespace chromeos_metrics
