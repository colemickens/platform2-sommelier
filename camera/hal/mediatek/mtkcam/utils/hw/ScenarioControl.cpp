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

#define LOG_TAG "MtkCam/HwUtils"
//
#include <bandwidth_control.h>
#include <camera_custom_scenario_control.h>
#include <dlfcn.h>
#include <map>
#include <memory>
//
#include <mtkcam/utils/hw/CamManager.h>
#include <mtkcam/utils/hw/ScenarioControl.h>
#include <mtkcam/utils/std/Log.h>
//
#include <string.h>
//

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
BWC_PROFILE_TYPE mapToBWCProfile(MINT32 const scenario) {
  switch (scenario) {
    case IScenarioControl::Scenario_NormalPreivew:
      return BWCPT_CAMERA_PREVIEW;
    case IScenarioControl::Scenario_ZsdPreview:
      return BWCPT_CAMERA_ZSD;
    case IScenarioControl::Scenario_VideoRecord:
      return BWCPT_VIDEO_RECORD_CAMERA;
    case IScenarioControl::Scenario_VSS:
      return BWCPT_VIDEO_SNAPSHOT;
    case IScenarioControl::Scenario_Capture:
      return BWCPT_CAMERA_CAPTURE;
    case IScenarioControl::Scenario_ContinuousShot:
      return BWCPT_CAMERA_ICFP;
    case IScenarioControl::Scenario_VideoTelephony:
      return BWCPT_VIDEO_TELEPHONY;
    case IScenarioControl::Scenario_HighSpeedVideo:
      return BWCPT_VIDEO_RECORD_SLOWMOTION;
    default:
      MY_LOGE("not supported scenario %d", scenario);
  }
  return BWCPT_NONE;
}

/******************************************************************************
 *
 ******************************************************************************/
std::mutex gLock;
std::map<MINT32, wp<ScenarioControl> > gScenarioControlMap;

/* for EnterScenerio() multi thread */
std::mutex gEnterLock;

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
isMultiOpen() {
  std::lock_guard<std::mutex> _l(gLock);
  return gScenarioControlMap.size() > 1;
}

/******************************************************************************
 *
 ******************************************************************************/
sp<IScenarioControl> IScenarioControl::create(MINT32 const openId) {
  std::lock_guard<std::mutex> _l(gLock);
  sp<ScenarioControl> pControl = NULL;
  ssize_t index = gScenarioControlMap.indexOfKey(openId);
  if (index < 0) {
    pControl = std::make_shared<ScenarioControl>(openId);
    gScenarioControlMap.add(openId, pControl);
  } else {
    MY_LOGW("dangerous, already have user with open id %d", openId);
    pControl = gScenarioControlMap.valueAt(index).promote();
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
ScenarioControl::ScenarioControl(MINT32 const openId)
    : mOpenId(openId), mCurPerfHandle(-1) {
  mCurParam.scenario = Scenario_None;
  mEngMode = -1;
  char value[PROPERTY_VALUE_MAX];
  property_get("ro.build.type", value, "eng");
  if (0 == strcmp(value, "eng")) {
    mEngMode = 1;
  } else {
    mEngMode = 0;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
void ScenarioControl::onLastStrongRef(const void* /*id*/) {
  // reset
  if (mCurParam.scenario != Scenario_None) {
    exitScenario();
  }
  exitPerfService();
  //
  {
    std::lock_guard<std::mutex> _l(gLock);
    ssize_t index = gScenarioControlMap.indexOfKey(mOpenId);
    if (index >= 0) {
      gScenarioControlMap.removeItemsAt(index);
    } else {
      MY_LOGW("dangerous, has been removed (open id %d)", mOpenId);
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
ScenarioControl::~ScenarioControl() {
  MY_LOGD("+");
  onLastStrongRef(nullptr);
}

MERROR
ScenarioControl::enterPerfService(ControlParam const& param) {
  //
  MY_LOGD("");

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::exitPerfService() {
  //
  if (mCurPerfHandle != -1) {
    std::shared_ptr<IPower> pPerf = IPower::getService();
    //
    pPerf->scnDisable(mCurPerfHandle);
    pPerf->scnUnreg(mCurPerfHandle);
    //
    mCurPerfHandle = -1;
    MY_LOGD("perfService disable");
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::enterScenario(MINT32 const scenario) {
  ControlParam param;
  param.scenario = scenario;
  param.sensorSize = mCurParam.sensorSize;
  param.sensorFps = mCurParam.sensorFps;
  param.featureFlag = mCurParam.featureFlag;
  param.videoSize = mCurParam.videoSize;
  param.camMode = mCurParam.camMode;

  return enterScenario(param);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::enterScenario(ControlParam const& param) {
  DUMP_SCENARIO_PARAM(mOpenId, "enter:", param);
  //
  std::lock_guard<std::mutex> _l(gEnterLock);
  //
  BWC_PROFILE_TYPE type = mapToBWCProfile(param.scenario);
  if (type == BWCPT_NONE) {
    return BAD_VALUE;
  }
  //
  // exit previous perfservice setting
  exitPerfService();
  // enter new perfservice setting
  enterPerfService(param);
  //
  if (param.enableBWCControl) {
    BWC BwcIns;
    BwcIns.Profile_Change(type, true);
    //
    MUINT32 multiple = 1;
    if (FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_DUAL_PD)) {
      multiple = 2;
    }
    //
    MY_LOGD(
        "mmdvfs_set type(%d) multiple(%d) sensorSize(%d) finalSize(%d) fps(%d) "
        "isMultiOpen(%d)",
        type, multiple, param.sensorSize.size(),
        param.sensorSize.size() * multiple, param.sensorFps, isMultiOpen());
    //
    mmdvfs_set(type, MMDVFS_SENSOR_SIZE,
               param.sensorSize.size() * multiple,  // param.sensorSize.size(),
               MMDVFS_SENSOR_FPS, param.sensorFps, MMDVFS_PREVIEW_SIZE,
               param.videoSize.w * param.videoSize.h, MMDVFS_CAMERA_MODE_PIP,
               isMultiOpen() && !isDualZoomMode(param.featureFlag),
               MMDVFS_CAMERA_MODE_DUAL_ZOOM, isDualZoomMode(param.featureFlag),
               MMDVFS_CAMERA_MODE_VFB,
               FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_VFB),
               MMDVFS_CAMERA_MODE_EIS_2_0,
               FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_ADV_EIS),
               MMDVFS_CAMERA_MODE_IVHDR,
               FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_IVHDR),
               MMDVFS_CAMERA_MODE_MVHDR,
               FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_MVHDR),
               MMDVFS_CAMERA_MODE_ZVHDR,
               FEATURE_CFG_IS_ENABLED(param.featureFlag, FEATURE_ZVHDR),
               MMDVFS_CAMERA_MODE_STEREO, isInStereoMode(param.featureFlag),
               MMDVFS_PARAMETER_EOF);
  }

  if (mCurParam.scenario != Scenario_None &&
      mCurParam.scenario != param.scenario) {
    MY_LOGD("exit previous scenario setting");
    exitScenario();
  }
  // keep param
  mCurParam = param;
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ScenarioControl::exitScenario() {
  if (mCurParam.scenario == Scenario_None) {
    MY_LOGD("already exit");
    return OK;
  }
  DUMP_SCENARIO_PARAM(mOpenId, "exit:", mCurParam);
  BWC_PROFILE_TYPE type = mapToBWCProfile(mCurParam.scenario);
  if (type == BWCPT_NONE) {
    return BAD_VALUE;
  }
  //
  if (mCurParam.enableBWCControl) {
    BWC BwcIns;
    BwcIns.Profile_Change(type, false);
  }
  // reset param
  mCurParam.scenario = Scenario_None;
  //
  return OK;
}
