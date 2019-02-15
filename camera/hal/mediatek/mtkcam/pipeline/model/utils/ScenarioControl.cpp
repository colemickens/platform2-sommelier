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

#define LOG_TAG "MtkCam3/HwUtils"
//
#include <mtkcam/utils/hw/CamManager.h>
#include <mtkcam/utils/std/Log.h>
#include "include/impl/ScenarioControl.h"
#include <dlfcn.h>
//
#include <memory>
#include <string.h>
#include <unordered_map>
#include <property_lib.h>
//
using NSCam::v3::pipeline::model::IScenarioControlV3;
/******************************************************************************
 *
 ******************************************************************************/
#define DUMP_SCENARIO_PARAM(_id, _str, _param)                                \
  do {                                                                        \
    MY_LOGD_IF(1, "(id:%d) %s: scenario %d: size %dx%d@%d feature 0x%x", _id, \
               _str, _param.scenario, _param.sensorSize.w,                    \
               _param.sensorSize.h, _param.sensorFps, _param.featureFlag);    \
  } while (0)

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Bandwidth control & dvfs
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class ScenarioControlV3 : public IScenarioControlV3 {
 public:
  explicit ScenarioControlV3(MINT32 const openId);
  virtual ~ScenarioControlV3();

 public:
  virtual MERROR enterScenario(ControlParam const& param);

  virtual MERROR enterScenario(MINT32 const scenario);

  virtual MERROR exitScenario();

  virtual MERROR boostScenario(int const scenario,
                               int const feature,
                               int64_t const frameNo);

  virtual MERROR checkIfNeedExitBoost(int64_t const frameNo,
                                      int const forceExit);
  //
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  RefBase Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                          Operations.
  void onLastStrongRef(const void* id);

 private:
  // Perfservice control
  virtual MERROR enterPerfService(ControlParam const& param);
  virtual MERROR exitPerfService();

  virtual MERROR changeBWCProfile(ControlParam const& param);

 private:
  MINT32 const mOpenId;
  ControlParam mCurParam;
  //
  MINT mEngMode;
  std::mutex mLock;
};
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam

/******************************************************************************
 *
 ******************************************************************************/
std::mutex gLock;
std::unordered_map<
    MINT32,
    std::weak_ptr<NSCam::v3::pipeline::model::ScenarioControlV3> >
    gScenarioControlMap;

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IScenarioControlV3> IScenarioControlV3::create(
    MINT32 const openId) {
  std::lock_guard<std::mutex> _l(gLock);
  std::shared_ptr<ScenarioControlV3> pControl = nullptr;
  auto it = gScenarioControlMap.find(openId);
  if (it == gScenarioControlMap.end()) {
    pControl = std::make_shared<ScenarioControlV3>(openId);
    gScenarioControlMap.emplace(openId, pControl);
  } else {
    MY_LOGW("dangerous, already have user with open id %d", openId);
    pControl = (it->second).lock();
  }
  //
  if (!pControl.get()) {
    MY_LOGE("cannot create properly");
  }
  //
  return pControl;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::pipeline::model::ScenarioControlV3::ScenarioControlV3(
    MINT32 const openId)
    : mOpenId(openId) {
  mCurParam.scenario = Scenario_None;
  mEngMode = -1;
}

/******************************************************************************
 *
 ******************************************************************************/
void NSCam::v3::pipeline::model::ScenarioControlV3::onLastStrongRef(
    const void* /*id*/) {
  // reset
  if (mCurParam.scenario != Scenario_None) {
    exitScenario();
  }
  exitPerfService();
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    auto it = gScenarioControlMap.find(mOpenId);
    if (it != gScenarioControlMap.end()) {
      gScenarioControlMap.erase(it);
    } else {
      MY_LOGW("dangerous, has been removed (open id %d)", mOpenId);
    }
  }
}

NSCam::v3::pipeline::model::ScenarioControlV3::~ScenarioControlV3() {
  MY_LOGD("deconstruction");
  onLastStrongRef(nullptr);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::enterPerfService(
    ControlParam const& param) {
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::exitPerfService() {
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::changeBWCProfile(
    ControlParam const& param) {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::enterScenario(
    MINT32 const scenario) {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::enterScenario(
    ControlParam const& param) {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::exitScenario() {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::boostScenario(
    int const scenario, int const feature, int64_t const frameNo) {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::pipeline::model::ScenarioControlV3::checkIfNeedExitBoost(
    int64_t const frameNo, int const forceExit) {
  return OK;
}
