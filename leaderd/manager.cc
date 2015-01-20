// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "leaderd/manager.h"

using chromeos::dbus_utils::ExportedObjectManager;

namespace leaderd {

Manager::Manager(ExportedObjectManager* /*object_manager*/) {
}

void Manager::RegisterAsync(const CompletionAction& completion_callback) {
  completion_callback.Run(true);
}

}  // namespace leaderd
