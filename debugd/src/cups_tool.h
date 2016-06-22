// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_CUPS_TOOL_H_
#define DEBUGD_SRC_CUPS_TOOL_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class CupsTool {
 public:
  CupsTool() = default;
  ~CupsTool() = default;

  void ResetState(DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(CupsTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_CUPS_TOOL_H_
