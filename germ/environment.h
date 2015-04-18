// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_ENVIRONMENT_H_
#define GERM_ENVIRONMENT_H_

#include <sys/types.h>

#include <string>

#include <base/macros.h>
#include <chromeos/minijail/minijail.h>

namespace germ {

class Environment {
 public:
  Environment(uid_t uid, gid_t gid);
  ~Environment() {}

  void SetEnterNewPidNamespace(bool enabled);
  void SetMountTmp(bool enabled);

  std::string GetForDaemonized() const;
  struct minijail* GetForInteractive() const;

 private:
  uid_t uid_;
  uid_t gid_;
  bool do_pid_namespace_;
  bool do_mount_tmp_;

  chromeos::Minijail* env_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace germ

#endif  // GERM_ENVIRONMENT_H_
