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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CROPPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CROPPER_H_

#include "P2_Header.h"

namespace P2 {

class P2Cropper : public Cropper {
 public:
  P2Cropper();
  P2Cropper(const ILog& log,
            const P2SensorInfo* sensorInfo,
            const P2SensorData* sensorData,
            const LMVInfo& lmvInfo);
  virtual ~P2Cropper();

  MBOOL isValid() const;
  MSize getSensorSize() const;
  MRect getResizedCrop() const;
  MRect getP1Crop() const;
  MSize getP1OutSize() const;
  MRect getP1BinCrop() const;
  MSize getP1BinSize() const;
  const LMVInfo& getLMVInfo() const;
  MCropRect calcViewAngle(const ILog& log,
                          const MSize& size,
                          MUINT32 cropFlag) const;
  MCropRect calcViewAngle(const ILog& log,
                          const MSize& size,
                          MUINT32 cropFlag,
                          const float cropRatio) const;
  MRectF calcViewAngleF(const ILog& log,
                        const MSize& size,
                        MUINT32 cropFlag,
                        const float cropRatio,
                        const MINT32 dmaConstrainFlag) const;
  MRectF applyViewRatio(const ILog& log,
                        const MRectF& src,
                        const MSize& size) const;
  MBOOL refineBoundaryF(const ILog& log,
                        const MSizeF& imgSize,
                        MRectF* crop) const;
  MBOOL refineBoundary(const ILog& log,
                       const MSize& imgSize,
                       MCropRect* crop) const;
  MRect getCropRegion() const;
  MRect getActiveCrop() const;
  MRect toActive(const MCropRect& cropRect, MBOOL resize) const;
  MRect toActive(const MRectF& cropF, MBOOL resize) const;
  MRect toActive(const MRect& crop, MBOOL resize) const;
  MVOID dump(const ILog& log) const;

 private:
  MBOOL initAppInfo(const P2SensorData* data);
  MBOOL initHalInfo(const P2SensorData* data);
  MBOOL initTransform();
  MBOOL initLMV();
  MVOID prepareLMV();
  MRect clip(const MRect& src, const MRect& box) const;
  MRectF applyEIS12(const ILog& og, const MRectF& src, MBOOL useResize) const;
  MRectF applyCropRatio(const ILog& log,
                        const MRectF& src,
                        const float ratio) const;

 private:
  ILog mLog;
  LMVInfo mLMVInfo;
  MUINT32 mSensorID = INVALID_SENSOR_ID;
  MBOOL mIsValid = MFALSE;
  MINT32 mSensorMode = 0;
  MUINT32 mEISFactor;

  MSize mSensorSize;
  MSize mActiveSize;
  MRect mActiveCrop;
  MRect mSensorCrop;
  MRect mResizedCrop;
  MRect mP1Crop;
  MRect mP1DMA;
  MSize mP1OutSize;
  MRect mP1BinCrop;
  MSize mP1BinSize;

  HwMatrix mActive2Sensor;
  HwMatrix mSensor2Active;
  simpleTransform mSensor2Resized;

  vector_f mActiveLMV;
  vector_f mSensorLMV;
  vector_f mResizedLMV;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CROPPER_H_
