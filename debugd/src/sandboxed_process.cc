// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/sandboxed_process.h"

#include <base/strings/stringprintf.h>

namespace debugd {

const char *SandboxedProcess::kDefaultUser = "debugd";
const char *SandboxedProcess::kDefaultGroup = "debugd";

SandboxedProcess::SandboxedProcess()
    : sandboxing_(true),
      access_root_mount_ns_(false),
      user_(kDefaultUser),
      group_(kDefaultGroup) {
}

// static
bool SandboxedProcess::GetHelperPath(const std::string& relative_path,
                                     std::string* full_path) {
  // This environment variable controls the root directory for debugd helpers,
  // which lets people develop helpers even when verified boot is on.
  const char* helpers_dir = getenv("DEBUGD_HELPERS");
  std::string path = base::StringPrintf(
      "%s/%s",
      helpers_dir ? helpers_dir : "/usr/libexec/debugd/helpers",
      relative_path.c_str());

  if (path.length() > PATH_MAX)
    return false;

  *full_path = path;
  return true;
}

bool SandboxedProcess::Init() {
  const char *kMiniJail = "/sbin/minijail0";

  AddArg(kMiniJail);
  // Enter a new mount namespace. This is done for every process to avoid
  // affecting the original mount namespace.
  AddArg("-v");

  if (sandboxing_) {
    if (user_.empty() || group_.empty())
      return false;

    if (user_ != "root") {
      AddArg("-u");
      AddArg(user_);
    }
    if (group_ != "root") {
      AddArg("-g");
      AddArg(group_);
    }
  }

  if (access_root_mount_ns_) {
    // Enter root mount namespace.
    AddStringOption("-V", "/proc/1/ns/mnt");
  }

  AddArg("--");

  return true;
}

void SandboxedProcess::DisableSandbox() {
  sandboxing_ = false;
}

void SandboxedProcess::SandboxAs(const std::string& user,
                                 const std::string& group) {
  sandboxing_ = true;
  user_ = user;
  group_ = group;
}

void SandboxedProcess::AllowAccessRootMountNamespace() {
  access_root_mount_ns_ = true;
}

}  // namespace debugd
