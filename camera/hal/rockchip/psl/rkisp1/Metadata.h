/*
 * Copyright (C) 2016 - 2017 Intel Corporation.
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

#ifndef PSL_RKISP1_METADATA_H_
#define PSL_RKISP1_METADATA_H_

#include "ControlUnit.h"
#include "PlatformData.h"
#include <math.h>

namespace android {
namespace camera2 {

class Metadata
{
public:
    Metadata(int cameraId, Rk3aPlus *a3aWrapper);
    virtual ~Metadata();
    status_t init();

    void writeSensorMetadata(RequestCtrlState &reqState);
    void writeAwbMetadata(RequestCtrlState &reqState);
    void writeMiscMetadata(RequestCtrlState &reqState) const; //[> generic. split to logical parts, if needed <]
    void writeJpegMetadata(RequestCtrlState &reqState) const;
    void writeLensMetadata(RequestCtrlState &reqState) const;
    void writeLSCMetadata(std::shared_ptr<RequestCtrlState> &reqState) const;
    void FillSensorDescriptor(const ControlUnit::Message &msg);

private:
    status_t initTonemaps();


private:
    uint32_t mMaxCurvePoints; /*!< Cache for max curve points for tonemap */
    float   *mRGammaLut;      /*!< [(P_IN, P_OUT), (P_IN, P_OUT), ..] */
    float   *mGGammaLut;      /*!< [(P_IN, P_OUT), (P_IN, P_OUT), ..] */
    float   *mBGammaLut;      /*!< [(P_IN, P_OUT), (P_IN, P_OUT), ..] */

    int mCameraId;
    rk_aiq_exposure_sensor_descriptor mSensorDescriptor;
    static constexpr float ONE_PERCENT = 0.01f;
};
} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_SETTINGSPROCESSOR_H_ */
