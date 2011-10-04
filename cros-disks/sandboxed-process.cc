// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/sandboxed-process.h"

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/libminijail.h>

using std::string;
using std::vector;

namespace cros_disks {

SandboxedProcess::SandboxedProcess()
    : jail_(minijail_new()) {
  CHECK(jail_) << "Failed to create a process jail";
}

SandboxedProcess::~SandboxedProcess() {
  minijail_destroy(jail_);
}

void SandboxedProcess::AddArgument(const std::string& argument) {
  arguments_.push_back(argument);
}

void SandboxedProcess::LoadSeccompFilterPolicy(const string& policy_file) {
  minijail_parse_seccomp_filters(jail_, policy_file.c_str());
  minijail_use_seccomp_filter(jail_);
}

void SandboxedProcess::SetCapabilities(uint64_t capabilities) {
  minijail_use_caps(jail_, capabilities);
}

void SandboxedProcess::SetGroupId(gid_t group_id) {
  minijail_change_gid(jail_, group_id);
}

void SandboxedProcess::SetUserId(uid_t user_id) {
  minijail_change_uid(jail_, user_id);
}

bool SandboxedProcess::Start() {
  BuildArgumentsArray();
  return minijail_run(jail_, arguments_array_[0], arguments_array_.get()) == 0;
}

int SandboxedProcess::Wait() {
  return minijail_wait(jail_);
}

int SandboxedProcess::Run() {
  if (Start()) {
    return Wait();
  }
  return -1;
}

void SandboxedProcess::BuildArgumentsArray() {
  // The following code to create a writable copy of argument strings
  // is based on chromeos/process.cc
  size_t num_arguments = arguments_.size();
  size_t arguments_buffer_size = 0;
  for (size_t i = 0; i < num_arguments; ++i) {
    arguments_buffer_size += arguments_[i].size() + 1;
  }

  arguments_buffer_.reset(new(std::nothrow) char[arguments_buffer_size]);
  CHECK(arguments_buffer_.get()) << "Failed to allocate arguments buffer";

  arguments_array_.reset(new(std::nothrow) char*[num_arguments + 1]);
  CHECK(arguments_array_.get()) << "Failed to allocate arguments array";

  char* buffer_pointer = arguments_buffer_.get();
  for (size_t i = 0; i < num_arguments; ++i) {
    arguments_array_[i] = buffer_pointer;
    size_t argument_size = arguments_[i].size();
    arguments_[i].copy(buffer_pointer, argument_size);
    buffer_pointer[argument_size] = '\0';
    buffer_pointer += argument_size + 1;
  }
  arguments_array_[num_arguments] = NULL;
}

}  // namespace cros_disks
