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

const int LmvStateMachineUni::N_EXTRA_ENQUES = 1;

LmvStateMachineUni::LmvStateMachineUni(LmvState initState)
    : LmvStateMachine(initState) {
  mWrongStateCount = 0;
  mEnqueRemain[WIDE] = 0;
  mEnqueRemain[TELE] = 0;
}

void LmvStateMachineUni::switchTo(SensorId sensor,
                                  Action* action,
                                  Transition* transition) {
  std::lock_guard<std::mutex> _l(mStateMutex);
  LmvState newState = mCurrentState;

  action->cmd = DO_NOTHING;

  switch (mCurrentState) {
    case STATE_ON_WIDE:
      if (sensor == TELE) {
        newState = STATE_GOING_TO_TELE;
        action->cmd = SEND_SWITCH_OUT_TO_WIDE;
        mEnqueRemain[WIDE] = N_EXTRA_ENQUES;
      }
      // else ignore
      break;
    case STATE_GOING_TO_TELE:
      // Ignore any switch-to query, we are waiting for the result
      break;
    case STATE_ON_TELE:
      if (sensor == WIDE) {
        newState = STATE_GOING_TO_WIDE;
        action->cmd = SEND_SWITCH_OUT_TO_TELE;
        mEnqueRemain[TELE] = N_EXTRA_ENQUES;
      }
      // else ignore
      break;
    case STATE_GOING_TO_WIDE:
      // Ignore any switch-to query, we are waiting for the result
      break;
  }

  if (transition != nullptr) {
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

bool LmvStateMachineUni::needEnque(SensorId sensor, bool willFollow) {
  std::lock_guard<std::mutex> _l(mStateMutex);

  bool need = (mEnqueRemain[sensor] > 0);

  if (need && willFollow) {
    CAM_LOGD("[LmvState] needEnque(): Will enque (mEnqueRemain[%d] = %d)",
             static_cast<int>(sensor), mEnqueRemain[sensor]);
    mEnqueRemain[sensor]--;
  }

  return need;
}

void LmvStateMachineUni::notifySwitchResult(SensorId sourcePath,
                                            SwitchResult result,
                                            Transition* transition) {
  std::lock_guard<std::mutex> _l(mStateMutex);
  LmvState newState = mCurrentState;

  switch (mCurrentState) {
    case STATE_GOING_TO_TELE:
      if (result == RESULT_OK) {
        newState = STATE_ON_TELE;
      } else if (result == RESULT_FAILED) {
        newState = STATE_ON_TELE;
      } else if (result == RESULT_SWITCHING) {
        newState = STATE_ON_WIDE;
      }
      break;
    case STATE_GOING_TO_WIDE:
      if (result == RESULT_OK) {
        newState = STATE_ON_WIDE;
      } else if (result == RESULT_FAILED) {
        newState = STATE_ON_WIDE;
      } else if (result == RESULT_SWITCHING) {
        newState = STATE_ON_TELE;
      }
      break;
    default:
      CAM_LOGE("[LmvState] State: %d, Source path: %d, Result: %d",
               static_cast<int>(mCurrentState), static_cast<int>(sourcePath),
               static_cast<int>(result));
      break;
  }

  if (transition != nullptr) {
    transition->oldState = mCurrentState;
    transition->newState = newState;
  }

  if (mCurrentState != newState) {
    CAM_LOGD("[LmvState] Result: %d, State: %d -> %d", static_cast<int>(result),
             static_cast<int>(mCurrentState), static_cast<int>(newState));
  }

  mCurrentState = newState;
}

void LmvStateMachineUni::notifyLmvValidity(SensorId sourcePath,
                                           bool isValid,
                                           Transition* transition) {
  if (!isValid) {
    return;
  }

  // Auto-recovery machanism

  std::lock_guard<std::mutex> _l(mStateMutex);

  switch (mCurrentState) {
    case STATE_ON_WIDE:
      if (sourcePath == WIDE) {
        mWrongStateCount = 0;  // correct
      } else {
        mWrongStateCount += 2;
        CAM_LOGW("[LmvState] State: %d, but valid LMV from %d", mCurrentState,
                 sourcePath);
      }
      break;
    case STATE_ON_TELE:
      if (sourcePath == TELE) {
        mWrongStateCount = 0;  // correct
      } else {
        mWrongStateCount += 2;
        CAM_LOGW("[LmvState] State: %d, but valid LMV from %d", mCurrentState,
                 sourcePath);
      }
      break;
    case STATE_GOING_TO_TELE:
      // temporary state
      mWrongStateCount = (sourcePath == TELE) ? 0 : (mWrongStateCount + 1);
      break;
    case STATE_GOING_TO_WIDE:
      // temporary state
      mWrongStateCount = (sourcePath == WIDE) ? 0 : (mWrongStateCount + 1);
      break;
  }

  if (transition != NULL) {
    transition->oldState = mCurrentState;
  }

  if (mWrongStateCount >= 10) {  // too many wrongs
    LmvState newState = mCurrentState;

    if (sourcePath == WIDE) {
      newState = STATE_ON_WIDE;
    } else {
      newState = STATE_ON_TELE;
    }

    mWrongStateCount = 0;

    CAM_LOGE(
        "[LmvState] State recovery: %d -> %d because received too many LMV "
        "validity from wrong path",
        static_cast<int>(mCurrentState), static_cast<int>(newState));

    mCurrentState = newState;
  }

  if (transition != NULL) {
    transition->newState = mCurrentState;
  }
}
