// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides the implementation of the helper functions for PlatformReader
// and derived classes.

#include "helpers.h"

#include <stdio.h>
#include <sys/types.h>

#include "platform_reader.h"

namespace cros_boot_mode {
namespace helpers {

size_t read_file(const char *path, char *buf, size_t max_bytes) {
  if (!buf)
    return 0;
  ::FILE *fp = ::fopen(path, "r");
  if (!fp || !max_bytes)
    return 0;
  size_t bytes_read = ::fread(buf, 1, max_bytes, fp);

  // If max_bytes doesn't consume the entire file, return 0 bytes read.
  // This is meant to ensure that unexpected platform changes appear as
  // kUnsupported rather than randomly based on truncation.
  if (::fgetc(fp) != EOF) {
    bytes_read = 0;
  }

  ::fclose(fp);
  return bytes_read;
}

int to_int(const char *file_contents, size_t length) {
  if (!length)
    return PlatformReader::kUnsupported;
  int enum_value = PlatformReader::kUnsupported;
  if (sscanf(file_contents, "%d", &enum_value) != 1 || enum_value < 0)
    return PlatformReader::kUnsupported;
  return enum_value;
}

}  // namespace helpers
}  // namespace cros_boot_mode
