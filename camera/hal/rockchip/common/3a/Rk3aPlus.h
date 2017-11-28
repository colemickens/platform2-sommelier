/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef AAA_RK3APLUS_H_
#define AAA_RK3APLUS_H_

#include <camera/camera_metadata.h>
#include <vector>
#include <map>

#include "CameraWindow.h"
#include "UtilityMacros.h"
#include "Rk3aCore.h"

NAMESPACE_DECLARATION {
/**
 * \class Rk3aPlus
 *
 * Rk3aPlus is an interface to Rockchip 3A Library.
 * The purpose of this class is to
 * - convert Google specific metadata to rk_aiq input parameters
 * - convert rk_aiq output parameters to Google specific metadata
 * - call 3a functions through Rk3aCore
 *
 */
class Rk3aPlus : public Rk3aCore {
public:
    explicit Rk3aPlus(int camId);
    status_t initAIQ(const char* sensorName = nullptr);

    rk_aiq_frame_use getFrameUseFromIntent(const CameraMetadata * settings);

    /*
     * static common operation
     */
    float getMinFocusDistance() const { return mMinFocusDistance; }

private:
    // prevent copy constructor and assignment operator
    Rk3aPlus(const Rk3aPlus& other);
    Rk3aPlus& operator=(const Rk3aPlus& other);

// private members
private:
    int mCameraId;

    /**
     * Cached values from static metadata
     */
    float mMinFocusDistance;
    int32_t mMinAeCompensation;
    int32_t mMaxAeCompensation;

    int mMinSensitivity;
    int mMaxSensitivity;
    int64_t mMinExposureTime;
    int64_t mMaxExposureTime;
    int64_t mMaxFrameDuration;
}; //  class Rk3aPlus
} NAMESPACE_DECLARATION_END

#endif  //  AAA_RK3APLUS_H_
