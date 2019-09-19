// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_FP_CONTEXT_COMMAND_FACTORY_H_
#define BIOD_FP_CONTEXT_COMMAND_FACTORY_H_

#include <memory>
#include <string>

#include "biod/cros_fp_device_interface.h"
#include "biod/fp_context_command.h"

namespace biod {

class FpContextCommandFactory {
 public:
  static std::unique_ptr<EcCommandInterface> Create(
      CrosFpDeviceInterface* cros_fp, const std::string& user_id);
};

}  // namespace biod

#endif  // BIOD_FP_CONTEXT_COMMAND_FACTORY_H_
