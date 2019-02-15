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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_SCENARIOCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_SCENARIOCONTROL_H_
//
#include <mtkcam/utils/hw/IScenarioControl.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Bandwidth control & dvfs
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class ScenarioControl : public IScenarioControl {
 public:
  explicit ScenarioControl(MINT32 const openId);
  virtual ~ScenarioControl();

 public:
  virtual MERROR enterScenario(ControlParam const& param);

  virtual MERROR enterScenario(MINT32 const scenario);

  virtual MERROR exitScenario();
  //
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  RefBase Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Operations.
  void onLastStrongRef(const void* id);

 private:
  // Perfservice control
  virtual MERROR enterPerfService(ControlParam const& param);
  virtual MERROR exitPerfService();

 private:
  MINT32 const mOpenId;
  ControlParam mCurParam;
  //
  int mCurPerfHandle;
  MINT mEngMode;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_SCENARIOCONTROL_H_
