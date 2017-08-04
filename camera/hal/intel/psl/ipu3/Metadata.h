/*
 * Copyright (C) 2016 - 2017 Intel Corporation.
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

#ifndef PSL_IPU3_METADATA_H_
#define PSL_IPU3_METADATA_H_

#include "ControlUnit.h"
#include "PlatformData.h"
#include <math.h>

namespace android {
namespace camera2 {

class Metadata
{
public:
    Metadata(int cameraId, Intel3aPlus *a3aWrapper);
    virtual ~Metadata();
    status_t init();

    void writeSensorMetadata(RequestCtrlState &reqState);
    void writeAwbMetadata(RequestCtrlState &reqState);
    void writePAMetadata(RequestCtrlState &reqState);
    void writeMiscMetadata(RequestCtrlState &reqState) const; //[> generic. split to logical parts, if needed <]
    void writeJpegMetadata(RequestCtrlState &reqState) const;
    void writeLensMetadata(RequestCtrlState &reqState) const;
    void writeLSCMetadata(std::shared_ptr<RequestCtrlState> &reqState) const;
    void FillSensorDescriptor(const ControlUnit::Message &msg);
    status_t fillTonemapCurve(RequestCtrlState &reqAiqCfg);

private:
    status_t initTonemaps();


private:
    uint32_t mMaxCurvePoints; /*!< Cache for max curve points for tonemap */
    float   *mRGammaLut;      /*!< [(P_IN, P_OUT), (P_IN, P_OUT), ..] */
    float   *mGGammaLut;      /*!< [(P_IN, P_OUT), (P_IN, P_OUT), ..] */
    float   *mBGammaLut;      /*!< [(P_IN, P_OUT), (P_IN, P_OUT), ..] */

    float mLscGridRGGB[MAX_LSC_GRID_SIZE * 4];
    float mLscOffGrid[MAX_LSC_GRID_SIZE * 4];
    int mCameraId;
    ia_aiq_exposure_sensor_descriptor mSensorDescriptor;
    static constexpr float ONE_PERCENT = 0.01f;
    Intel3aPlus *m3aWrapper; /* Metadata doesn't own m3aWrapper */
    static const int32_t DELTA_ISO = 1;
};
} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_SETTINGSPROCESSOR_H_ */
