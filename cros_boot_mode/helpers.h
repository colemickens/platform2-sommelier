// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides basic file reading and parsing helpers for use in PlatformReader
// and PlatformReader derived classes.  Nothing fancy.

#ifndef CROS_BOOT_MODE_HELPERS_H_
#define CROS_BOOT_MODE_HELPERS_H_

#include <sys/types.h>

namespace cros_boot_mode {
namespace helpers {

// atoi, using sscanf
int to_int(const char *str, size_t length);

// very simple wrapper of fopen,fread,fclose.
size_t read_file(const char *path, char *buf, size_t max_bytes);

}  // namespace helpers
}  // namespace cros_boot_mode
#endif  // CROS_BOOT_MODE_HELPERS_H_
