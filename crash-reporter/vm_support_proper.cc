// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/vm_support_proper.h"

#include <base/logging.h>

void VmSupportProper::AddMetadata(UserCollector* collector) {
  // TODO(hollingum): implement me.
}

void VmSupportProper::FinishCrash(const base::FilePath& crash_meta_path) {
  LOG(INFO) << "A program crashed in the VM and was logged at: "
            << crash_meta_path.value();
  // TODO(hollingum): implement me.
}
