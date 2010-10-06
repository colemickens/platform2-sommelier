// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BootloaderType implementation

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <string>

#include "bootloader_type.h"

namespace cros_boot_mode {

const char *BootloaderType::kBootloaderTypeText[] = {
 "debug",  // cros_debug (cros_fw with developer override)
 "chromeos",  // cros_fw (any platform)
 "efi",  // cros_efi
 "legacy",  // cros_legacy
};
const size_t BootloaderType::kBootloaderTypeCount =
  sizeof(kBootloaderTypeText) / sizeof(*kBootloaderTypeText);
// These values are expected to be found in the kernel commandline with space
// or '\0' word boundaries.  The ordering of this array must correspond to
// the array above and the defined enum.
const char *BootloaderType::kSupportedBootloaders[] = {
  "cros_debug",
  "cros_secure",
  "cros_efi",
  "cros_legacy",
};

const int BootloaderType::kMaxKernelCmdlineSize = 4096;  // one page.

BootloaderType::BootloaderType()  { }
BootloaderType::~BootloaderType() { }

// This function looks for given argument space or nul delimited in
// a /proc/cmdline style char array. See bootloader_type.h for details.
int BootloaderType::Process(const char *contents, size_t length) {
  if (!length || !contents)
    return kUnsupported;

  static const char kDelimiter = ' ';
  static const char kTerminal = '\0';
  for (unsigned i = 0; i < kBootloaderTypeCount; ++i) {
    const char *candidate = kSupportedBootloaders[i];
    const char *start = strstr(contents, candidate);
    while (start) {
      const char *end = start + strlen(candidate);
      if ((*end == kTerminal || *end == kDelimiter) &&
          (start == contents || *(start - 1) == kDelimiter))
        return i;
      start = strstr(end, candidate);
    }
  }
  return kUnsupported;
}

}  // namespace cros_boot_mode
