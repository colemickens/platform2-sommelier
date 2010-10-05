// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the PlatformReader base class.  By default, it will read an integer
// from a file and assigned it to a PlatformValue which is
// implementation-specific.

#include "platform_reader.h"

#include <sys/types.h>

namespace cros_boot_mode {

PlatformReader::PlatformReader() :
  value_(kUnsupported), platform_file_path_(NULL) { }

PlatformReader::~PlatformReader() { }

const char *PlatformReader::platform_file_path() const {
  if (platform_file_path_)
    return platform_file_path_;
  return default_platform_file_path();
}

void PlatformReader::Initialize() {
  char *buf = new char[max_size() + 1];
  size_t bytes_read = helpers::read_file(platform_file_path(),
                                         buf,
                                         max_size());
  // read_file doesn't NUL-terminate.
  buf[bytes_read] = '\0';
  set_value(Process(buf, bytes_read));
  delete [] buf;
}

}  // namespace cros_boot_mode
