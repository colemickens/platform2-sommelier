//
// Copyright (C) 2012 The Android Open Source Project
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

#ifndef SHILL_MOCK_PROCESS_KILLER_H_
#define SHILL_MOCK_PROCESS_KILLER_H_

#include <gmock/gmock.h>

#include "shill/process_killer.h"

namespace shill {

class MockProcessKiller : public ProcessKiller {
 public:
  MockProcessKiller();
  ~MockProcessKiller() override;

  MOCK_METHOD2(Wait, bool(int pid, const base::Closure& callback));
  MOCK_METHOD2(Kill, void(int pid, const base::Closure& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProcessKiller);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROCESS_KILLER_H_
