// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/process.h"

namespace cros_disks {

// static
const pid_t Process::kInvalidProcessId = -1;

Process::Process() : pid_(kInvalidProcessId) {}

Process::~Process() {}

void Process::AddArgument(const std::string& argument) {
  arguments_.push_back(argument);
}

char** Process::GetArguments() {
  if (arguments_array_.empty())
    BuildArgumentsArray();

  return arguments_array_.data();
}

bool Process::BuildArgumentsArray() {
  // The following code creates a writable copy of argument strings.
  size_t num_arguments = arguments_.size();
  if (num_arguments == 0)
    return false;

  size_t arguments_buffer_size = 0;
  for (const auto& argument : arguments_) {
    arguments_buffer_size += argument.size() + 1;
  }

  arguments_buffer_.resize(arguments_buffer_size);
  arguments_array_.resize(num_arguments + 1);

  char** array_pointer = arguments_array_.data();
  char* buffer_pointer = arguments_buffer_.data();
  for (const auto& argument : arguments_) {
    *array_pointer = buffer_pointer;
    size_t argument_size = argument.size();
    argument.copy(buffer_pointer, argument_size);
    buffer_pointer[argument_size] = '\0';
    buffer_pointer += argument_size + 1;
    ++array_pointer;
  }
  *array_pointer = nullptr;
  return true;
}

int Process::Run() {
  if (Start()) {
    return Wait();
  }
  return -1;
}

}  // namespace cros_disks
