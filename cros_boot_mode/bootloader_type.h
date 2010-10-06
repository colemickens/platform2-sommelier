// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the BootloaderType class.  This class wraps the kernel commandline
// to determine the bootloader type.  Our bootloaders are configured to boot
// with identifying kernel command lines:
// - EFI uses cros_efi
// - Legacy uses cros_legacy
// - Chrome OS uses cros_secure
// - A debug override can use cros_debug.
// This done by walking /proc/cmdline.
//
#ifndef CROS_BOOT_MODE_BOOTLOADER_TYPE_H_
#define CROS_BOOT_MODE_BOOTLOADER_TYPE_H_

#include <sys/types.h>

#include "platform_reader.h"

namespace cros_boot_mode {

class BootloaderType : public PlatformReader {
 public:
  static const char *kBootloaderTypePath;
  enum {
    kDebug = 0,
    kChromeOS,
    kEFI,
    kLegacy,
  };
  // API-exposed names
  static const char *kBootloaderTypeText[];
  static const size_t kBootloaderTypeCount;
  // Maximum allowed /proc/cmdline size.
  static const int kMaxKernelCmdlineSize;
  // Functional names found in /proc/cmdline.
  static const char *kSupportedBootloaders[];


  BootloaderType();
  virtual ~BootloaderType();

  // Process walks over the /proc/cmdline and converts it to the enum
  // above.  The conversion is done by finding the first match in
  // kSupportedBootloaders and then emits the enum that corresponds to
  // the matching array index (or kUnsupported).
  virtual int Process(const char *contents, size_t length);
  virtual const char *name() const {
    return "bootloader_type";
  }
  virtual const char *c_str() const {
    return (value() >= 0 ? kBootloaderTypeText[value()] : "unsupported");
  }
  virtual const char *default_platform_file_path() const {
    return "/proc/cmdline";
  }
  virtual size_t max_size() const {
    return kMaxKernelCmdlineSize;
  }
};

}  // namespace cros_boot_mode
#endif  // CROS_BOOT_MODE_BOOTLOADER_TYPE_H_
