/*
 * Copyright (C) 2018 Intel Corporation
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

#include <string>
#include <unordered_map>

namespace android {
namespace camera2 {

enum CameraOrientationDetectorAngle
{
    CAM_ORI_DETECTOR_ANGLE_0 = 0,
    CAM_ORI_DETECTOR_ANGLE_90 = 90,
    CAM_ORI_DETECTOR_ANGLE_180 = 180,
    CAM_ORI_DETECTOR_ANGLE_270 = 270
};

class CameraOrientationDetector {
public:
    CameraOrientationDetector(int facing);
    virtual ~CameraOrientationDetector();

    void prepare();

    CameraOrientationDetectorAngle getOrientation();
private:
    int mFacing;
    bool mPrepared;

    std::string mXFileName;
    std::string mYFileName;
    std::string mZFileName;
    std::unordered_map<std::string, int> mFds;
    std::unordered_map<std::string, int> mVals;

    float mScaleVal;
};

}
}
#endif
