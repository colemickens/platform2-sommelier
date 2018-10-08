// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_VERIFY_RO_TOOL_H_
#define DEBUGD_SRC_VERIFY_RO_TOOL_H_

#include <string>

#include <base/macros.h>

#include "debugd/src/process_with_output.h"
#include "debugd/src/subprocess_tool.h"

namespace debugd {

class VerifyRoTool : public SubprocessTool {
 public:
  VerifyRoTool() = default;
  ~VerifyRoTool() override = default;

  // Returns the USB-connected DUT's Cr50 RW firmware version on success or
  // error messages if the DUT isn't available or compatible or if an error
  // happens. A normal output on success looks like
  //
  //     RW_FW_VER=0.3.10
  std::string GetGscOnUsbRWFirmwareVer();

 private:
  // Returns the first line that starts with "|key|=" from the output of the
  // given |process| or an error message if such line is not found or something
  // wrong happens.
  std::string GetLineWithKeyFromProcess(const ProcessWithOutput& process,
                                        const std::string& key);

  DISALLOW_COPY_AND_ASSIGN(VerifyRoTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_VERIFY_RO_TOOL_H_
