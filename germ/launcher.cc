// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/launcher.h"

#include <sys/types.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/minijail/minijail.h>

namespace germ {

// Starts with the top half of user ids.
class UidService final {
 public:
  UidService() { min_uid_ = 1 << ((sizeof(uid_t) * 8) >> 1); }
  ~UidService() {}

  uid_t GetUid() {
    return static_cast<uid_t>(base::RandInt(min_uid_, 2 * min_uid_));
  }

 private:
  uid_t min_uid_;
};

Launcher::Launcher() {
  uid_service_.reset(new UidService());
}

Launcher::~Launcher() {}

bool Launcher::RunInteractiveCommand(const std::string& name,
                                     const std::vector<std::string>& argv,
                                     int* status) {
  std::vector<char*> cmdline;
  for (const auto& t : argv) {
    cmdline.push_back(const_cast<char*>(t.c_str()));
  }
  // Minijail will use the underlying char* array as 'argv',
  // so null-terminate it.
  cmdline.push_back(nullptr);

  uid_t uid = uid_service_->GetUid();
  Environment env(uid, uid);

  return RunWithMinijail(env, cmdline, status);
}

bool Launcher::RunInteractiveSpec(const soma::ReadOnlyContainerSpec& spec,
                                  int* status) {
  std::vector<char*> cmdline;
  // TODO(jorgelo): support running more than one executable.
  for (const auto& t : spec.executables()[0]->command_line) {
    cmdline.push_back(const_cast<char*>(t.c_str()));
  }
  // Minijail will use the underlying char* array as 'argv',
  // so null-terminate it.
  cmdline.push_back(nullptr);

  Environment env(spec.executables()[0]->uid, spec.executables()[0]->gid);
  return RunWithMinijail(env, cmdline, status);
}

void Launcher::ExecveInMinijail(const SomaExecutable& executable) {
    std::vector<char*> argv;
    argv.reserve(executable.command_line_size() + 1);
    for (const std::string& command_line : executable.command_line()) {
      argv.push_back(const_cast<char*>(command_line.c_str()));
    }
    // Null-terminate |argv|.
    argv.push_back(nullptr);

    Environment env(executable.uid(), executable.gid());
    // We'll already be in a PID namespace at this point.
    env.SetEnterNewPidNamespace(false);
    chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
    minijail->Enter(env.GetForInteractive());
    // TODO(rickyz): Environment?
    execve(argv[0], argv.data(), nullptr);
}

bool Launcher::RunWithMinijail(const Environment& env,
                               const std::vector<char*>& cmdline,
                               int* status) {
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  return minijail->RunSyncAndDestroy(env.GetForInteractive(), cmdline,
                                     status);
}

}  // namespace germ
