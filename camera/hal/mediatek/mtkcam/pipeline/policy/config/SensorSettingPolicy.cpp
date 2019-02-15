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
 * See the License for the specific language governing permissions and the
 * limitations under the License.
 */

#define LOG_TAG "mtkcam-SensorSettingPolicy"

#include <compiler.h>
#include <map>
#include <memory>
#include <mtkcam/pipeline/policy/ISensorSettingPolicy.h>
//
#include "MyUtils.h"

#include <mtkcam/drv/IHalSensor.h>

#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/hw/HwTransform.h>
#include <unordered_map>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
/******************************************************************************
 *
 ******************************************************************************/
#define CHECK_ERROR(_err_)                          \
  do {                                              \
    MERROR const err = (_err_);                     \
    if (CC_UNLIKELY(err != OK)) {                   \
      MY_LOGE("err:%d(%s)", err, ::strerror(-err)); \
      return err;                                   \
    }                                               \
  } while (0)

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

enum eMode {
  eNORMAL_PREVIEW = 0,
  eNORMAL_VIDEO,
  eNORMAL_CAPTURE,
  eSLIM_VIDEO1,
  eSLIM_VIDEO2,
  eNUM_SENSOR_MODE,
};

const char* kModeNames[eMode::eNUM_SENSOR_MODE + 1] = {
    "PREVIEW", "VIDEO", "CAPTURE", "SLIM_VIDEO1", "SLIM_VIDEO2", "UNDEFINED"};

struct SensorParams {
  std::unordered_map<eMode, SensorSetting> mSetting;
  std::unordered_map<eMode, eMode> mAltMode;
  bool bSupportPrvMode = true;
};

static auto parseSensorParamsSetting(std::shared_ptr<SensorParams>* pParams,
                                     const NSCamHW::HwInfoHelper* rHelper)
    -> int {
#define addStaticParams(idx, _scenarioId_, _item_)                   \
  do {                                                               \
    int32_t fps;                                                     \
    MSize size;                                                      \
    if (CC_UNLIKELY(!rHelper->getSensorFps(_scenarioId_, &fps))) {   \
      MY_LOGW("getSensorFps fail");                                  \
      break;                                                         \
    }                                                                \
    if (CC_UNLIKELY(!rHelper->getSensorSize(_scenarioId_, &size))) { \
      MY_LOGW("getSensorSize fail");                                 \
      break;                                                         \
    }                                                                \
    _item_->mSetting[idx].sensorFps = static_cast<uint32_t>(fps);    \
    _item_->mSetting[idx].sensorSize = size;                         \
    _item_->mSetting[idx].sensorMode = _scenarioId_;                 \
    _item_->mAltMode[idx] = idx;                                     \
    MY_LOGD("candidate mode %d, size(%d, %d)@%d", idx,               \
            _item_->mSetting[idx].sensorSize.w,                      \
            _item_->mSetting[idx].sensorSize.h,                      \
            _item_->mSetting[idx].sensorFps);                        \
  } while (0)

  addStaticParams(eNORMAL_PREVIEW, SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
                  (*pParams));
  addStaticParams(eNORMAL_VIDEO, SENSOR_SCENARIO_ID_NORMAL_VIDEO, (*pParams));
  addStaticParams(eNORMAL_CAPTURE, SENSOR_SCENARIO_ID_NORMAL_CAPTURE,
                  (*pParams));
  addStaticParams(eSLIM_VIDEO1, SENSOR_SCENARIO_ID_SLIM_VIDEO1, (*pParams));
  addStaticParams(eSLIM_VIDEO2, SENSOR_SCENARIO_ID_SLIM_VIDEO2, (*pParams));

#undef addStaticParams

  //
  return OK;
}

static auto checkVhdrSensor(SensorSetting* res,
                            std::shared_ptr<SensorParams> const& pParams,
                            const uint32_t vhdrMode,
                            const NSCamHW::HwInfoHelper& rHelper) -> int {
  uint32_t supportHDRMode = 0;
  char forceSensorMode[PROPERTY_VALUE_MAX];
  property_get("vendor.debug.force.vhdr.sensormode", forceSensorMode, "0");
  switch (forceSensorMode[0]) {
    case '0':
      break;
    case 'P':
    case 'p':
      (*res) = pParams->mSetting[eNORMAL_PREVIEW];
      MY_LOGD("set sensor mode to NORMAL_PREVIEW(%d)",
              SENSOR_SCENARIO_ID_NORMAL_PREVIEW);
      return OK;
    case 'V':
    case 'v':
      (*res) = pParams->mSetting[eNORMAL_VIDEO];
      MY_LOGD("set sensor mode to NORMAL_VIDEO(%d)",
              SENSOR_SCENARIO_ID_NORMAL_VIDEO);
      return OK;
    case 'C':
    case 'c':
      (*res) = pParams->mSetting[eNORMAL_CAPTURE];
      MY_LOGD("set sensor mode to NORMAL_CAPTURE(%d)",
              SENSOR_SCENARIO_ID_NORMAL_CAPTURE);
      return OK;
    default:
      MY_LOGW("unknown force sensor mode(%s), not used", forceSensorMode);
      MY_LOGW("usage : setprop debug.force.vhdr.sensormode P/V/C");
      break;
  }

  // 1. Current sensor mode is VHDR support, use it.
  if (CC_UNLIKELY(
          !rHelper.querySupportVHDRMode(res->sensorMode, &supportHDRMode))) {
    MY_LOGE("[vhdrhal] HwInfoHelper querySupportVHDRMode fail");
    return -EINVAL;
  }
  if (vhdrMode == supportHDRMode) {
    MY_LOGD(
        "[vhdrhal] senosr setting : vhdrMode_supportHDRMode_sensormode(%d,%d, "
        "%d)",
        vhdrMode, supportHDRMode, res->sensorMode);
    return OK;
  }

  // 2. Check sensor mode in order: preview -> video -> capture
  // Find acceptable sensor mode for this vhdrMode
  /*
         SENSOR_SCENARIO_ID_NORMAL_PREVIEW
         SENSOR_SCENARIO_ID_NORMAL_CAPTURE
         SENSOR_SCENARIO_ID_NORMAL_VIDEO
         SENSOR_SCENARIO_ID_SLIM_VIDEO1
         SENSOR_SCENARIO_ID_SLIM_VIDEO2
    */

#define CHECK_SENSOR_MODE_VHDR_SUPPORT(senMode, scenariomode)             \
  do {                                                                    \
    if (!rHelper.querySupportVHDRMode(senMode, &supportHDRMode)) {        \
      return -EINVAL;                                                     \
    }                                                                     \
    if (vhdrMode == supportHDRMode) {                                     \
      res = &pParams->mSetting[scenariomode];                             \
      MY_LOGD("[vhdrhal] re-try senosr setting : (%d, %d, %d)", vhdrMode, \
              supportHDRMode, res->sensorMode);                           \
      return OK;                                                          \
    }                                                                     \
  } while (0)
  CHECK_SENSOR_MODE_VHDR_SUPPORT(SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
                                 eNORMAL_PREVIEW);
  CHECK_SENSOR_MODE_VHDR_SUPPORT(SENSOR_SCENARIO_ID_NORMAL_VIDEO,
                                 eNORMAL_VIDEO);
  CHECK_SENSOR_MODE_VHDR_SUPPORT(SENSOR_SCENARIO_ID_NORMAL_CAPTURE,
                                 eNORMAL_CAPTURE);
#undef CHECK_SENSOR_MODE_VHDR_SUPPORT

  // 3.  PREVIEW & VIDEO & CAPTURE mode are all not acceptable
  MY_LOGE("[vhdrhal] VHDR not support preview & video & capture mode.");
  return -EINVAL;
}

static auto determineAlternativeMode(std::shared_ptr<SensorParams>* pParams,
                                     NSCamHW::HwTransHelper* rHelper) -> int {
  auto verifyFov = [](NSCamHW::HwTransHelper* rHelper,
                      uint32_t const mode) -> bool {
#define FOV_DIFF_TOLERANCE (0.002)
    float dX = 0.0f;
    float dY = 0.0f;
    return (rHelper->calculateFovDifference(mode, &dX, &dY) &&
            dX < FOV_DIFF_TOLERANCE && dY < FOV_DIFF_TOLERANCE)
               ? true
               : false;
  };

  bool bAcceptPrv =
      verifyFov(rHelper, (*pParams)->mSetting[eNORMAL_PREVIEW].sensorMode);
  bool bAcceptVid =
      verifyFov(rHelper, (*pParams)->mSetting[eNORMAL_VIDEO].sensorMode);

  if (!bAcceptPrv) {
    if (!bAcceptVid) {
      (*pParams)->mAltMode[eNORMAL_VIDEO] = eNORMAL_CAPTURE;
      (*pParams)->mAltMode[eNORMAL_PREVIEW] = eNORMAL_CAPTURE;
    } else {
      (*pParams)->mAltMode[eNORMAL_PREVIEW] = eNORMAL_VIDEO;
    }
  } else if (!bAcceptVid) {
    if ((*pParams)->mSetting[eNORMAL_CAPTURE].sensorFps >= 30) {
      (*pParams)->mAltMode[eNORMAL_VIDEO] = eNORMAL_CAPTURE;
    } else if ((*pParams)->bSupportPrvMode) {
      (*pParams)->mAltMode[eNORMAL_VIDEO] = eNORMAL_PREVIEW;
    } else {
      (*pParams)->mAltMode[eNORMAL_VIDEO] = eNORMAL_VIDEO;
    }
  }

  for (auto const& alt : (*pParams)->mAltMode) {
    MY_LOGD("alt sensor mode: %s -> %s", kModeNames[alt.first],
            kModeNames[alt.second]);
  }

  return OK;
}

static auto determineScenDefault(
    SensorSetting* res,
    std::shared_ptr<SensorParams> const& pParams,
    std::shared_ptr<ParsedAppImageStreamInfo> const& pParsedAppImageInfo)
    -> int {
  bool hit = false;
  for (int i = 0; i < eNUM_SENSOR_MODE; ++i) {
    auto const& mode = static_cast<eMode>(i);
    auto const& size = pParams->mSetting[pParams->mAltMode[mode]].sensorSize;
    if (mode == eNORMAL_VIDEO || mode == eSLIM_VIDEO1 || mode == eSLIM_VIDEO2) {
      MY_LOGD("skip video related mode since it didn't have full capbility");
      continue;
    }
    if (pParsedAppImageInfo->maxImageSize.w <= size.w &&
        pParsedAppImageInfo->maxImageSize.h <= size.h) {
      (*res) = pParams->mSetting[pParams->mAltMode[mode]];
      hit = true;
      break;
    }
  }
  if (!hit) {
    // pick largest one
    MY_LOGW("select capture mode");
    (*res) = pParams->mSetting[eNORMAL_CAPTURE];
  }
  MINT32 forceSensorMode =
      ::property_get_int32("vendor.debug.cameng.force_sensormode", -1);
  if (forceSensorMode != -1) {
    switch (forceSensorMode) {
      case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        (*res) = pParams->mSetting[eNORMAL_PREVIEW];
        break;
      case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        (*res) = pParams->mSetting[eNORMAL_CAPTURE];
        break;
      case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        (*res) = pParams->mSetting[eNORMAL_VIDEO];
        break;
      case SENSOR_SCENARIO_ID_SLIM_VIDEO1:
        (*res) = pParams->mSetting[eSLIM_VIDEO1];
        break;
      case SENSOR_SCENARIO_ID_SLIM_VIDEO2:
        (*res) = pParams->mSetting[eSLIM_VIDEO2];
        break;
      default:
        MY_LOGW("Unknown sensorMode: %d", forceSensorMode);
        break;
    }
    MY_LOGD("Force set sensorMode: %d. Selected sensorMode: %d",
            forceSensorMode, res->sensorMode);
  }
  return OK;
}

static auto determineScenDefault(
    SensorSetting* res,
    std::shared_ptr<SensorParams> const& pParams,
    std::shared_ptr<ParsedAppImageStreamInfo> const& pParsedAppImageInfo,
    bool isVideo) -> int {
  bool hit = false;
  for (int i = 0; i < eNUM_SENSOR_MODE; ++i) {
    auto const& mode = static_cast<eMode>(i);
    auto const& size = pParams->mSetting[pParams->mAltMode[mode]].sensorSize;
    if (!isVideo && (mode == eNORMAL_VIDEO || mode == eSLIM_VIDEO1 ||
                     mode == eSLIM_VIDEO2)) {
      MY_LOGD("skip video related mode since it didn't have full capbility");
      continue;
    }
    if (pParsedAppImageInfo->maxImageSize.w <= size.w &&
        pParsedAppImageInfo->maxImageSize.h <= size.h) {
      (*res) = pParams->mSetting[pParams->mAltMode[mode]];
      hit = true;
      break;
    }
  }
  if (!hit) {
    // pick largest one
    MY_LOGW("select capture mode");
    (*res) = pParams->mSetting[eNORMAL_CAPTURE];
  }
  MINT32 forceSensorMode =
      ::property_get_int32("vendor.debug.cameng.force_sensormode", -1);
  if (forceSensorMode != -1) {
    switch (forceSensorMode) {
      case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        (*res) = pParams->mSetting[eNORMAL_PREVIEW];
        break;
      case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        (*res) = pParams->mSetting[eNORMAL_CAPTURE];
        break;
      case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        (*res) = pParams->mSetting[eNORMAL_VIDEO];
        break;
      case SENSOR_SCENARIO_ID_SLIM_VIDEO1:
        (*res) = pParams->mSetting[eSLIM_VIDEO1];
        break;
      case SENSOR_SCENARIO_ID_SLIM_VIDEO2:
        (*res) = pParams->mSetting[eSLIM_VIDEO2];
        break;
      default:
        MY_LOGW("Unknown sensorMode: %d", forceSensorMode);
        break;
    }
    MY_LOGD("Force set sensorMode: %d. Selected sensorMode: %d",
            forceSensorMode, res->sensorMode);
  }
  return OK;
}

/**
 * Make a function target - default version
 */
FunctionType_SensorSettingPolicy makePolicy_SensorSetting_Default() {
  return
      [](std::vector<SensorSetting>* pvOut,
         StreamingFeatureSetting const* pStreamingFeatureSetting,
         PipelineStaticInfo const* pPipelineStaticInfo,
         PipelineUserConfiguration const* pPipelineUserConfiguration) -> int {
        if (CC_UNLIKELY(!pvOut || !pStreamingFeatureSetting ||
                        !pPipelineStaticInfo || !pPipelineUserConfiguration)) {
          MY_LOGE("error input params");
          return -EINVAL;
        }

        auto const& pParsedAppCfg =
            pPipelineUserConfiguration->pParsedAppConfiguration;
        auto const& pParsedAppImageInfo =
            pPipelineUserConfiguration->pParsedAppImageStreamInfo;

        std::map<uint32_t, std::shared_ptr<SensorParams> > mStatic;
        for (auto const id : pPipelineStaticInfo->sensorIds) {
          SensorSetting res;  // output
          NSCamHW::HwInfoHelper infoHelper = NSCamHW::HwInfoHelper(id);
          infoHelper.updateInfos();
          NSCamHW::HwTransHelper tranHelper = NSCamHW::HwTransHelper(id);
          auto& pStatic = mStatic[id] = std::make_shared<SensorParams>();
          if (CC_UNLIKELY(pStatic == nullptr)) {
            MY_LOGE("initial sensor static fail");
            return -EINVAL;
          }

          CHECK_ERROR(parseSensorParamsSetting(&pStatic, &infoHelper));
          CHECK_ERROR(determineAlternativeMode(&pStatic, &tranHelper));
          // 2. has video consumer
          if (pParsedAppImageInfo->hasVideoConsumer) {
            if (pParsedAppCfg->operationMode == 1) {
              // 2-1. constrained high speed video
              res = pStatic->mSetting[eSLIM_VIDEO1];
            } else if (pParsedAppImageInfo->hasVideo4K) {
              // 2-2. 4K record
              res = pStatic->mSetting[eNORMAL_VIDEO];
            } else {
              // 2-3 others
              CHECK_ERROR(determineScenDefault(&res, pStatic,
                                               pParsedAppImageInfo, true));
            }
            // always happen, force skip determineScenDefault()...
            goto lbDone;
          }

          // 3. default rules policy:
          //    find the smallest size that is "larger" than max of stream size
          //    (not the smallest difference)
          CHECK_ERROR(determineScenDefault(&res, pStatic, pParsedAppImageInfo));
          if (pStreamingFeatureSetting->vhdrMode != SENSOR_VHDR_MODE_NONE) {
            CHECK_ERROR(checkVhdrSensor(
                &res, pStatic, pStreamingFeatureSetting->vhdrMode, infoHelper));
          }
        lbDone:
          MY_LOGD("select mode %d, size(%dx%d) @ %d vhdr mode(%d)",
                  res.sensorMode, res.sensorSize.w, res.sensorSize.h,
                  res.sensorFps, pStreamingFeatureSetting->vhdrMode);
          pvOut->push_back(res);
        }

        return OK;
      };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
