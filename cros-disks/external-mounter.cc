// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/external-mounter.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/process.h>

using std::string;

namespace {

// Expected locations of an external mount program
const char* kMountProgramPaths[] = {
  "/bin/mount", "/sbin/mount", "/usr/bin/mount", "/usr/sbin/mount"
};

}  // namespace

namespace cros_disks {

const char ExternalMounter::kMounterType[] = "external";

ExternalMounter::ExternalMounter(const string& source_path,
                                 const string& target_path,
                                 const string& filesystem_type,
                                 const MountOptions& mount_options)
    : Mounter(source_path, target_path, filesystem_type, mount_options) {
}

MountErrorType ExternalMounter::MountImpl() {
  string mount_program = GetMountProgramPath();
  if (mount_program.empty()) {
    LOG(WARNING) << "Could not find an external mount program.";
    return MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND;
  }

  chromeos::ProcessImpl mount_process;
  mount_process.AddArg(mount_program);
  mount_process.AddArg("-t");
  mount_process.AddArg(filesystem_type());
  string options_string = mount_options().ToString();
  if (!options_string.empty()) {
    mount_process.AddArg("-o");
    mount_process.AddArg(options_string);
  }
  mount_process.AddArg(source_path());
  mount_process.AddArg(target_path());

  int return_code = mount_process.Run();
  if (return_code != 0) {
    LOG(WARNING) << "External mount program failed with a return code "
                 << return_code;
    return MOUNT_ERROR_MOUNT_PROGRAM_FAILED;
  }
  return MOUNT_ERROR_NONE;
}

string ExternalMounter::GetMountProgramPath() const {
  for (size_t i = 0; i < arraysize(kMountProgramPaths); ++i) {
    string path = kMountProgramPaths[i];
    if (file_util::PathExists(FilePath(path)))
      return path;
  }
  return string();
}

}  // namespace cros_disks
