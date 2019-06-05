// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_TRACEPATH_TOOL_H_
#define DEBUGD_SRC_TRACEPATH_TOOL_H_

#include <string>

#include <base/macros.h>
#include <brillo/variant_dictionary.h>

#include "debugd/src/subprocess_tool.h"

namespace debugd {

class TracePathTool : public SubprocessTool {
 public:
  TracePathTool() = default;
  ~TracePathTool() override = default;

  std::string Start(const base::ScopedFD& outfd,
                    const std::string& destination,
                    const brillo::VariantDictionary& options);

 private:
  DISALLOW_COPY_AND_ASSIGN(TracePathTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_TRACEPATH_TOOL_H_
