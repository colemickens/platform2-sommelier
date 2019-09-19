// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_CROS_HEALTHD_TOOL_H_
#define DEBUGD_SRC_CROS_HEALTHD_TOOL_H_

#include <string>

#include <base/macros.h>
#include <brillo/dbus/file_descriptor.h>
#include <brillo/errors/error.h>

namespace debugd {

class CrosHealthdTool {
 public:
  CrosHealthdTool() = default;
  ~CrosHealthdTool() = default;

  bool GetManufactureDate(brillo::ErrorPtr* error,
                          brillo::dbus_utils::FileDescriptor* outfd);

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosHealthdTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_CROS_HEALTHD_TOOL_H_
