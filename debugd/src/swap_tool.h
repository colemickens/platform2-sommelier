// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_SWAP_TOOL_H_
#define DEBUGD_SRC_SWAP_TOOL_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class SwapTool {
 public:
  SwapTool() = default;
  ~SwapTool() = default;

  std::string SwapEnable(uint32_t size, bool change_now,
                         DBus::Error* error) const;
  std::string SwapDisable(bool change_now, DBus::Error* error) const;
  std::string SwapStartStop(bool on, DBus::Error* error) const;
  std::string SwapStatus(DBus::Error* error) const;
  std::string SwapSetMargin(uint32_t size, DBus::Error* error) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SwapTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_SWAP_TOOL_H_
