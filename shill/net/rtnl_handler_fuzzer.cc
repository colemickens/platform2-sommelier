//
// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/net/rtnl_handler.h"
#include "shill/net/io_handler.h"

#include <base/at_exit.h>

namespace shill {

class RTNLHandlerFuzz {
 public:
  static void Run(const uint8_t *data, size_t size) {
    base::AtExitManager exit_manager;
    InputData input((unsigned char *)data, size);
    RTNLHandler::GetInstance()->ParseRTNL(&input);
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  RTNLHandlerFuzz::Run(data, size);
  return 0;
}

}  // namespace shill
