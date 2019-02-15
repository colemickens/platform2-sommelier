/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LmvState"

#include <lmv_state_impl.h>
#include <mtkcam/feature/3dnr/lmv_state.h>
#include <mtkcam/utils/std/Log.h>

#include <memory>

std::shared_ptr<LmvStateMachine> LmvStateMachine::spInstance = nullptr;
std::mutex LmvStateMachine::sSingletonMutex;

std::shared_ptr<LmvStateMachine> LmvStateMachine::createInstance(
    LmvState initState) {
  std::lock_guard<std::mutex> _l(sSingletonMutex);

  if (spInstance == NULL) {
    spInstance = std::make_shared<LmvStateMachineUni>(initState);
  }

  return spInstance;
}

std::shared_ptr<LmvStateMachine> LmvStateMachine::getInstance() {
  return spInstance;
}

LmvStateMachine::LmvState LmvStateMachine::getCurrentState() {
  std::lock_guard<std::mutex> _l(sSingletonMutex);
  return mCurrentState;
}

LmvStateMachine::LmvStateMachine(LmvState initState) {
  mCurrentState = initState;
}

LmvStateMachine::~LmvStateMachine() {
  // Do nothing
}
