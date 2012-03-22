// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/device-ejector.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util-inl.h>

#include "cros-disks/glib-process.h"

using std::string;

namespace {

// Expected location of the 'eject' program.
const char kEjectProgram[] = "/usr/bin/eject";

}  // namespace

namespace cros_disks {

DeviceEjector::DeviceEjector() {
}

DeviceEjector::~DeviceEjector() {
  STLDeleteElements(&eject_processes_);
}

bool DeviceEjector::Eject(const string& device_path) {
  CHECK(!device_path.empty()) << "Invalid device path";

  GlibProcess* process = new(std::nothrow) GlibProcess();
  CHECK(process) << "Failed to create process object";
  eject_processes_.push_back(process);

  process->AddArgument(kEjectProgram);
  process->AddArgument(device_path);
  process->set_callback(base::Bind(&DeviceEjector::OnEjectProcessTerminated,
                                   base::Unretained(this)));
  // TODO(benchan): Set up a timeout to kill a hanging process.
  return process->Start();
}

void DeviceEjector::OnEjectProcessTerminated(GlibProcess* process) {
  CHECK(process);
  eject_processes_.remove(process);
  delete process;
}

}  // namespace cros_disks
