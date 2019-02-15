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

#define LOG_TAG "mtkcam-AppRequestParser"

#include <impl/AppRequestParser.h>
#include <memory>
#include <unordered_map>

#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 ******************************************************************************/
static auto categorizeImgStream(
    ParsedAppRequest* out,
    std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamBuffer>> const&
        inMap,
    bool isInput) -> int {
  auto const& pParsedAppImageStreamBuffers = out->pParsedAppImageStreamBuffers;
  auto const& pParsedAppImageStreamInfo = out->pParsedAppImageStreamInfo;
  MSize maxStreamSize = MSize(0, 0);
  for (auto const& it : inMap) {
    auto const& pStreamBuffer = it.second;
    auto const& pStreamInfo = pStreamBuffer->getStreamInfo();
    if (CC_LIKELY(pStreamInfo != nullptr)) {
      switch (pStreamInfo->getImgFormat()) {
        case eImgFmt_CAMERA_OPAQUE:
          if (isInput) {
            pParsedAppImageStreamBuffers->pAppImage_Input_Priv = pStreamBuffer;
            pParsedAppImageStreamInfo->pAppImage_Input_Priv = pStreamInfo;
          } else {
            pParsedAppImageStreamBuffers->pAppImage_Output_Priv = pStreamBuffer;
            pParsedAppImageStreamInfo->pAppImage_Output_Priv = pStreamInfo;
          }
          break;
          //
        case eImgFmt_BLOB:  // AS-IS: should be removed in the future
        case eImgFmt_JPEG:  // TO-BE: Jpeg Capture
          pParsedAppImageStreamBuffers->pAppImage_Jpeg = pStreamBuffer;
          pParsedAppImageStreamInfo->pAppImage_Jpeg = pStreamInfo;
          if (pStreamInfo->getImgSize().size() > maxStreamSize.size()) {
            maxStreamSize = pStreamInfo->getImgSize();
          }
          break;
          //
        case eImgFmt_YV12:
        case eImgFmt_NV12:
        case eImgFmt_NV21:
        case eImgFmt_YUY2:
        case eImgFmt_Y8:
        case eImgFmt_Y16:
          if (isInput) {
            pParsedAppImageStreamBuffers->pAppImage_Input_Yuv = pStreamBuffer;
            pParsedAppImageStreamInfo->pAppImage_Input_Yuv = pStreamInfo;
          } else {
            pParsedAppImageStreamBuffers->vAppImage_Output_Proc.emplace(
                it.first, pStreamBuffer);
            pParsedAppImageStreamInfo->vAppImage_Output_Proc.emplace(
                it.first, pStreamInfo);
            if (!pParsedAppImageStreamInfo->hasVideoConsumer &&
                (pStreamInfo->getUsageForConsumer() &
                 GRALLOC_USAGE_HW_VIDEO_ENCODER)) {
              pParsedAppImageStreamInfo->hasVideoConsumer = true;
              pParsedAppImageStreamInfo->videoImageSize =
                  pStreamInfo->getImgSize();
              pParsedAppImageStreamInfo->hasVideo4K =
                  (pParsedAppImageStreamInfo->videoImageSize.w *
                       pParsedAppImageStreamInfo->videoImageSize.h >
                   8000000)
                      ? true
                      : false;
            }
            if (pStreamInfo->getImgSize().size() > maxStreamSize.size()) {
              maxStreamSize = pStreamInfo->getImgSize();
            }
          }
          break;
          //
        default:
          MY_LOGE("Unsupported format:0x%x", pStreamInfo->getImgFormat());
          break;
      }
    }
  }
  pParsedAppImageStreamInfo->maxImageSize = maxStreamSize;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto parseAppRequest(ParsedAppRequest* out, UserRequestParams const* in)
    -> int {
  if (CC_UNLIKELY(!out || !in)) {
    MY_LOGE("bad arguments - out:%p in:%p", out, in);
    return -EINVAL;
  }

  out->requestNo = in->requestNo;
  out->pAppMetaControlStreamBuffer = in->IMetaBuffers.begin()->second;
  out->pParsedAppImageStreamBuffers =
      std::make_shared<ParsedAppImageStreamBuffers>();
  out->pParsedAppImageStreamInfo = std::make_shared<ParsedAppImageStreamInfo>();
  out->pParsedAppMetaControl = std::make_shared<policy::ParsedMetaControl>();
  if (CC_UNLIKELY(!out->pParsedAppImageStreamBuffers ||
                  !out->pParsedAppImageStreamInfo ||
                  !out->pParsedAppMetaControl)) {
    MY_LOGE(
        "Fail on make_shared - pParsedAppImageStreamBuffers:%p "
        "pParsedAppImageStreamInfo:%p pParsedAppMetaControl:%p",
        out->pParsedAppImageStreamBuffers.get(),
        out->pParsedAppImageStreamInfo.get(), out->pParsedAppMetaControl.get());
    return -EINVAL;
  }

  ////////////////////////////////////////////////////////////////////////////
  //  pParsedAppImageStreamBuffers
  //  pParsedAppImageStreamInfo

  categorizeImgStream(out, in->IImageBuffers, true);
  categorizeImgStream(out, in->OImageBuffers, false);

  ////////////////////////////////////////////////////////////////////////////
  //  pParsedAppMetaControl

  auto const& pAppControl = out->pAppMetaControlStreamBuffer;
  IMetadata* pMetadata = pAppControl->tryReadLock(LOG_TAG);
  if (CC_UNLIKELY(!pMetadata)) {
    MY_LOGE("bad metadata(%p) SBuffer(%p)", pMetadata, pAppControl.get());
    pAppControl->unlock(LOG_TAG, pMetadata);
    return -EINVAL;
  }

  auto const& pParsedCtrl = out->pParsedAppMetaControl;
  pParsedCtrl->repeating = pAppControl->isRepeating();

  {
    IMetadata::IEntry const entry =
        pMetadata->entryFor(MTK_CONTROL_AE_TARGET_FPS_RANGE);
    if (entry.count() == 2) {
      pParsedCtrl->control_aeTargetFpsRange[0] =
          entry.itemAt(0, Type2Type<int32_t>());
      pParsedCtrl->control_aeTargetFpsRange[1] =
          entry.itemAt(1, Type2Type<int32_t>());
    }
  }

#define PARSE_META_CONTROL(_tag_, _value_)                       \
  do {                                                           \
    IMetadata::IEntry const entry = pMetadata->entryFor(_tag_);  \
    if (!entry.isEmpty())                                        \
      _value_ = entry.itemAt(0, Type2Type<decltype(_value_)>()); \
  } while (0)

  PARSE_META_CONTROL(MTK_CONTROL_CAPTURE_INTENT,
                     pParsedCtrl->control_captureIntent);
  PARSE_META_CONTROL(MTK_CONTROL_ENABLE_ZSL, pParsedCtrl->control_enableZsl);
  PARSE_META_CONTROL(MTK_CONTROL_MODE, pParsedCtrl->control_mode);
  PARSE_META_CONTROL(MTK_CONTROL_SCENE_MODE, pParsedCtrl->control_sceneMode);
  PARSE_META_CONTROL(MTK_CONTROL_VIDEO_STABILIZATION_MODE,
                     pParsedCtrl->control_videoStabilizationMode);
#undef PARSE_META_CONTROL

  pAppControl->unlock(LOG_TAG, pMetadata);

  ////////////////////////////////////////////////////////////////////////////

  return OK;
}
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
