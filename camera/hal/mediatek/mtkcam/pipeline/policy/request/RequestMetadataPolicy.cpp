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

#define LOG_TAG "mtkcam-RequestMetadataPolicy"

#include "mtkcam/pipeline/policy/request/RequestMetadataPolicy.h"
//
#include "MyUtils.h"
#include <compiler.h>
#include <memory>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Time.h>

//
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
/******************************************************************************
 *
 ******************************************************************************/
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace requestmetadata {

/******************************************************************************
 *
 ******************************************************************************/
RequestMetadataPolicy_Default::RequestMetadataPolicy_Default(
    CreationParams const& params)
    : mPolicyParams(params) {
  for (size_t i = 0; i < params.pPipelineStaticInfo->sensorIds.size(); i++) {
    mvTargetRrzoSize.push_back(MSize(0, 0));
  }
}

/******************************************************************************
 *
 ******************************************************************************/
auto RequestMetadataPolicy_Default::evaluateRequest(
    EvaluateRequestParams const& params) -> int {
  // update request unique key
  if (params.pvAdditionalHal->size() > 0) {
    IMetadata::IEntry entry(MTK_PIPELINE_UNIQUE_KEY);
    IMetadata::IEntry entry1 =
        (*params.pvAdditionalHal)[0]->entryFor(MTK_PIPELINE_UNIQUE_KEY);
    if (entry1.isEmpty()) {
      uint32_t TS = NSCam::Utils::TimeTool::getReadableTime();
      entry.push_back(TS, Type2Type<MINT32>());
    } else {
      entry = entry1;
    }
    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      (*params.pvAdditionalHal)[i]->update(entry.tag(), entry);
    }
  }

  // update request id
  {
    IMetadata::IEntry entry(MTK_PIPELINE_REQUEST_NUMBER);
    entry.push_back(params.requestNo, Type2Type<MINT32>());
    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      (*params.pvAdditionalHal)[i]->update(entry.tag(), entry);
    }
  }

  // common metadata
  {
    MINT64 iMinFrmDuration = 0;
    for (auto const& it :
         params.pRequest_AppImageStreamInfo->vAppImage_Output_Proc) {
      StreamId_T const streamId = it.first;
      auto minFrameDuration =
          mPolicyParams.pPipelineUserConfiguration->vMinFrameDuration.find(
              streamId);
      if (CC_UNLIKELY(minFrameDuration->second < 0)) {
        MY_LOGE("Request App stream %#" PRIx64 "have not configured yet",
                streamId);
        continue;
      }
      iMinFrmDuration = (minFrameDuration->second > iMinFrmDuration)
                            ? minFrameDuration->second
                            : iMinFrmDuration;
    }
    MY_LOGD("The min frame duration is %" PRId64, iMinFrmDuration);
    IMetadata::IEntry entry(MTK_P1NODE_MIN_FRM_DURATION);
    entry.push_back(iMinFrmDuration, Type2Type<MINT64>());
    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      (*params.pvAdditionalHal)[i]->update(entry.tag(), entry);
    }
  }
  MUINT8 bRepeating = (MUINT8)params.pRequest_ParsedAppMetaControl->repeating;
  {
    IMetadata::IEntry entry(MTK_HAL_REQUEST_REPEAT);
    entry.push_back(bRepeating, Type2Type<MUINT8>());
    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      (*params.pvAdditionalHal)[i]->update(entry.tag(), entry);
    }
    MY_LOGD("Control AppMetadata is repeating(%d)", bRepeating);
  }
  {
    if (params.isZSLMode ||
        params.pRequest_AppImageStreamInfo->pAppImage_Jpeg != nullptr ||
        params.pRequest_AppImageStreamInfo->pAppImage_Output_Priv != nullptr) {
      MY_LOGD("set MTK_HAL_REQUEST_REQUIRE_EXIF = 1");
      IMetadata::IEntry entry2(MTK_HAL_REQUEST_REQUIRE_EXIF);
      entry2.push_back(1, Type2Type<MUINT8>());
      for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
        (*params.pvAdditionalHal)[i]->update(entry2.tag(), entry2);
      }
    }

    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      IMetadata::IEntry entry3(MTK_HAL_REQUEST_SENSOR_SIZE);
      entry3.push_back((*params.pSensorSize)[i], Type2Type<MSize>());
      (*params.pvAdditionalHal)[i]->update(entry3.tag(), entry3);
    }
  }
#if 1  // need check
  {
    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      if (!bRepeating || mvTargetRrzoSize[i].size() == 0) {
        IMetadata::IEntry Crop = (*params.pvAdditionalHal)[i]->entryFor(
            MTK_P1NODE_SENSOR_CROP_REGION);
        if (Crop.isEmpty()) {
          Crop = params.pRequest_AppControl->entryFor(MTK_SCALER_CROP_REGION);
          if (Crop.isEmpty()) {
            MY_LOGW("cannot get scaler crop region, index : %d", i);
            continue;
          }
        }
        MSize rrzoBufSize = params.RrzoSize[i];
        MRect cropRegion = Crop.itemAt(0, Type2Type<MRect>());
        if ((cropRegion.s.w * rrzoBufSize.h) >
            (cropRegion.s.h * rrzoBufSize.w)) {
          MINT32 temp = rrzoBufSize.h;
          rrzoBufSize.h =
              ALIGNX(rrzoBufSize.w * cropRegion.s.h / cropRegion.s.w, 4);
          if (rrzoBufSize.h > temp) {
            rrzoBufSize.h = temp;
          }
        } else {
          MINT32 temp = rrzoBufSize.w;
          rrzoBufSize.w =
              ALIGNX(rrzoBufSize.h * cropRegion.s.w / cropRegion.s.h, 4);
          if (rrzoBufSize.w > temp) {
            rrzoBufSize.w = temp;
          }
        }
        mvTargetRrzoSize[i].w = rrzoBufSize.w;
        mvTargetRrzoSize[i].h = rrzoBufSize.h;
      }
      IMetadata::IEntry Rrzotag(MTK_P1NODE_RESIZER_SET_SIZE);
      Rrzotag.push_back(mvTargetRrzoSize[i], Type2Type<MSize>());
      (*params.pvAdditionalHal)[i]->update(MTK_P1NODE_RESIZER_SET_SIZE,
                                           Rrzotag);
    }
  }
#endif
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto makePolicy_RequestMetadata_Default(CreationParams const& params)
    -> std::shared_ptr<IRequestMetadataPolicy> {
  return std::make_shared<RequestMetadataPolicy_Default>(params);
}

/******************************************************************************
 *
 ******************************************************************************/
RequestMetadataPolicy_DebugDump::RequestMetadataPolicy_DebugDump(
    CreationParams const& params)
    : mPolicyParams(params) {}

/******************************************************************************
 *
 ******************************************************************************/
auto RequestMetadataPolicy_DebugDump::evaluateRequest(
    EvaluateRequestParams const& params) -> int {
  if (CC_LIKELY(mPolicyParams.pRequestMetadataPolicy != nullptr)) {
    mPolicyParams.pRequestMetadataPolicy->evaluateRequest(params);
  }

  //
  int debugRawType = property_get_int32("vendor.debug.camera.raw.type", -1);
  if (debugRawType >= 0) {
    MY_LOGD(
        "set vendor.debug.camera.raw.type(%d) => MTK_P1NODE_RAW_TYPE(%d)  "
        "0:processed-raw 1:pure-raw",
        debugRawType, debugRawType);
    IMetadata::IEntry entry(MTK_P1NODE_RAW_TYPE);
    entry.push_back(debugRawType, Type2Type<int>());
    for (size_t i = 0; i < params.pvAdditionalHal->size(); ++i) {
      (*params.pvAdditionalHal)[i]->update(entry.tag(), entry);
    }
  }
  return OK;
}
/******************************************************************************
 *
 ******************************************************************************/
auto makePolicy_RequestMetadata_DebugDump(CreationParams const& params)
    -> std::shared_ptr<IRequestMetadataPolicy> {
  return std::make_shared<RequestMetadataPolicy_DebugDump>(params);
}
};  // namespace requestmetadata
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
