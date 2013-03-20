// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandboxed_process.h"

namespace debugd {

const char *SandboxedProcess::kDefaultUser = "debugd";
const char *SandboxedProcess::kDefaultGroup = "debugd";

SandboxedProcess::SandboxedProcess()
    : sandboxing_(true), user_(kDefaultUser), group_(kDefaultGroup) { }
SandboxedProcess::~SandboxedProcess() { }

bool SandboxedProcess::Init() {
  const char *kMiniJail = "/sbin/minijail0";
  if (sandboxing_) {
    if (user_.empty() || group_.empty())
      return false;
    AddArg(kMiniJail);
    if (user_ != "root") {
      AddArg("-u");
      AddArg(user_);
    }
    if (group_ != "root") {
      AddArg("-g");
      AddArg(group_);
    }
    AddArg("--");
  }
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

};  // namespace debugd
