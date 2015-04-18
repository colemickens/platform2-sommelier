// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/environment.h"

#include <base/strings/stringprintf.h>

namespace germ {

Environment::Environment(uid_t uid, gid_t gid)
    : uid_{uid},
      gid_{gid},
      do_pid_namespace_{true},
      do_mount_tmp_{true},
      env_manager_{chromeos::Minijail::GetInstance()} {
}

void Environment::SetEnterNewPidNamespace(bool enabled) {
  do_pid_namespace_ = enabled;
}

void Environment::SetMountTmp(bool enabled) {
  do_mount_tmp_ = enabled;
}

std::string Environment::GetForDaemonized() const {
  return base::StringPrintf("ENVIRONMENT=-u %d -g %d %s %s", uid_, gid_,
                            do_pid_namespace_ ? "-p" : "",
                            do_mount_tmp_ ? "-t" : "");
}

struct minijail* Environment::GetForInteractive() const {
  struct minijail* env_description = env_manager_->New();
  env_manager_->DropRoot(env_description, uid_, gid_);
  if (do_pid_namespace_) {
    env_manager_->EnterNewPidNamespace(env_description);
  }
  if (do_mount_tmp_) {
    env_manager_->MountTmp(env_description);
  }
  return env_description;
}

}  // namespace germ
