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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_P2CAMCONTEXT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_P2CAMCONTEXT_H_

#include <common/3dnr/3dnr_hal_base.h>
#include <memory>
#include <mutex>
#include <mtkcam/def/common.h>
#include <mtkcam/aaa/IHal3A.h>
#include "StreamingFeatureData.h"
#include <utility>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class P2CamContext {
 public:
  static constexpr int SENSOR_INDEX_MAX = 4;

 private:
  static const char* MODULE_NAME;

  static std::mutex sMutex;
  static std::shared_ptr<P2CamContext> spInstance[SENSOR_INDEX_MAX];

  MINT32 mRefCount;
  MUINT32 mSensorIndex;
  MBOOL mIsInited;
  std::shared_ptr<NSCam::NSIoPipe::NSPostProc::hal3dnrBase> mp3dnr;
  ImgBuffer mPrevFullImg;
  std::shared_ptr<NS3Av3::IHal3A> mp3A;

  void init(const StreamingFeaturePipeUsage& pipeUsage);
  void uninit();

 public:
  explicit P2CamContext(MUINT32 sensorIndex);
  virtual ~P2CamContext();

  static std::shared_ptr<P2CamContext> createInstance(
      MUINT32 sensorIndex, const StreamingFeaturePipeUsage& pipeUsage);

  static void destroyInstance(MUINT32 sensorIndex);
  static std::shared_ptr<P2CamContext> getInstance(MUINT32 sensorIndex);

  MUINT32 getSensorIndex() { return mSensorIndex; }
  std::shared_ptr<NSCam::NSIoPipe::NSPostProc::hal3dnrBase> get3dnr() {
    return mp3dnr;
  }
  ImgBuffer getPrevFullImg() { return mPrevFullImg; }

  std::shared_ptr<NS3Av3::IHal3A> get3A() { return mp3A; }

  void setPrevFullImg(ImgBuffer buffer) { mPrevFullImg = buffer; }

  template <typename _Func, typename... _Arg>
  static void forAllInstances(_Func&& func, _Arg&&... arg);

  // No copy
  P2CamContext(const P2CamContext&) = delete;
  P2CamContext& operator=(const P2CamContext&) = delete;
};

// NOTE: Do not invoke destoryInstance() or other function
// which may acquire the mutex in the callback function
template <typename _Func, typename... _Arg>
void P2CamContext::forAllInstances(_Func&& func, _Arg&&... arg) {
  std::lock_guard<std::mutex> lock(sMutex);

  for (MUINT32 i = 0; i < SENSOR_INDEX_MAX; i++) {
    std::shared_ptr<P2CamContext> instance = spInstance[i];
    if (instance != NULL) {
      func(instance, std::forward<_Arg>(arg)...);
    }
  }
}

std::shared_ptr<P2CamContext> getP2CamContext(MUINT32 sensorIndex);

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_P2CAMCONTEXT_H_
