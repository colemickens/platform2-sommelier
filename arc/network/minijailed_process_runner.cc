// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/minijailed_process_runner.h"

#include <linux/capability.h>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <brillo/process.h>

namespace arc_networkd {

namespace {

const char kUnprivilegedUser[] = "nobody";
const uint64_t kIpTablesCapMask =
    CAP_TO_MASK(CAP_NET_ADMIN) | CAP_TO_MASK(CAP_NET_RAW);
const uint64_t kModprobeCapMask = CAP_TO_MASK(CAP_SYS_MODULE);
const char kModprobePath[] = "/sbin/modprobe";
const char kNsEnterPath[] = "/usr/bin/nsenter";
const char kTouchPath[] = "/system/bin/touch";
const char kSentinelFile[] = "/dev/.arc_network_ready";

int RunSyncDestroy(const std::vector<std::string>& argv,
                   brillo::Minijail* mj,
                   minijail* jail,
                   bool log_failures) {
  std::vector<char*> args;
  for (const auto& arg : argv) {
    args.push_back(const_cast<char*>(arg.c_str()));
  }
  args.push_back(nullptr);

  int status;
  bool ran = mj->RunSyncAndDestroy(jail, args, &status);
  if (!ran) {
    LOG(ERROR) << "Could not execute '" << base::JoinString(argv, " ") << "'";
  } else if (log_failures && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
    if (WIFEXITED(status)) {
      LOG(WARNING) << "Subprocess '" << base::JoinString(argv, " ")
                   << "' exited with code " << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      LOG(WARNING) << "Subprocess '" << base::JoinString(argv, " ")
                   << "' exited with signal " << WTERMSIG(status);
    } else {
      LOG(WARNING) << "Subprocess '" << base::JoinString(argv, " ")
                   << "' exited with unknown status " << status;
    }
  }
  return ran && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int RunSync(const std::vector<std::string>& argv,
            brillo::Minijail* mj,
            bool log_failures) {
  return RunSyncDestroy(argv, mj, mj->New(), log_failures);
}

}  // namespace

MinijailedProcessRunner::MinijailedProcessRunner(brillo::Minijail* mj) {
  mj_ = mj ? mj : brillo::Minijail::GetInstance();
}

int MinijailedProcessRunner::Run(const std::vector<std::string>& argv,
                                 bool log_failures) {
  minijail* jail = mj_->New();
  CHECK(mj_->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser));
  mj_->UseCapabilities(jail, kIpTablesCapMask);
  return RunSyncDestroy(argv, mj_, jail, log_failures);
}

int MinijailedProcessRunner::AddInterfaceToContainer(
    const std::string& host_ifname,
    const std::string& con_ifname,
    const std::string& con_ipv4,
    const std::string& con_nmask,
    bool enable_multicast,
    const std::string& con_pid) {
  int rc = RunSync({kNsEnterPath, "-t", con_pid, "-n", "--", kIpPath, "link",
                    "set", host_ifname, "name", con_ifname},
                   mj_, true);
  if (rc != 0)
    return rc;

  std::vector<std::string> args = {
      kNsEnterPath,  "-t",       con_pid,  "-n",      "--",
      kIfConfigPath, con_ifname, con_ipv4, "netmask", con_nmask};
  if (!enable_multicast)
    args.emplace_back("-multicast");

  return RunSync(args, mj_, true);
}

int MinijailedProcessRunner::WriteSentinelToContainer(
    const std::string& con_pid) {
  return RunSync({kNsEnterPath, "-t", con_pid, "--mount", "--pid", "--",
                  kTouchPath, kSentinelFile},
                 mj_, true);
}

int MinijailedProcessRunner::ModprobeAll(
    const std::vector<std::string>& modules) {
  minijail* jail = mj_->New();
  CHECK(mj_->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser));
  mj_->UseCapabilities(jail, kModprobeCapMask);
  std::vector<std::string> args = {kModprobePath, "-a"};
  args.insert(args.end(), modules.begin(), modules.end());
  return RunSyncDestroy(args, mj_, jail, true);
}

}  // namespace arc_networkd
