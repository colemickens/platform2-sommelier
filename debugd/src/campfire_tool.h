// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_CAMPFIRE_TOOL_H_
#define DEBUGD_SRC_CAMPFIRE_TOOL_H_

#include <string>

#include <base/macros.h>

namespace debugd {

class CampfireTool {
 public:
  CampfireTool() = default;
  ~CampfireTool() = default;

  std::string EnableAltOS(int size_gb);
  std::string DisableAltOS();

 private:
  std::string WriteTagFile(const std::string& content);

  DISALLOW_COPY_AND_ASSIGN(CampfireTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_CAMPFIRE_TOOL_H_
