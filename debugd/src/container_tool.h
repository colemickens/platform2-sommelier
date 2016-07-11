// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_CONTAINER_TOOL_H_
#define DEBUGD_SRC_CONTAINER_TOOL_H_

#include <base/macros.h>

namespace debugd {

class ContainerTool {
 public:
  ContainerTool() = default;
  ~ContainerTool() = default;

  void ContainerStarted();
  void ContainerStopped();

 private:
  bool device_jail_started_;

  DISALLOW_COPY_AND_ASSIGN(ContainerTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_CONTAINER_TOOL_H_
