// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/environment.h"

#include <base/strings/stringprintf.h>

namespace germ {

std::string Environment::GetServiceEnvironment(uid_t uid) {
  return base::StringPrintf("ENVIRONMENT=-u %d -g %d -p -t", uid, uid);
}

struct minijail* Environment::GetInteractiveEnvironment(uid_t uid) {
  chromeos::Minijail* minijail = chromeos::Minijail::GetInstance();
  struct minijail* jail = minijail->New();

  minijail->DropRoot(jail, uid, uid);
  minijail->EnterNewPidNamespace(jail);
  minijail->MountTmp(jail);
  return jail;
}

}  // namespace germ
