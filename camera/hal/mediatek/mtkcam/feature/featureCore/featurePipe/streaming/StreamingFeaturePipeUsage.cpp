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

#include <featurePipe/common/include/DebugControl.h>

#define PIPE_CLASS_TAG "PipeUsage"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_USAGE
#include <featurePipe/common/include/PipeLog.h>
#include <mtkcam/feature/featureCore/featurePipe/streaming/StreamingFeaturePipeUsage.h>
#include "StreamingFeaturePipe.h"
#include "TuningHelper.h"
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

typedef IStreamingFeaturePipe::UsageHint UsageHint;
#define ADD_3DNR_RSC_SUPPORT 1

StreamingFeaturePipeUsage::StreamingFeaturePipeUsage() {}

StreamingFeaturePipeUsage::StreamingFeaturePipeUsage(UsageHint hint,
                                                     MUINT32 sensorIndex)
    : mUsageHint(hint),
      mStreamingSize(mUsageHint.mStreamingSize),
      mVendorMode(mUsageHint.mVendorMode),
      mVendorCusSize(mUsageHint.mVendorCusSize),
      m3DNRMode(mUsageHint.m3DNRMode),
      mSensorIndex(sensorIndex),
      mOutCfg(mUsageHint.mOutCfg),
      mNumSensor(mUsageHint.mAllSensorIDs.size()),
      mResizedRawSizeList(mUsageHint.mResizedRawMap) {
  if (mUsageHint.mMode == IStreamingFeaturePipe::USAGE_DEFAULT) {
    mUsageHint.mMode = IStreamingFeaturePipe::USAGE_FULL;
  }

  // remove future
  mUsageHint.mVendorMode =
      (getPropertyValue(KEY_ENABLE_VENDOR_V1, SUPPORT_VENDOR_NODE) == 1);
  mVendorMode = mUsageHint.mVendorMode;
  mVendorDebug = getPropertyValue(KEY_DEBUG_TPI, 0);
  mVendorLog = getPropertyValue(KEY_DEBUG_TPI_LOG, 0);
  mEnableVendorCusSize =
      (getPropertyValue(KEY_ENABLE_VENDOR_V1_SIZE, SUPPORT_VENDOR_SIZE) == 1);
  mEnableVendorCusFormat = (getPropertyValue(KEY_ENABLE_VENDOR_V1_FORMAT,
                                             SUPPORT_VENDOR_FORMAT) == 1);
  mEnableDummy = (getPropertyValue(KEY_ENABLE_DUMMY, SUPPORT_DUMMY_NODE) == 1);
  mSupportPure = (getPropertyValue(KEY_ENABLE_PURE_YUV, SUPPORT_PURE_YUV) == 1);

  switch (mUsageHint.mMode) {
    case IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH_TIME_SHARING:
      mPipeFunc = 0;
      mP2AMode = P2A_MODE_TIME_SHARING;
      break;
    case IStreamingFeaturePipe::USAGE_P2A_FEATURE:
      mPipeFunc = IStreamingFeaturePipe::PIPE_USAGE_3DNR;
      mP2AMode = P2A_MODE_FEATURE;
      break;
    case IStreamingFeaturePipe::USAGE_FULL:
      mPipeFunc = IStreamingFeaturePipe::PIPE_USAGE_EIS |
                  IStreamingFeaturePipe::PIPE_USAGE_3DNR |
                  IStreamingFeaturePipe::PIPE_USAGE_EARLY_DISPLAY;
      mP2AMode = P2A_MODE_FEATURE;
      break;
    case IStreamingFeaturePipe::USAGE_STEREO_EIS:
      mPipeFunc = IStreamingFeaturePipe::PIPE_USAGE_EIS;
      mP2AMode = P2A_MODE_BYPASS;
      break;
    case IStreamingFeaturePipe::USAGE_P2A_PASS_THROUGH:
    default:
      mPipeFunc = 0;
      mP2AMode = P2A_MODE_NORMAL;
      break;
  }
  if (m3DNRMode && !support3DNR()) {
    MY_LOGE("3DNR not supportted! But UsageHint 3DNR Mode(%d) enabled!!",
            m3DNRMode);
  }
  MY_LOGI("create usage : sPure(%d), mP2AMode:%d,mMode:%d", mSupportPure,
          mP2AMode, mUsageHint.mMode);
}
MBOOL StreamingFeaturePipeUsage::supportP2AP2() const {
  return MTRUE;
}
MBOOL StreamingFeaturePipeUsage::supportLargeOut() const {
  return mOutCfg.mHasLarge;
}
MBOOL StreamingFeaturePipeUsage::supportPhysicalOut() const {
  return mOutCfg.mHasPhysical;
}

MBOOL StreamingFeaturePipeUsage::supportIMG3O() const {
#ifdef SUPPORT_IMG3O
  return MTRUE;
#else
  return MFALSE;
#endif
}
MBOOL StreamingFeaturePipeUsage::supportP2ALarge() const {
  return mOutCfg.mHasLarge;
}

MBOOL StreamingFeaturePipeUsage::support4K2K() const {
  return is4K2K(mStreamingSize);
}
MBOOL StreamingFeaturePipeUsage::supportTimeSharing() const {
  return mP2AMode == P2A_MODE_TIME_SHARING;
}

MBOOL StreamingFeaturePipeUsage::supportP2AFeature() const {
  return mP2AMode == P2A_MODE_FEATURE;
}

MBOOL StreamingFeaturePipeUsage::supportBypassP2A() const {
  return mP2AMode == P2A_MODE_BYPASS;
}

MBOOL StreamingFeaturePipeUsage::supportYUVIn() const {
  return mP2AMode == P2A_MODE_BYPASS;
}

MBOOL StreamingFeaturePipeUsage::supportPure() const {
  return mSupportPure;
}

MBOOL StreamingFeaturePipeUsage::supportFull_YUY2() const {
  return MFALSE;  // USE_YUY2_FULL_IMG && supportWPE() ;
}
MBOOL StreamingFeaturePipeUsage::support3DNR() const {
  MBOOL ret = MFALSE;
  if (!supportIMG3O()) {
    return MFALSE;
  }
  if (this->is3DNRModeMaskEnable(NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) ||
      this->is3DNRModeMaskEnable(NR3D::E3DNR_MODE_MASK_UI_SUPPORT)) {
    ret = this->supportP2AFeature();
  }
  return ret;
}
MBOOL StreamingFeaturePipeUsage::support3DNRRSC() const {
  MBOOL ret = MFALSE;
#if ADD_3DNR_RSC_SUPPORT
  if (support3DNR() && is3DNRModeMaskEnable(NR3D::E3DNR_MODE_MASK_RSC_EN)) {
    ret = MTRUE;
  }
#endif  // ADD_3DNR_RSC_SUPPORT
  return ret;
}
MBOOL StreamingFeaturePipeUsage::is3DNRModeMaskEnable(
    NR3D::E3DNR_MODE_MASK mask) const {
  return (m3DNRMode & mask);
}

MBOOL StreamingFeaturePipeUsage::supportGraphicBuffer() const {
  return MTRUE;  // !NSCam::NSIoPipe::WPEQuerySupport();
}

EImageFormat StreamingFeaturePipeUsage::getFullImgFormat() const {
  return supportFull_YUY2() ? eImgFmt_YUY2 : eImgFmt_YV12;
}

MBOOL StreamingFeaturePipeUsage::supportDummy() const {
  return mEnableDummy;
}

MBOOL StreamingFeaturePipeUsage::isDynamicTuning() const {
  return mUsageHint.mDynamicTuning;
}

MBOOL StreamingFeaturePipeUsage::isQParamIOValid() const {
  return mUsageHint.mQParamIOValid;
}

std::vector<MUINT32> StreamingFeaturePipeUsage::getAllSensorIDs() const {
  return mUsageHint.mAllSensorIDs;
}

MUINT32 StreamingFeaturePipeUsage::getMode() const {
  return mUsageHint.mMode;
}
MUINT32 StreamingFeaturePipeUsage::getSensorModule() const {
  return mUsageHint.mSensorModule;
}

MUINT32 StreamingFeaturePipeUsage::getVendorMode() const {
  return mVendorMode;
}
MUINT32 StreamingFeaturePipeUsage::get3DNRMode() const {
  return m3DNRMode;
}

MSize StreamingFeaturePipeUsage::getStreamingSize() const {
  return mStreamingSize;
}

MSize StreamingFeaturePipeUsage::getRrzoSizeByIndex(MUINT32 index) {
  MSize result;
  if (index >= mResizedRawSizeList.size()) {
    MY_LOGE("index(%d) >= resized raw size list(%d)", index,
            mResizedRawSizeList.size());
    return result;
  }
  result = mResizedRawSizeList[index];
  return result;
}

MUINT32 StreamingFeaturePipeUsage::getNumSensor() const {
  return mNumSensor;
}

MUINT32 StreamingFeaturePipeUsage::getNumP2ABuffer() const {
  MUINT32 num =
      (mOutCfg.mMaxOutNum > 2) ? 3 : 0;  // need full for additional MDP run (
                                         // no need consider physical & large)
  num = fmax(num, get3DNRBufferNum().mBasic);
  return num;
}

MUINT32 StreamingFeaturePipeUsage::getNumP2APureBuffer() const {
  MUINT32 num = 3;  // max((MUINT32)3, getVendorBufferNum().mBasic);
  return num;
}
MUINT32 StreamingFeaturePipeUsage::getNumP2ATuning() const {
  MUINT32 num = MIN_P2A_TUNING_BUF_NUM;
  if (mOutCfg.mHasPhysical) {
    num *= 2;
  }
  if (supportP2ALarge()) {
    num *= 2;
  }
  num = fmax(num, getNumP2ABuffer());
  return num;
}
StreamingFeaturePipeUsage::BufferNumInfo
StreamingFeaturePipeUsage::get3DNRBufferNum() const {
  MUINT32 num = 0;
  if (support3DNR()) {
    num = 3;
  }
  return BufferNumInfo(num);
}
MUINT32 StreamingFeaturePipeUsage::getSensorIndex() const {
  return mSensorIndex;
}
MUINT32 StreamingFeaturePipeUsage::supportVendor(MUINT32 ver) const {
  MUINT32 v1 = mVendorMode ? 1 : 0;
  MUINT32 v2 = 0;
  if (mP2AMode == P2A_MODE_FEATURE) {
    ver = ver == 1 ? v1 : ver == 2 ? v2 : v1 ? v1 : v2;
  }
  return ver;
}

MBOOL StreamingFeaturePipeUsage::supportVendorDebug() const {
  return mVendorDebug;
}

MBOOL StreamingFeaturePipeUsage::supportVendorLog() const {
  return mVendorLog;
}

MBOOL StreamingFeaturePipeUsage::supportVendorInplace() const {
  return MFALSE;
}

MBOOL StreamingFeaturePipeUsage::supportVendorCusSize() const {
  return MFALSE;
}

MBOOL StreamingFeaturePipeUsage::supportVendorCusFormat() const {
  return MFALSE;
}

MBOOL StreamingFeaturePipeUsage::supportVendorFullImg() const {
  // always use separate vendor fullimg
  return MTRUE;
}

MSize StreamingFeaturePipeUsage::getVendorCusSize(const MSize& original) const {
  return original;
}

EImageFormat StreamingFeaturePipeUsage::getVendorCusFormat(
    const EImageFormat& original) const {
  return original;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
