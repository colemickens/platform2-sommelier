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

/**
 * @file lmv_tuning.h
 *
 * LMV tuning Implementation Header File
 *
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_TUNING_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_TUNING_H_

#include <memory>

namespace NSCam {
namespace LMV {

/**
 *@brief LMV Pass1 HW Setting Callback
 */

class LMVP1Cb : public P1_TUNING_NOTIFY {
 public:
  explicit LMVP1Cb(MVOID* arg);
  virtual ~LMVP1Cb();

  virtual void p1TuningNotify(MVOID* pInput, MVOID* pOutput);
  virtual const char* TuningName() { return "Update LMV"; }
};

class SGG2P1Cb : public P1_TUNING_NOTIFY {
 public:
  SGG2P1Cb();
  virtual ~SGG2P1Cb();

  virtual void p1TuningNotify(MVOID* pIn, MVOID* pOut);
  virtual const char* TuningName() { return "Update SGG2"; }
};

class LMVTuning {
 public:
  explicit LMVTuning(MVOID* obj);
  virtual ~LMVTuning();

 public:
  MBOOL isSupportLMVCb();
  MBOOL isSupportSGG2Cb();

  std::shared_ptr<LMVP1Cb> getLMVCb();
  std::shared_ptr<SGG2P1Cb> getSGG2Cb();

 private:
  std::shared_ptr<LMVP1Cb> mLMVCb = nullptr;
  std::shared_ptr<SGG2P1Cb> mSGG2Cb = nullptr;

  MBOOL mIsSupportLMVCb = MTRUE;
  MBOOL mIsSupportSGG2Cb = MFALSE;
};

}  // namespace LMV
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_TUNING_H_
