/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_IPU3_AAARUNNER_H_
#define PSL_IPU3_AAARUNNER_H_

#include "RequestCtrlState.h"

namespace android {
namespace camera2 {

class IntelAEStateMachine;
class IntelAFStateMachine;
class IntelAWBStateMachine;

class SettingsProcessor;
class LensHw;

class AAARunner
{
public:
    AAARunner(int cameraId, Intel3aPlus *aaaWrapper, SettingsProcessor *settingsProcessor, LensHw *lensController);
    status_t init(bool digiGainOnSensor);
    virtual ~AAARunner();

    void reset();

    status_t run2A(RequestCtrlState &reqState);
    void runAf(RequestCtrlState &reqState);
    AiqResults &getLatestResults() { return mLatestResults; }
    void updateInputParams(const AiqInputParams &update) { mLatestInputParams = update; }

    status_t allocateLscTable(int tableSize);
    void initLsc(AiqResults &results, uint32_t lscSize) const;

private:
    status_t updateNeutralColorPoint(RequestCtrlState &reqAiqCfg);
    status_t processAeResults(RequestCtrlState &reqState);
    status_t processAfResults(RequestCtrlState &reqState);
    status_t processAfTriggers(RequestCtrlState &reqAiqCfg);
    status_t processSAResults(RequestCtrlState &reqState);
    status_t processAwbResults(RequestCtrlState &reqState);

    void applyDigitalGain(RequestCtrlState &reqState, float digitalGain) const;
    status_t applyTonemaps(RequestCtrlState &reqState);

    float interpolate(float pos, const float *src, int srcSize) const;
    void interpolateArray(const float *src, int srcSize, float *dst, int dstSize) const;

private:
    int mCameraId;
    AiqResults  mLatestResults;
    AiqInputParams mLatestInputParams;
    Intel3aPlus *m3aWrapper;

    // LSC data
    float mResizeLscGridR[MAX_LSC_GRID_SIZE];
    float mResizeLscGridGr[MAX_LSC_GRID_SIZE];
    float mResizeLscGridGb[MAX_LSC_GRID_SIZE];
    float mResizeLscGridB[MAX_LSC_GRID_SIZE];
    float mLscGridRGGB[MAX_LSC_GRID_SIZE * 4];


    /**
     * To be handled by the AE state machine
     */
    IntelAEStateMachine     *mAeState;      /**< AE state machine */

    /*
     * AF state machine
     */
    IntelAFStateMachine *mAfState;

    /**
     * To be handled by the AWB state machine
     */
    IntelAWBStateMachine    *mAwbState;      /**< AWB state machine */

    LensHw              *mLensController; /* AAARunner doesn't own mLensController */

    float mLastSaGain; /**< digital gain applied on last saved LSC Table */

    SettingsProcessor *mSettingsProcessor; /* AAARunner doesn't owns this */

    bool mDigiGainOnSensor;

    static const int PRECAP_TIME_ALIVE = 15; /*!< Precapture results validity,
                                                  //counted in request id's */
    AiqResults       mPrecaptureResults;     /*!< Results from precapture, to be
                                                  used in the actual still capture */
    int              mPrecaptureResultRequestId;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_AAARUNNER_H_ */
