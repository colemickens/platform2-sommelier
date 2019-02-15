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

#include "P2_StreamingProcessor.h"

#define P2_CLASS_TAG Streaming_3DNR
#define P2_TRACE TRACE_STREAMING_3DNR
#include "P2_LogHeader.h"
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include <mtkcam/feature/eis/eis_ext.h>
#include "hal/inc/camera_custom_3dnr.h"

#include <memory>

// anonymous namespace
namespace {
static MINT32 getIso3DNR(const std::shared_ptr<P2::P2Request>& request) {
  const MINT32 DEFAULT_ISO = 100;
  MINT iso = request->mP2Pack.getSensorData().mISO;
  if (iso == INVALID_P1_ISO_VAL) {
    iso = DEFAULT_ISO;
  }
  return iso;
}
}  // namespace

namespace P2 {

MVOID StreamingProcessor::init3DNR() {
  m3dnrDebugLevel = property_get_int32("vendor.camera.3dnr.log.level", 0);

  for (MUINT32 sensorID : mP2Info.getConfigInfo().mAllSensorID) {
    mUtil3dnrMap[sensorID] = std::make_shared<Util3dnr>(sensorID);
    mUtil3dnrMap[sensorID]->init(
        mP2Info.getConfigInfo().mUsageHint.m3DNRMode &
        NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT);
  }

  MY_LOGD("usageHint.3DNRMode(0x%x)",
          mP2Info.getConfigInfo().mUsageHint.m3DNRMode);
}

MVOID StreamingProcessor::uninit3DNR() {
  mUtil3dnrMap.clear();
}

MBOOL StreamingProcessor::is3DNRFlowEnabled(MINT32 force3DNR,
                                            IMetadata* appInMeta,
                                            IMetadata* halInMeta,
                                            const ILog& log) const {
  MINT32 e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;
  MINT32 eHal3DnrMode = MTK_NR_FEATURE_3DNR_MODE_ON;

  if (appInMeta == NULL ||
      !tryGet<MINT32>(appInMeta, MTK_NR_FEATURE_3DNR_MODE, &e3DnrMode)) {
    MY_LOGD("no MTK_NR_FEATURE_3DNR_MODE: appInMeta: %p", appInMeta);
  }

  if (force3DNR) {
    char EnableOption[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("vendor.debug.camera.3dnr.enable", EnableOption, "1");
    if (EnableOption[0] == '1') {
      e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_ON;
      eHal3DnrMode = MTK_NR_FEATURE_3DNR_MODE_ON;
    } else if (EnableOption[0] == '0') {
      e3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;
      eHal3DnrMode = MTK_NR_FEATURE_3DNR_MODE_OFF;
    }
  }

  MY_LOGD("[3DNR] Meta App: %d, Hal(dual-cam): %d", e3DnrMode, eHal3DnrMode);

  return (e3DnrMode == MTK_NR_FEATURE_3DNR_MODE_ON ||
          eHal3DnrMode == MTK_NR_FEATURE_3DNR_MODE_ON);
}

MBOOL StreamingProcessor::getInputCrop3DNR(MBOOL* isEIS4K,
                                           MBOOL* isIMGO,
                                           MRect* inputCrop,
                                           P2Util::SimpleIn const& input,
                                           const ILog& log) const {
  MSize nr3dInputSize = input.getInputSize();

  *isEIS4K = MFALSE;

  *isIMGO = !(input.isResized());
  std::shared_ptr<Cropper> cropper = input.mRequest->getCropper();

  if (*isEIS4K) {
    MUINT32 cropFlag = Cropper::USE_EIS_12;
    cropFlag |= input.isResized() ? Cropper::USE_RESIZED : 0;
    MCropRect cropRect =
        cropper->calcViewAngle(log, cropper->getActiveCrop().s, cropFlag);
    inputCrop->p = cropRect.p_integral;
    inputCrop->s = cropRect.s;
  } else if (*isIMGO) {
    *inputCrop = cropper->getP1Crop();
  } else {
    inputCrop->p.x = 0;
    inputCrop->p.y = 0;
    inputCrop->s = nr3dInputSize;
  }

  MY_LOGD(
      "[3DNR] isEIS4K: %d, isIMGO: %d, input(%d,%d), inputCrop(%d,%d;%d,%d)",
      (*isEIS4K ? 1 : 0), (*isIMGO ? 1 : 0), nr3dInputSize.w, nr3dInputSize.h,
      inputCrop->p.x, inputCrop->p.y, inputCrop->s.w, inputCrop->s.h);

  return (*isEIS4K || *isIMGO);
}

MINT32 StreamingProcessor::get3DNRIsoThreshold(MUINT32 /*sensorID*/,
                                               MUINT8 ispProfile) const {
  MINT32 isoThreshold = NR3DCustom::get_3dnr_off_iso_threshold(
      ispProfile, mP2Info.getConfigInfo().mUsageHint.m3DNRMode &
                      NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT);
  MY_LOGD("Get isoThreshold : %d", isoThreshold);
  return isoThreshold;
}

MBOOL StreamingProcessor::prepare3DNR_FeatureData(MBOOL en3DNRFlow,
                                                  MBOOL isEIS4K,
                                                  MBOOL isIMGO,
                                                  P2Util::SimpleIn* input,
                                                  P2MetaSet const& /*metaSet*/,
                                                  MUINT8 ispProfile,
                                                  const ILog& log) const {
  TRACE_S_FUNC_ENTER(log);

  FeaturePipeParam& featureParam = input->mFeatureParam;
  std::shared_ptr<P2Request>& request = input->mRequest;
  const std::shared_ptr<Util3dnr>& util3dnr =
      mUtil3dnrMap.at(request->getSensorID());
  if (util3dnr == NULL) {
    MY_LOGW("No util3dnr!");
    return MFALSE;
  }
  std::shared_ptr<Cropper> cropper = request->getCropper();

  LMVInfo lmv = cropper->getLMVInfo();
  NR3DMVInfo mvInfo;
  mvInfo.status = lmv.is_valid ? NR3DMVInfo::VALID : NR3DMVInfo::INVALID;
  mvInfo.x_int = lmv.x_int;
  mvInfo.y_int = lmv.y_int;
  mvInfo.gmvX = lmv.gmvX;
  mvInfo.gmvY = lmv.gmvY;
  mvInfo.confX = lmv.confX;
  mvInfo.confY = lmv.confY;
  mvInfo.maxGMV = lmv.gmvMax;

  MINT32 iso = getIso3DNR(request);
  MINT32 isoThreshold = get3DNRIsoThreshold(request->getSensorID(), ispProfile);
  MBOOL canEnable3dnrOnThisFrame =
      util3dnr->canEnable3dnr(en3DNRFlow, iso, isoThreshold);
  MSize rrzoSize = cropper->getP1OutSize();
  MRect p1Crop = cropper->getP1Crop();

  util3dnr->modifyMVInfo(MTRUE, isIMGO, p1Crop, rrzoSize, &mvInfo);
  util3dnr->prepareFeatureData(canEnable3dnrOnThisFrame, mvInfo, iso,
                               isoThreshold, isEIS4K, &featureParam);
  util3dnr->prepareGyro(NULL, &featureParam);

  TRACE_S_FUNC_EXIT(log);
  return canEnable3dnrOnThisFrame;
}

MBOOL StreamingProcessor::prepare3DNR(P2Util::SimpleIn* input,
                                      const ILog& log) const {
  TRACE_S_FUNC_ENTER(log);

  MBOOL en3DNRFlow = MFALSE;
  MBOOL forceSupport = ((mP2Info.getConfigInfo().mUsageHint.m3DNRMode &
                         NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) != 0);
  MBOOL uiSupport = ((mP2Info.getConfigInfo().mUsageHint.m3DNRMode &
                      NSCam::NR3D::E3DNR_MODE_MASK_UI_SUPPORT) != 0);
  if (uiSupport || forceSupport) {
    P2MetaSet metaSet = input->mRequest->getMetaSet();
    en3DNRFlow =
        is3DNRFlowEnabled(forceSupport, &metaSet.mInApp, &metaSet.mInHal, log);
    if (en3DNRFlow) {
      FeaturePipeParam& featureParam = input->mFeatureParam;
      featureParam.setFeatureMask(NSCam::NSCamFeature::NSFeaturePipe::MASK_3DNR,
                                  en3DNRFlow);
      featureParam.setFeatureMask(
          NSCam::NSCamFeature::NSFeaturePipe::MASK_3DNR_RSC,
          (mP2Info.getConfigInfo().mUsageHint.m3DNRMode &
           NSCam::NR3D::E3DNR_MODE_MASK_RSC_EN));
    }

    MBOOL isEIS4K = MFALSE, isIMGO = MFALSE;
    MRect inputCrop;
    MUINT8 ispProfile = 0;
    if (!tryGet<MUINT8>(&metaSet.mInHal, MTK_3A_ISP_PROFILE, &ispProfile)) {
      MY_LOGD("no ISPProfile from HalMeta");
    }

    prepare3DNR_FeatureData(en3DNRFlow, isEIS4K, isIMGO, input, metaSet,
                            ispProfile, log);
  }

  MY_LOGD("[3DNR] en3DNRFlow: %d, forceSupport: %d", (en3DNRFlow ? 1 : 0),
          (forceSupport ? 1 : 0));

  TRACE_S_FUNC_EXIT(log);
  return en3DNRFlow;
}

};  // namespace P2
