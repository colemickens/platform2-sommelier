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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_HWINFOHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_HWINFOHELPER_H_
//
#include <mtkcam/def/common.h>
#include <mtkcam/drv/def/ICam_type.h>
#define DISPLAY_WIDTH 2560;

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCamHW {

class VISIBILITY_PUBLIC HwInfoHelper {
 protected:  ////                Implementor.
  class Implementor;
  class DualImplementor;
  Implementor* mpImp;

 public:
  explicit HwInfoHelper(MINT32 const openId);
  ~HwInfoHelper();

 public:
  MBOOL updateInfos();
  MBOOL isRaw() const;
  MBOOL isYuv() const;
  MBOOL hasRrzo() const { return !isYuv(); }
  MBOOL getSensorSize(MUINT32 const sensorMode, NSCam::MSize* size) const;
  MBOOL getSensorFps(MUINT32 const sensorMode, MINT32* fps) const;
  MBOOL getImgoFmt(MUINT32 const bitDepth,
                   MINT* fmt,
                   MBOOL forceUFO = MFALSE,
                   MBOOL needUnpakFmt = MFALSE) const;
  MBOOL getRrzoFmt(MUINT32 const bitDepth,
                   MINT* fmt,
                   MBOOL forceUFO = MFALSE) const;
  MBOOL getLpModeSupportBitDepthFormat(const MINT& fmt, MUINT32* pipeBit) const;
  MBOOL getRecommendRawBitDepth(MINT32* bitDepth) const;
  MBOOL getSensorPowerOnPredictionResult(MBOOL* isPowerOnSuccess) const;
  //
  MBOOL queryPixelMode(MUINT32 const sensorMode,
                       MINT32 const fps,
                       MUINT32* pixelMode) const;
  MBOOL alignPass1HwLimitation(MUINT32 const pixelMode,
                               MINT const imgFormat,
                               MBOOL isImgo,
                               NSCam::MSize* size,
                               size_t* stride) const;
  MBOOL alignRrzoHwLimitation(NSCam::MSize const targetSize,
                              NSCam::MSize const sensorSize,
                              NSCam::MSize* result) const;

  MBOOL querySupportVHDRMode(MUINT32 const sensorMode, MUINT32* vhdrMode) const;
  MBOOL quertMaxRrzoWidth(MINT32* maxWidth) const;
  MBOOL queryUFOStride(MINT const imgFormat,
                       NSCam::MSize const imgSize,
                       size_t stride[3]) const;
  MBOOL getShutterDelayFrameCount(MINT32* shutterDelayCnt) const;
  MBOOL getSensorRawFmtType(MUINT32* u4RawFmtType) const;
  MBOOL shrinkCropRegion(NSCam::MSize const sensorSize,
                         NSCam::MRect* cropRegion,
                         MINT32 shrinkPx = 2) const;

  MBOOL querySupportResizeRatio(MUINT32* rPrecentage) const;
  MBOOL querySupportBurstNum(MUINT32* rBitField) const;
  MBOOL querySupportRawPattern(MUINT32* rBitField) const;

  static MBOOL getDynamicTwinSupported();

  static MUINT32 getCameraSensorPowerOnCount();
};

};      // namespace NSCamHW
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_HWINFOHELPER_H_
