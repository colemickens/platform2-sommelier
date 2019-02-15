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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURE_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURE_COMMON_H_

#include <algorithm>
#include <common/include/ImageBufferPool.h>
#include <common/include/MtkHeader.h>
#include <memory>
#include <mtkcam/utils/hw/HwTransform.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/std/Log.h>
// check later
// #include <core/include/FatImageBufferPool.h>
// check later
// #include <core/include/GraphicBufferPool.h>

// check later
// #include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#if MTK_DP_ENABLE
#include <DpIspStream.h>
#endif
#include "DebugControl.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

MBOOL copyImageBuffer(IImageBuffer* src, std::shared_ptr<IImageBuffer> dst);

MBOOL dumpToFile(IImageBuffer* buffer, const char* fmt, ...);

typedef MUINT8 PathID_T;
typedef MUINT8 NodeID_T;
const char* NodeID2Name(MUINT8 nodeId);
const char* PathID2Name(MUINT8 pathId);
const char* TypeID2Name(MUINT8 typeId);
const char* FeatID2Name(MUINT8 featId);
const char* SizeID2Name(MUINT8 sizeId);
PathID_T FindPath(NodeID_T src, NodeID_T dst);
const NodeID_T* GetPath(PathID_T pid);

/******************************************************************************
 *  Metadata Access
 ******************************************************************************/
/**
 * Try to get metadata value
 *
 * @param[in]  pMetadata: IMetadata instance
 * @param[in]  tag: the metadata tag to retrieve
 * @param[out]  rVal: the metadata value
 *
 *
 * @return
 *  -  true if successful; otherwise false.
 */
template <typename T>
inline MBOOL tryGetMetadata(const IMetadata* pMetadata,
                            MUINT32 const tag,
                            T* rVal) {
  if (pMetadata == NULL) {
    CAM_LOGW("pMetadata == NULL");
    return MFALSE;
  }
  //
  IMetadata::IEntry entry = pMetadata->entryFor(tag);
  if (!entry.isEmpty()) {
    *rVal = entry.itemAt(0, Type2Type<T>());
    return MTRUE;
  }
  return MFALSE;
}

/**
 * Try to set metadata value
 *
 * @param[in]  pMetadata: IMetadata instance
 * @param[in]  tag: the metadata tag to set
 * @param[in]  val: the metadata value to be configured
 *
 *
 * @return
 *  -  true if successful; otherwise false.
 */
template <typename T>
inline MVOID trySetMetadata(IMetadata* pMetadata,
                            MUINT32 const tag,
                            T const& val) {
  if (pMetadata == NULL) {
    CAM_LOGW("pMetadata == NULL");
    return;
  }
  //
  IMetadata::IEntry entry(tag);
  entry.push_back(val, Type2Type<T>());
  pMetadata->update(tag, entry);
}

// utilities for crop

/******************************************************************************
 *  Metadata Access
 ******************************************************************************/
inline MINT32 div_round(MINT32 const numerator, MINT32 const denominator) {
  return ((numerator < 0) ^ (denominator < 0))
             ? ((numerator - denominator / 2) / denominator)
             : ((numerator + denominator / 2) / denominator);
}

struct vector_f {  // vector with floating point
  MPoint p;
  MPoint pf;

  explicit vector_f(MPoint const& rP = MPoint(), MPoint const& rPf = MPoint())
      : p(rP), pf(rPf) {}
};

struct simpleTransform {
  // just support translation than scale, not a general formulation
  // translation
  MPoint tarOrigin;
  // scale
  MSize oldScale;
  MSize newScale;

  simpleTransform(MPoint rOrigin = MPoint(),
                  MSize rOldScale = MSize(),
                  MSize rNewScale = MSize())
      : tarOrigin(rOrigin), oldScale(rOldScale), newScale(rNewScale) {}
};

// transform MPoint
inline MPoint transform(simpleTransform const& trans, MPoint const& p) {
  return MPoint(
      div_round((p.x - trans.tarOrigin.x) * trans.newScale.w, trans.oldScale.w),
      div_round((p.y - trans.tarOrigin.y) * trans.newScale.h,
                trans.oldScale.h));
}

inline MPoint inv_transform(simpleTransform const& trans, MPoint const& p) {
  return MPoint(
      div_round(p.x * trans.oldScale.w, trans.newScale.w) + trans.tarOrigin.x,
      div_round(p.y * trans.oldScale.h, trans.newScale.h) + trans.tarOrigin.y);
}

inline int int_floor(float x) {
  int i = static_cast<int>(x);
  return i - (i > x);
}

// transform vector_f
inline vector_f transform(simpleTransform const& trans, vector_f const& p) {
  MFLOAT const x = (p.p.x + (p.pf.x / (MFLOAT)(1u << 31))) * trans.newScale.w /
                   trans.oldScale.w;
  MFLOAT const y = (p.p.y + (p.pf.y / (MFLOAT)(1u << 31))) * trans.newScale.h /
                   trans.oldScale.h;
  int const x_int = int_floor(x);
  int const y_int = int_floor(y);
  return vector_f(MPoint(x_int, y_int),
                  MPoint((x - x_int) * (1u << 31), (y - y_int) * (1u << 31)));
}

inline vector_f inv_transform(simpleTransform const& trans, vector_f const& p) {
  MFLOAT const x = (p.p.x + (p.pf.x / (MFLOAT)(1u << 31))) * trans.oldScale.w /
                   trans.newScale.w;
  MFLOAT const y = (p.p.y + (p.pf.y / (MFLOAT)(1u << 31))) * trans.oldScale.h /
                   trans.newScale.h;
  int const x_int = int_floor(x);
  int const y_int = int_floor(y);
  return vector_f(MPoint(x_int, y_int),
                  MPoint((x - x_int) * (1u << 31), (y - y_int) * (1u << 31)));
}

// transform MSize
inline MSize transform(simpleTransform const& trans, MSize const& s) {
  return MSize(div_round(s.w * trans.newScale.w, trans.oldScale.w),
               div_round(s.h * trans.newScale.h, trans.oldScale.h));
}

inline MSize inv_transform(simpleTransform const& trans, MSize const& s) {
  return MSize(div_round(s.w * trans.oldScale.w, trans.newScale.w),
               div_round(s.h * trans.oldScale.h, trans.newScale.h));
}

// transform MRect
inline MRect transform(simpleTransform const& trans, MRect const& r) {
  return MRect(transform(trans, r.p), transform(trans, r.s));
}

inline MRect inv_transform(simpleTransform const& trans, MRect const& r) {
  return MRect(inv_transform(trans, r.p), inv_transform(trans, r.s));
}

class CropCalculator {
 public:
  CropCalculator(MUINT uSensorIndex, MUINT32 uLogLevel);

 public:
  struct Factor {
    MSize mSensorSize;
    // P1 Crop Info
    MRect mP1SensorCrop;
    MRect mP1DmaCrop;
    MSize mP1ResizerSize;
    // Transform Matrix
    NSCamHW::HwMatrix mActive2Sensor;
    NSCamHW::HwMatrix mSensor2Active;
    simpleTransform mSensor2Resizer;
    MINT32 mSensorMode;
    // Target crop: cropRegion (active array coorinate)
    // not applied eis's mv yet, but the crop area is already reduced by EIS
    // ratio.
    MRect mActiveCrop;

    MVOID dump() const;
  };

  std::shared_ptr<Factor> getFactor(const IMetadata* inApp,
                                    const IMetadata* inHal);

  static MVOID evaluate(MSize const& srcSize,
                        MSize const& dstSize,
                        MRect* srcCrop);

  MVOID evaluate(std::shared_ptr<Factor> pFactor,
                 MSize const& dstSize,
                 MRect* srcCrop,
                 MBOOL const bResized = MFALSE) const;

  MBOOL refineBoundary(MSize const& bufSize, MRect* crop) const;

  const MRect& getActiveArray() { return mActiveArray; }

 private:
  MUINT32 mLogLevel;
  MRect mActiveArray;
  mutable NSCamHW::HwTransHelper mHwTransHelper;
};

enum NVRAM_TYPE { NVRAM_TYPE_SWNR_THRES };

const void* getTuningFromNvram(MUINT32 openId,
                               const MUINT32& idx,
                               MINT32 magicNo,
                               MINT32 type,
                               MBOOL enableLog);

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURE_COMMON_H_
