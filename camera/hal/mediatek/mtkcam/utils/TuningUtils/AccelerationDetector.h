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

#ifndef _CAMERA3_HAL_CAMERAORIENTATIONDETECTOR_H_
#define _CAMERA3_HAL_CAMERAORIENTATIONDETECTOR_H_

#include <mtkcam/def/common.h>
#include <string>
#include <unordered_map>

namespace cros {
namespace NSCam {

class VISIBILITY_PUBLIC AccelerationDetector {
 public:
  AccelerationDetector();
  virtual ~AccelerationDetector();

  void prepare();

  bool getAcceleration(float data[3]);

 private:
  bool mPrepared;

  std::string mXFileName;
  std::string mYFileName;
  std::string mZFileName;
  std::unordered_map<std::string, int> mFds;
  std::unordered_map<std::string, int> mVals;

  float mScaleVal;
};

}  // namespace NSCam
}  // namespace cros
#endif
