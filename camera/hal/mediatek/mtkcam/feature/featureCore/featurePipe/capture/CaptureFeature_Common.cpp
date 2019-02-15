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

#include <common/include/DebugUtil.h>

#include <capture/CaptureFeature_Common.h>
#include "CaptureFeatureRequest.h"

#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>

#include "DebugControl.h"
#define PIPE_CLASS_TAG "Util"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_COMMON
#include <algorithm>
#include <cmath>
#include <common/include/PipeLog.h>
#include <cstdlib>

#include <isp_tuning/isp_tuning.h>
#include <memory>
#include <mtkcam/drv/IHalSensor.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

MBOOL copyImageBuffer(IImageBuffer* src, IImageBuffer* dst) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  if (!src || !dst) {
    MY_LOGE("Invalid buffers src=%p dst=%p", src, dst);
    ret = MFALSE;
  } else if (src->getImgSize() != dst->getImgSize()) {
    MY_LOGE("Mismatch buffer size src(%dx%d) dst(%dx%d)", src->getImgSize().w,
            src->getImgSize().h, dst->getImgSize().w, dst->getImgSize().h);
    ret = MFALSE;
  } else {
    unsigned srcPlane = src->getPlaneCount();
    unsigned dstPlane = dst->getPlaneCount();

    if (!srcPlane || !dstPlane ||
        (srcPlane != dstPlane && srcPlane != 1 && dstPlane != 1)) {
      MY_LOGE("Mismatch buffer plane src(%d) dst(%d)", srcPlane, dstPlane);
      ret = MFALSE;
    }
    for (unsigned i = 0; i < srcPlane; ++i) {
      if (!src->getBufVA(i)) {
        MY_LOGE("Invalid src plane[%d] VA", i);
        ret = MFALSE;
      }
    }
    for (unsigned i = 0; i < dstPlane; ++i) {
      if (!dst->getBufVA(i)) {
        MY_LOGE("Invalid dst plane[%d] VA", i);
        ret = MFALSE;
      }
    }

    if (srcPlane == 1) {
      MY_LOGD("src: plane=1 size=%zu stride=%zu", src->getBufSizeInBytes(0),
              src->getBufStridesInBytes(0));
      ret = MFALSE;
    }
    if (dstPlane == 1) {
      MY_LOGD("dst: plane=1 size=%zu stride=%zu", dst->getBufSizeInBytes(0),
              dst->getBufStridesInBytes(0));
      ret = MFALSE;
    }

    if (ret) {
      char *srcVA = NULL, *dstVA = NULL;
      size_t srcSize = 0;
      size_t dstSize = 0;
      size_t srcStride = 0;
      size_t dstStride = 0;

      for (unsigned i = 0; i < srcPlane && i < dstPlane; ++i) {
        if (i < srcPlane) {
          srcVA = reinterpret_cast<char*>(src->getBufVA(i));
        }
        if (i < dstPlane) {
          dstVA = reinterpret_cast<char*>(dst->getBufVA(i));
        }

        srcSize = src->getBufSizeInBytes(i);
        dstSize = dst->getBufSizeInBytes(i);
        srcStride = src->getBufStridesInBytes(i);
        dstStride = dst->getBufStridesInBytes(i);
        MY_LOGD("plane[%d] memcpy %p(%zu)=>%p(%zu)", i, srcVA, srcSize, dstVA,
                dstSize);
        if (srcStride == dstStride) {
          memcpy(reinterpret_cast<void*>(dstVA), reinterpret_cast<void*>(srcVA),
                 (srcSize <= dstSize) ? srcSize : dstSize);
        } else {
          MY_LOGD("Stride: src(%zu) dst(%zu)", srcStride, dstStride);
          size_t stride = (srcStride < dstStride) ? srcStride : dstStride;
          unsigned height = dstSize / dstStride;
          for (unsigned j = 0; j < height; ++j) {
            memcpy(reinterpret_cast<void*>(dstVA),
                   reinterpret_cast<void*>(srcVA), stride);
            srcVA += srcStride;
            dstVA += dstStride;
          }
        }
      }
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL dumpToFile(IImageBuffer* buffer, const char* fmt, ...) {
  MBOOL ret = MFALSE;
  if (buffer && fmt) {
    char filename[256];
    va_list arg;
    va_start(arg, fmt);
    vsprintf(filename, fmt, arg);
    va_end(arg);
    buffer->saveToFile(filename);
    ret = MTRUE;
  }
  return ret;
}

CropCalculator::CropCalculator(MUINT uSensorIndex, MUINT32 uLogLevel)
    : mLogLevel(uLogLevel), mHwTransHelper(uSensorIndex) {
  std::shared_ptr<IMetadataProvider> pMetadataProvider =
      NSMetadataProviderManager::valueFor(uSensorIndex);
  if (pMetadataProvider != NULL) {
    const IMetadata& mrStaticMetadata =
        pMetadataProvider->getMtkStaticCharacteristics();

    if (tryGetMetadata<MRect>(&mrStaticMetadata,
                              MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION,
                              &mActiveArray)) {
      MY_LOGD("Active Array(%d,%d)(%dx%d)", mActiveArray.p.x, mActiveArray.p.y,
              mActiveArray.s.w, mActiveArray.s.h);
    } else {
      MY_LOGE("no static info: MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION");
    }
  } else {
    MY_LOGD("no metadata provider, senser:%d", uSensorIndex);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<CropCalculator::Factor> CropCalculator::getFactor(
    const IMetadata* pInAppMeta, const IMetadata* pInHalMeta) {
  std::shared_ptr<Factor> pFactor = std::make_shared<Factor>();
  Factor& factor = *pFactor.get();

  if (!tryGetMetadata<MSize>(pInHalMeta, MTK_HAL_REQUEST_SENSOR_SIZE,
                             &(factor.mSensorSize))) {
    MY_LOGE("cannot get MTK_HAL_REQUEST_SENSOR_SIZE");
    return NULL;
  }

  const MSize& sensor = factor.mSensorSize;

  // 1. Get current p1 buffer crop status
  if (!(tryGetMetadata<MRect>(pInHalMeta, MTK_P1NODE_SCALAR_CROP_REGION,
                              &(factor.mP1SensorCrop)) &&
        tryGetMetadata<MSize>(pInHalMeta, MTK_P1NODE_RESIZER_SIZE,
                              &(factor.mP1ResizerSize)) &&
        tryGetMetadata<MRect>(pInHalMeta, MTK_P1NODE_DMA_CROP_REGION,
                              &(factor.mP1DmaCrop)))) {
    MY_LOGW_IF(mLogLevel, "[FIXME] should sync with p1 for factor setting");
    factor.mP1SensorCrop = MRect(MPoint(0, 0), sensor);
    factor.mP1ResizerSize = sensor;
    factor.mP1DmaCrop = MRect(MPoint(0, 0), sensor);
  }

  MINT32 mSensorMode;
  if (!tryGetMetadata<MINT32>(pInHalMeta, MTK_P1NODE_SENSOR_MODE,
                              &mSensorMode)) {
    MY_LOGE("cannot get MTK_P1NODE_SENSOR_MODE");
    return NULL;
  }

  // 2. Transform Matrix
  if (!mHwTransHelper.getMatrixToActive(mSensorMode, &factor.mSensor2Active) ||
      !mHwTransHelper.getMatrixFromActive(mSensorMode,
                                          &factor.mActive2Sensor)) {
    MY_LOGE("fail to get HW transform matrix!");
    return NULL;
  }

  factor.mSensor2Resizer = simpleTransform(
      factor.mP1SensorCrop.p, factor.mP1SensorCrop.s, factor.mP1ResizerSize);

  // 3. Query crop region (in active array domain)
  MRect& cropRegion = factor.mActiveCrop;
  {
    if (!tryGetMetadata<MRect>(pInAppMeta, MTK_SCALER_CROP_REGION,
                               &(cropRegion))) {
      cropRegion.p = MPoint(0, 0);
      cropRegion.s = mActiveArray.s;
      MY_LOGW("no MTK_SCALER_CROP_REGION: using full crop size %dx%d",
              cropRegion.s.w, cropRegion.s.h);
    }
  }

  MY_LOGD(
      "Active:(%d,%d)(%dx%d) Sensor:(%d,%d)(%dx%d) Resizer:(%dx%d) "
      "DMA:(%d,%d)(%dx%d)",
      factor.mActiveCrop.p.x, factor.mActiveCrop.p.y, factor.mActiveCrop.s.w,
      factor.mActiveCrop.s.h, factor.mP1SensorCrop.p.x,
      factor.mP1SensorCrop.p.y, factor.mP1SensorCrop.s.w,
      factor.mP1SensorCrop.s.h, factor.mP1ResizerSize.w,
      factor.mP1ResizerSize.h, factor.mP1DmaCrop.p.x, factor.mP1DmaCrop.p.y,
      factor.mP1DmaCrop.s.w, factor.mP1DmaCrop.s.h);

  return pFactor;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID CropCalculator::evaluate(MSize const& srcSize,
                               MSize const& dstSize,
                               MRect* srcCrop) {
  // pillarbox
  if (srcSize.w * dstSize.h > srcSize.h * dstSize.w) {
    srcCrop->s.w = div_round(srcSize.h * dstSize.w, dstSize.h);
    srcCrop->s.h = srcSize.h;
    srcCrop->p.x = ((srcSize.w - srcCrop->s.w) >> 1);
    srcCrop->p.y = 0;
  } else {  // letterbox
    srcCrop->s.w = srcSize.w;
    srcCrop->s.h = div_round(srcSize.w * dstSize.h, dstSize.w);
    srcCrop->p.x = 0;
    srcCrop->p.y = ((srcSize.h - srcCrop->s.h) >> 1);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID CropCalculator::evaluate(std::shared_ptr<Factor> pFactor,
                               MSize const& dstSize,
                               MRect* srcCrop,
                               MBOOL const bResized) const {
  Factor& factor = *pFactor.get();
  // coordinates: s_: sensor
  MRect s_crop;
  factor.mActive2Sensor.transform(factor.mActiveCrop, &s_crop);
#define FOV_DIFF_TOLERANCE (3)
  MRect s_viewcrop;
  // pillarbox
  if (s_crop.s.w * dstSize.h > s_crop.s.h * dstSize.w) {
    s_viewcrop.s.w = div_round(s_crop.s.h * dstSize.w, dstSize.h);
    s_viewcrop.s.h = s_crop.s.h;
    s_viewcrop.p.x = s_crop.p.x + ((s_crop.s.w - s_viewcrop.s.w) >> 1);
    if (s_viewcrop.p.x < 0 && std::abs(s_viewcrop.p.x) < FOV_DIFF_TOLERANCE) {
      s_viewcrop.p.x = 0;
    }
    s_viewcrop.p.y = s_crop.p.y;
  } else {  // letterbox
    s_viewcrop.s.w = s_crop.s.w;
    s_viewcrop.s.h = div_round(s_crop.s.w * dstSize.h, dstSize.w);
    s_viewcrop.p.x = s_crop.p.x;
    s_viewcrop.p.y = s_crop.p.y + ((s_crop.s.h - s_viewcrop.s.h) >> 1);
    if (s_viewcrop.p.y < 0 && std::abs(s_viewcrop.p.y) < FOV_DIFF_TOLERANCE) {
      s_viewcrop.p.y = 0;
    }
  }
  MY_LOGD_IF(mLogLevel > 1,
             "s_cropRegion(%d, %d, %dx%d), dst %dx%d, view crop(%d, %d, %dx%d)",
             s_crop.p.x, s_crop.p.y, s_crop.s.w, s_crop.s.h, dstSize.w,
             dstSize.h, s_viewcrop.p.x, s_viewcrop.p.y, s_viewcrop.s.w,
             s_viewcrop.s.h);
#undef FOV_DIFF_TOLERANCE

  // according to sensor mode adjust crop region
  float fov_diff_x = 0.0f;
  float fov_diff_y = 0.0f;
  mHwTransHelper.calculateFovDifference(factor.mSensorMode, &fov_diff_x,
                                        &fov_diff_y);
  //
  float ratio_s = static_cast<float>(factor.mP1SensorCrop.s.w) /
                  static_cast<float>(factor.mP1SensorCrop.s.h);
  float ratio_d =
      static_cast<float>(s_viewcrop.s.w) / static_cast<float>(s_viewcrop.s.h);
  MY_LOGD_IF(mLogLevel > 1, "ratio_s:%f ratio_d:%f", ratio_s, ratio_d);
  /*
   * handle HAL3 sensor mode 16:9 FOV
   */
  //
  if ((s_viewcrop.p.x < 0 || s_viewcrop.p.y < 0) &&
      std::abs(ratio_s - ratio_d) < 0.1f) {
    MRect refined = s_viewcrop;
    float ratio = static_cast<float>(factor.mP1SensorCrop.s.h) /
                  static_cast<float>(s_viewcrop.s.h);
    refined.s.w = s_viewcrop.s.w * ratio;
    refined.s.h = s_viewcrop.s.h * ratio;
    refined.p.x = s_viewcrop.p.x + (s_viewcrop.s.w - refined.s.w) / 2.0f;
    refined.p.y = s_viewcrop.p.y + (s_viewcrop.s.h - refined.s.h) / 2.0f;
    //
    s_viewcrop = refined;
    MY_LOGD_IF(mLogLevel > 1,
               "refine negative crop ratio %f r.s(%dx%d) r.p(%d, %d)", ratio,
               refined.s.w, refined.s.h, refined.p.x, refined.p.y);
  } else if (fov_diff_y < 1 && fov_diff_y > fov_diff_x &&
             (s_viewcrop.s.w * factor.mP1SensorCrop.s.h <
              s_viewcrop.s.h * factor.mP1SensorCrop.s.w)) {
    MRect refined = s_viewcrop;
    float dY = 1.0f - fov_diff_y;
    refined.s.w = s_viewcrop.s.w * dY;
    refined.s.h = s_viewcrop.s.h * dY;
    refined.p.x = s_viewcrop.p.x + (s_viewcrop.s.w - refined.s.w) / 2.0f;
    refined.p.y = s_viewcrop.p.y + (s_viewcrop.s.h - refined.s.h) / 2.0f;
    //
    s_viewcrop = refined;
    float dX = 1.0f - fov_diff_x;
    MY_LOGD_IF(mLogLevel > 1, "dX %f dY %f r.s(%dx%d) r.p(%d, %d)", dX, dY,
               refined.s.w, refined.s.h, refined.p.x, refined.p.y);
  }
  //
  MY_LOGD_IF(mLogLevel > 1, "p1 sensor crop(%d, %d,%dx%d), %d, %d",
             factor.mP1SensorCrop.p.x, factor.mP1SensorCrop.p.y,
             factor.mP1SensorCrop.s.w, factor.mP1SensorCrop.s.h,
             s_viewcrop.s.w * factor.mP1SensorCrop.s.h,
             s_viewcrop.s.h * factor.mP1SensorCrop.s.w);
  if (bResized) {
    MRect r_viewcrop = transform(factor.mSensor2Resizer, s_viewcrop);
    srcCrop->s = r_viewcrop.s;
    srcCrop->p = r_viewcrop.p;

    // make sure hw limitation
    srcCrop->s.w &= ~(0x1);
    srcCrop->s.h &= ~(0x1);

    // check boundary
    if (refineBoundary(factor.mP1ResizerSize, srcCrop)) {
      MY_LOGW_IF(mLogLevel, "[FIXME] need to check crop!");
      factor.dump();
    }
  } else {
    srcCrop->s = s_viewcrop.s;
    srcCrop->p = s_viewcrop.p;

    // make sure hw limitation
    srcCrop->s.w &= ~(0x1);
    srcCrop->s.h &= ~(0x1);

    // check boundary
    if (refineBoundary(factor.mSensorSize, srcCrop)) {
      MY_LOGW_IF(mLogLevel, "[FIXME] need to check crop!");
      factor.dump();
    }
  }

  MY_LOGD_IF(mLogLevel > 1, "resized %d, crop (%d.%d)(%dx%d)", bResized,
             srcCrop->p.x, srcCrop->p.y, srcCrop->s.w, srcCrop->s.h);
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL CropCalculator::refineBoundary(MSize const& bufSize, MRect* crop) const {
  // tolerance
  if (crop->p.x == -1) {
    crop->p.x = 0;
  }

  if (crop->p.y == -1) {
    crop->p.y = 0;
  }

  MBOOL isRefined = MFALSE;
  MRect refined = *crop;
  if (crop->p.x < 0) {
    refined.p.x = 0;
    isRefined = MTRUE;
  }
  if (crop->p.y < 0) {
    refined.p.y = 0;
    isRefined = MTRUE;
  }

  if ((refined.p.x + crop->s.w) > bufSize.w) {
    refined.s.w = bufSize.w - refined.p.x;
    isRefined = MTRUE;
  }
  if ((refined.p.y + crop->s.h) > bufSize.h) {
    refined.s.h = bufSize.h - refined.p.y;
    isRefined = MTRUE;
  }

  if (isRefined) {
    // make sure hw limitation
    refined.s.w &= ~(0x1);
    refined.s.h &= ~(0x1);

    MY_LOGW_IF(
        mLogLevel,
        "buffer size:%dx%d, crop(%d,%d)(%dx%d) -> refined crop(%d,%d)(%dx%d)",
        bufSize.w, bufSize.h, crop->p.x, crop->p.y, crop->s.w, crop->s.h,
        refined.p.x, refined.p.y, refined.s.w, refined.s.h);
    *crop = refined;
  }
  return isRefined;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID CropCalculator::Factor::dump() const {
  MY_LOGD(
      "p1 sensro crop(%d,%d,%dx%d), resizer size(%dx%d), crop dma(%d,%d,%dx%d)",
      mP1SensorCrop.p.x, mP1SensorCrop.p.y, mP1SensorCrop.s.w,
      mP1SensorCrop.s.h, mP1ResizerSize.w, mP1ResizerSize.h, mP1DmaCrop.p.x,
      mP1DmaCrop.p.y, mP1DmaCrop.s.w, mP1DmaCrop.s.h);

  mActive2Sensor.dump("tran active to sensor");

  MY_LOGD("tran sensor to resized o %d, %d, s %dx%d -> %dx%d",
          mSensor2Resizer.tarOrigin.x, mSensor2Resizer.tarOrigin.y,
          mSensor2Resizer.oldScale.w, mSensor2Resizer.oldScale.h,
          mSensor2Resizer.newScale.w, mSensor2Resizer.newScale.h);
  MY_LOGD("modified active crop %d, %d, %dx%d", mActiveCrop.p.x,
          mActiveCrop.p.y, mActiveCrop.s.w, mActiveCrop.s.h);
}

const char* PathID2Name(PathID_T pid) {
  switch (pid) {
    case PID_ENQUE:
      return "enque";
    case PID_ROOT_TO_RAW:
      return "root_to_raw";
    case PID_ROOT_TO_P2A:
      return "root_to_p2a";
    case PID_ROOT_TO_MULTIFRAME:
      return "root_to_multiframe";
    case PID_RAW_TO_P2A:
      return "raw_to_p2a";
    case PID_P2A_TO_DEPTH:
      return "p2a_to_depth";
    case PID_P2A_TO_FUSION:
      return "p2a_to_fusion";
    case PID_P2A_TO_MULTIFRAME:
      return "p2a_to_multiframe";
    case PID_P2A_TO_YUV:
      return "p2a_to_yuv";
    case PID_P2A_TO_YUV2:
      return "p2a_to_yuv2";
    case PID_P2A_TO_MDP:
      return "p2a_to_mdp";
    case PID_P2A_TO_FD:
      return "p2a_to_fd";
    case PID_FD_TO_DEPTH:
      return "fd_to_depth";
    case PID_FD_TO_FUSION:
      return "fd_to_fusion";
    case PID_FD_TO_MULTIFRAME:
      return "fd_to_multiframe";
    case PID_FD_TO_YUV:
      return "fd_to_yuv";
    case PID_FD_TO_YUV2:
      return "fd_to_yuv2";
    case PID_MULTIFRAME_TO_YUV:
      return "multiframe_to_yuv";
    case PID_MULTIFRAME_TO_YUV2:
      return "multiframe_to_yuv2";
    case PID_MULTIFRAME_TO_BOKEH:
      return "multiframe_to_bokeh";
    case PID_MULTIFRAME_TO_MDP:
      return "multiframe_to_mdp";
    case PID_FUSION_TO_YUV:
      return "fusion_to_yuv";
    case PID_FUSION_TO_MDP:
      return "fusion_to_mdp";
    case PID_DEPTH_TO_BOKEH:
      return "depth_to_bokeh";
    case PID_YUV_TO_BOKEH:
      return "yuv_to_bokeh";
    case PID_YUV_TO_YUV2:
      return "yuv_to_yuv2";
    case PID_YUV_TO_MDP:
      return "yuv_to_mdp";
    case PID_BOKEH_TO_YUV2:
      return "bokeh_to_yuv2";
    case PID_BOKEH_TO_MDP:
      return "bokeh_to_mdp";
    case PID_YUV2_TO_MDP:
      return "yuv2_to_mdp";
    case PID_DEQUE:
      return "deque";

    default:
      return "unknown";
  }
}

const char* NodeID2Name(NodeID_T nid) {
  switch (nid) {
    case NID_ROOT:
      return "root";
    case NID_RAW:
      return "raw";
    case NID_P2A:
      return "p2a";
    case NID_FD:
      return "fd";
    case NID_MULTIFRAME:
      return "multiframe";
    case NID_FUSION:
      return "fusion";
    case NID_DEPTH:
      return "depth";
    case NID_YUV:
      return "yuv";
    case NID_YUV_R1:
      return "yuv_r1";
    case NID_YUV_R2:
      return "yuv_r2";
    case NID_YUV2:
      return "yuv2";
    case NID_YUV2_R1:
      return "yuv2_r1";
    case NID_YUV2_R2:
      return "yuv2_r2";
    case NID_BOKEH:
      return "bokeh";
    case NID_MDP:
      return "mdp";

    default:
      return "unknown";
  }
}

const char* TypeID2Name(TypeID_T tid) {
  switch (tid) {
    case TID_MAN_FULL_RAW:
      return "man_full_raw";
    case TID_MAN_FULL_YUV:
      return "man_full_yuv";
    case TID_MAN_RSZ_RAW:
      return "man_rsz_raw";
    case TID_MAN_RSZ_YUV:
      return "man_rsz_yuv";
    case TID_MAN_CROP1_YUV:
      return "man_crop1_yuv";
    case TID_MAN_CROP2_YUV:
      return "man_crop2_yuv";
    case TID_MAN_SPEC_YUV:
      return "man_spec_yuv";
    case TID_MAN_DEPTH:
      return "man_depth";
    case TID_MAN_LCS:
      return "man_lcs";
    case TID_MAN_FD_YUV:
      return "man_fd_yuv";
    case TID_MAN_FD:
      return "man_fd";
    case TID_SUB_FULL_RAW:
      return "sub_full_raw";
    case TID_SUB_FULL_YUV:
      return "sub_full_yuv";
    case TID_SUB_RSZ_RAW:
      return "sub_rsz_raw";
    case TID_SUB_RSZ_YUV:
      return "sub_rsz_yuv";
    case TID_SUB_LCS:
      return "sub_lcs";
    case TID_POSTVIEW:
      return "postview";
    case TID_JPEG:
      return "jpeg";
    case TID_THUMBNAIL:
      return "thumbnail";
    case NULL_TYPE:
      return "";

    default:
      return "unknown";
  }
}

const char* FeatID2Name(FeatureID_T fid) {
  switch (fid) {
    case FID_REMOSAIC:
      return "remosaic";
    case FID_NR:
      return "nr";
    case FID_ABF:
      return "abf";
    case FID_HDR:
      return "hdr";
    case FID_MFNR:
      return "mfnr";
    case FID_FB:
      return "fb";
    case FID_BOKEH:
      return "bokeh";
    case FID_DEPTH:
      return "depth";
    case FID_FUSION:
      return "fusion";
    case FID_CZ:
      return "cz";
    case FID_DRE:
      return "dre";
    case FID_FB_3RD_PARTY:
      return "fb_3rd_party";
    case FID_HDR_3RD_PARTY:
      return "hdr_3rd_party";
    case FID_HDR2_3RD_PARTY:
      return "hdr2_3rd_party";
    case FID_MFNR_3RD_PARTY:
      return "mfnr_3rd_party";
    case FID_BOKEH_3RD_PARTY:
      return "bokeh_3rd_party";
    case FID_DEPTH_3RD_PARTY:
      return "depth_3rd_party";
    case FID_FUSION_3RD_PARTY:
      return "fusion_3rd_party";

    default:
      return "unknown";
  }
}

const char* SizeID2Name(FeatureID_T fid) {
  switch (fid) {
    case SID_FULL:
      return "full";
    case SID_RESIZED:
      return "resized";
    case SID_BINNING:
      return "binning";
    case SID_ARBITRARY:
      return "arbitrary";
    case SID_SPECIFIC:
      return "specific";
    case NULL_SIZE:
      return "";

    default:
      return "unknown";
  }
}

static const NodeID_T sPathMap[NUM_OF_PATH][2] = {
    {NID_ROOT, NID_ROOT},         // PID_ENQUE
    {NID_ROOT, NID_RAW},          // PID_ROOT_TO_RAW
    {NID_ROOT, NID_P2A},          // PID_ROOT_TO_P2A
    {NID_ROOT, NID_MULTIFRAME},   // PID_ROOT_TO_MULTIFRAME
    {NID_RAW, NID_P2A},           // PID_RAW_TO_P2A
    {NID_P2A, NID_DEPTH},         // PID_P2A_TO_DEPTH
    {NID_P2A, NID_FUSION},        // PID_P2A_TO_FUSION
    {NID_P2A, NID_MULTIFRAME},    // PID_P2A_TO_MULTIFRAME
    {NID_P2A, NID_YUV},           // PID_P2A_TO_YUV
    {NID_P2A, NID_YUV2},          // PID_P2A_TO_YUV2
    {NID_P2A, NID_MDP},           // PID_P2A_TO_MDP
    {NID_P2A, NID_FD},            // PID_P2A_TO_FD
    {NID_FD, NID_DEPTH},          // PID_FD_TO_DEPTH
    {NID_FD, NID_FUSION},         // PID_FD_TO_FUSION
    {NID_FD, NID_MULTIFRAME},     // PID_FD_TO_MULTIFRAME
    {NID_FD, NID_YUV},            // PID_FD_TO_YUV
    {NID_FD, NID_YUV2},           // PID_FD_TO_YUV2
    {NID_MULTIFRAME, NID_YUV},    // PID_MULTIFRAME_TO_YUV
    {NID_MULTIFRAME, NID_YUV2},   // PID_MULTIFRAME_TO_YUV2
    {NID_MULTIFRAME, NID_BOKEH},  // PID_MULTIFRAME_TO_BOKEH
    {NID_MULTIFRAME, NID_MDP},    // PID_MULTIFRAME_TO_MDP
    {NID_FUSION, NID_YUV},        // PID_FUSION_TO_YUV
    {NID_FUSION, NID_MDP},        // PID_FUSION_TO_MDP
    {NID_DEPTH, NID_BOKEH},       // PID_DEPTH_TO_BOKEH
    {NID_YUV, NID_BOKEH},         // PID_YUV_TO_BOKEH
    {NID_YUV, NID_YUV2},          // PID_YUV_TO_YUV2
    {NID_YUV, NID_MDP},           // PID_YUV_TO_MDP
    {NID_BOKEH, NID_YUV2},        // PID_BOKEH_TO_YUV2
    {NID_BOKEH, NID_MDP},         // PID_BOKEH_TO_MDP
    {NID_YUV2, NID_MDP},          // PID_YUV2_TO_MDP
};

PathID_T FindPath(NodeID_T src, NodeID_T dst) {
  for (PathID_T pid = PID_ENQUE + 1; pid < NUM_OF_PATH; pid++) {
    if (sPathMap[pid][0] == src && sPathMap[pid][1] == dst) {
      return pid;
    }
  }
  return NULL_PATH;
}

const NodeID_T* GetPath(PathID_T pid) {
  if (pid < NUM_OF_PATH) {
    return sPathMap[pid];
  }
  return nullptr;
}

const void* getTuningFromNvram(MUINT32 openId,
                               const MUINT32& idx,
                               MINT32 magicNo,
                               MINT32 type,
                               MBOOL enableLog) {
  (void)openId;
  (void)idx;
  (void)magicNo;
  (void)type;
  (void)enableLog;
  return NULL;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
