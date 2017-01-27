//
// Copyright (C) 2017 The Android Open Source Project
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

#ifndef TRUNKS_MOCK_RESOURCE_MANAGER_H_
#define TRUNKS_MOCK_RESOURCE_MANAGER_H_

#include <gmock/gmock.h>

#include "trunks/resource_manager.h"

namespace trunks {

class MockResourceManager : public ResourceManager {
 public:
  MockResourceManager(const TrunksFactory& factory,
                      CommandTransceiver* next_transceiver)
    : ResourceManager(factory, next_transceiver) {}

  MOCK_METHOD0(Suspend, void());
  MOCK_METHOD0(Resume, void());
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_RESOURCE_MANAGER_H_
