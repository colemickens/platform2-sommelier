// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/fp_context_command_factory.h"

namespace biod {

std::unique_ptr<EcCommandInterface> FpContextCommandFactory::Create(
    CrosFpDeviceInterface* cros_fp, const std::string& user_id) {
  if (cros_fp->EcCmdVersionSupported(EC_CMD_FP_CONTEXT, 1) ==
      EcCmdVersionSupportStatus::SUPPORTED) {
    return FpContextCommand_v1::Create(user_id);
  }

  return FpContextCommand_v0::Create(user_id);
}

}  // namespace biod
