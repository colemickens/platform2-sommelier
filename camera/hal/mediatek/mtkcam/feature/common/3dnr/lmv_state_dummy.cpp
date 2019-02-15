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

LmvStateMachineDummy::LmvStateMachineDummy(LmvState initState)
    : LmvStateMachine(initState) {
  // A dummy state machine, do nothing
}

void LmvStateMachineDummy::switchTo(SensorId sensor,
                                    Action* action,
                                    Transition* transition) {
  std::lock_guard<std::mutex> _l(mStateMutex);
  LmvState newState = mCurrentState;

  action->cmd = DO_NOTHING;

  switch (sensor) {
    case WIDE:
      newState = STATE_ON_WIDE;
      break;
    case TELE:
      newState = STATE_ON_TELE;
      break;
    default:
      CAM_LOGD("[LmvState] Invalid sensor ID: %d", static_cast<int>(sensor));
      break;
  }

  if (transition != NULL) {
    transition->oldState = mCurrentState;
    transition->newState = newState;
  }

  if (mCurrentState != newState) {
    CAM_LOGD("[LmvState] Switch-to: %d, State: %d -> %d, Action: %d",
             static_cast<int>(sensor), static_cast<int>(mCurrentState),
             static_cast<int>(newState), static_cast<int>(action->cmd));
  }

  mCurrentState = newState;
}

bool LmvStateMachineDummy::needEnque(SensorId sensor, bool willFollow) {
  return false;
}

void LmvStateMachineDummy::notifySwitchResult(SensorId sourcePath,
                                              SwitchResult result,
                                              Transition* transition) {
  // There should be no switch result!
  CAM_LOGE("[LmvState] notifySwitchResult: sourcePath: %d, result = %d",
           static_cast<int>(sourcePath), static_cast<int>(result));

  if (transition != NULL) {
    std::lock_guard<std::mutex> _l(mStateMutex);
    transition->oldState = mCurrentState;
    transition->newState = mCurrentState;
  }
}

void LmvStateMachineDummy::notifyLmvValidity(SensorId sourcePath,
                                             bool isValid,
                                             Transition* transition) {
  if (!isValid) {
    // Incorrect, both LMVs should be valid
    CAM_LOGE("[LmvState] notifyLmvValidity: sourcePath: %d, isValid = %d",
             static_cast<int>(sourcePath), static_cast<int>(isValid));
  }

  if (transition != NULL) {
    std::lock_guard<std::mutex> _l(mStateMutex);
    transition->oldState = mCurrentState;
    transition->newState = mCurrentState;
  }
}
