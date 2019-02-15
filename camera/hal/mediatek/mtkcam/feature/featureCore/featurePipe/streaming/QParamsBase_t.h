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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_QPARAMSBASE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_QPARAMSBASE_T_H_

#include "MtkHeader.h"
#include <memory>
#include <mutex>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
class QParamsBase {
 public:
  enum MAGIC {
    MAGIC_VALID = 0xabcd,
    MAGIC_USED = 0xdddd,
    MAGIC_FREED = 0xfaaf,
  };

  class BACKUP_DATA_TYPE {
   public:
    BACKUP_DATA_TYPE() : mMagic(MAGIC_VALID) {}

    BACKUP_DATA_TYPE(std::shared_ptr<QParamsBase> parent,
                     NSCam::NSIoPipe::QParams qparams,
                     const T& data)
        : mParent(parent),
          mQParamsCookie(qparams.mpCookie),
          mQParamsCB(qparams.mpfnEnQFailCallback),
          mQParamsFailCB(qparams.mpfnEnQFailCallback),
          mQParamsBlockCB(qparams.mpfnEnQBlockCallback),
          mData(data),
          mMagic(MAGIC_VALID) {}

    ~BACKUP_DATA_TYPE() { mMagic = MAGIC_FREED; }

    void restore(NSCam::NSIoPipe::QParams* qparams) {
      qparams->mpCookie = mQParamsCookie;
      qparams->mpfnCallback = mQParamsCB;
      qparams->mpfnEnQFailCallback = mQParamsFailCB;
      qparams->mpfnEnQBlockCallback = mQParamsBlockCB;
    }

    std::shared_ptr<QParamsBase> mParent;
    std::unique_ptr<void> mQParamsCookie;
    NSCam::NSIoPipe::QParams::PFN_CALLBACK_T mQParamsCB;
    NSCam::NSIoPipe::QParams::PFN_CALLBACK_T mQParamsFailCB;
    NSCam::NSIoPipe::QParams::PFN_CALLBACK_T mQParamsBlockCB;
    T mData;
    MAGIC mMagic;
  };

  QParamsBase();
  virtual ~QParamsBase();

 protected:
  virtual MBOOL onQParamsCB(const NSCam::NSIoPipe::QParams& param,
                            const T& data) = 0;
  virtual MBOOL onQParamsFailCB(const NSCam::NSIoPipe::QParams& param,
                                const T& data);
  virtual MBOOL onQParamsBlockCB(const NSCam::NSIoPipe::QParams& param,
                                 const T& data);

  enum CB_TYPE { CB_DONE, CB_FAIL, CB_BLOCK };
  static void processCB(NSCam::NSIoPipe::QParams param, CB_TYPE type);

 public:
  static void staticQParamsCB(NSCam::NSIoPipe::QParams* param);
  static void staticQParamsFailCB(NSCam::NSIoPipe::QParams* param);
  static void staticQParamsBlockCB(NSCam::NSIoPipe::QParams* param);

 protected:
  MBOOL enqueQParams(NSCam::NSIoPipe::NSPostProc::INormalStream* stream,
                     NSCam::NSIoPipe::QParams param,
                     const T& data);
  MVOID waitEnqueQParamsDone();

 private:
  MVOID signalDone();
  MVOID signalEnque();
  static BACKUP_DATA_TYPE* checkOutBackup(void* handle);

 private:
  std::mutex mMutex;
  std::condition_variable mCondition;
  unsigned mCount;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_QPARAMSBASE_T_H_
