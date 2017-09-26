// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/device_ejector.h"

#include <memory>

#include <base/bind.h>
#include <base/logging.h>

#include "cros-disks/glib_process.h"

using std::string;

namespace {

// Expected location of the 'eject' program.
const char kEjectProgram[] = "/usr/bin/eject";

}  // namespace

namespace cros_disks {

DeviceEjector::DeviceEjector() {}

DeviceEjector::~DeviceEjector() {}

bool DeviceEjector::Eject(const string& device_path) {
  CHECK(!device_path.empty()) << "Invalid device path";

  eject_processes_.push_back(std::make_unique<GlibProcess>());
  GlibProcess* process = eject_processes_.back().get();

  process->AddArgument(kEjectProgram);
  process->AddArgument(device_path);
  process->set_callback(base::Bind(&DeviceEjector::OnEjectProcessTerminated,
                                   base::Unretained(this)));
  // TODO(benchan): Set up a timeout to kill a hanging process.
  return process->Start();
}

void DeviceEjector::OnEjectProcessTerminated(GlibProcess* process) {
  CHECK(process);
  for (auto it = eject_processes_.begin(); it != eject_processes_.end(); ++it) {
    if (it->get() == process) {
      eject_processes_.erase(it);
      break;
    }
  }
}

}  // namespace cros_disks
