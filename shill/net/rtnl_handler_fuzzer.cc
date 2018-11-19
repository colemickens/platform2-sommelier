// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/rtnl_handler.h"
#include "shill/net/io_handler.h"

#include <base/at_exit.h>

namespace shill {

class RTNLHandlerFuzz {
 public:
  static void Run(const uint8_t* data, size_t size) {
    base::AtExitManager exit_manager;
    InputData input(static_cast<const unsigned char*>(data), size);
    RTNLHandler::GetInstance()->ParseRTNL(&input);
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  RTNLHandlerFuzz::Run(data, size);
  return 0;
}

}  // namespace shill
