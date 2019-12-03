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

constexpr char kUnprivilegedUser[] = "nobody";
constexpr char kNetworkUnprivilegedUser[] = "arc-networkd";
constexpr char kChownCapMask = CAP_TO_MASK(CAP_CHOWN);
constexpr uint64_t kIpTablesCapMask =
    CAP_TO_MASK(CAP_NET_ADMIN) | CAP_TO_MASK(CAP_NET_RAW);
constexpr uint64_t kModprobeCapMask = CAP_TO_MASK(CAP_SYS_MODULE);
constexpr uint64_t kRawCapMask = CAP_TO_MASK(CAP_NET_RAW);
constexpr char kChownPath[] = "/bin/chown";
constexpr char kModprobePath[] = "/sbin/modprobe";
constexpr char kNsEnterPath[] = "/usr/bin/nsenter";
constexpr char kTouchPath[] = "/system/bin/touch";
constexpr char kSysctlPath[] = "/usr/sbin/sysctl";
constexpr char kSentinelFile[] = "/dev/.arc_network_ready";

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

void EnterChildProcessJail() {
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  struct minijail* jail = m->New();

  // Most of these return void, but DropRoot() can fail if the user/group
  // does not exist.
  CHECK(m->DropRoot(jail, kNetworkUnprivilegedUser, kNetworkUnprivilegedUser))
      << "Could not drop root privileges";
  m->UseCapabilities(jail, kRawCapMask);
  m->Enter(jail);
  m->Destroy(jail);
}

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

int MinijailedProcessRunner::SysctlWrite(const std::string& key,
                                         const std::string& value) {
  minijail* jail = mj_->New();
  std::vector<std::string> args = {kSysctlPath, "-w", key + "=" + value};
  return RunSyncDestroy(args, mj_, jail, true);
}

int MinijailedProcessRunner::Chown(const std::string& uid,
                                   const std::string& gid,
                                   const std::string& file) {
  minijail* jail = mj_->New();
  CHECK(mj_->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser));
  mj_->UseCapabilities(jail, kChownCapMask);
  std::vector<std::string> args = {kChownPath, uid + ":" + gid, file};
  return RunSyncDestroy(args, mj_, jail, true);
}

}  // namespace arc_networkd
