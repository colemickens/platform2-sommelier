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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_3DNR_LMV_STATE_IMPL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_3DNR_LMV_STATE_IMPL_H_

#include <mtkcam/feature/3dnr/lmv_state.h>

class LmvStateMachineUni : public LmvStateMachine {
 public:
  virtual void switchTo(SensorId sensor,
                        Action* action,
                        Transition* transition = NULL);
  virtual void notifySwitchResult(SensorId sourcePath,
                                  SwitchResult result,
                                  Transition* transition = NULL);
  virtual void notifyLmvValidity(SensorId sourcePath,
                                 bool isValid,
                                 Transition* transition = NULL);
  virtual bool needEnque(SensorId sensor, bool willFollow);

  explicit LmvStateMachineUni(LmvState initState);

 private:
  int mWrongStateCount;
  int mEnqueRemain[SENSOR_ID_MAX];

  static const int N_EXTRA_ENQUES;
};

class LmvStateMachineDummy : public LmvStateMachine {
 public:
  virtual void switchTo(SensorId sensor,
                        Action* action,
                        Transition* transition = NULL);
  virtual void notifySwitchResult(SensorId sourcePath,
                                  SwitchResult result,
                                  Transition* transition = NULL);
  virtual void notifyLmvValidity(SensorId sourcePath,
                                 bool isValid,
                                 Transition* transition = NULL);
  virtual bool needEnque(SensorId sensor, bool willFollow);

  explicit LmvStateMachineDummy(LmvState initState);
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_3DNR_LMV_STATE_IMPL_H_
