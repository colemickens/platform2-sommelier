// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox.h"

#include <chromeos/libminijail.h>
#include <glog/logging.h>
#include <stdlib.h>

void Sandbox::Enter() {
  if (getuid() != 0 && geteuid() != 0)
    // Already sandboxed. Do nothing.
    // TODO(ellyjones): Remove this hack once
    // http://gerrit.chromium.org/gerrit/8650 is in.
    return;

  struct minijail *jail = minijail_new();
  if (!jail) {
    LOG(ERROR) << "Can't allocate minijail.";
    abort();
  }

  minijail_change_user(jail, "cromo");
  minijail_change_group(jail, "cromo");

  minijail_enter(jail);
}
