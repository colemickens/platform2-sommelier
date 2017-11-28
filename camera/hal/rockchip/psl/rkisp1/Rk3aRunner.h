/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_RKISP1_Rk3aRunner_H_
#define PSL_RKISP1_Rk3aRunner_H_

#include "RequestCtrlState.h"
#include "Rk3aPlus.h"
#include "RkAEStateMachine.h"
#include "RkAWBStateMachine.h"

namespace android {
namespace camera2 {

class SettingsProcessor;
class LensHw;

class Rk3aRunner
{
public:
    Rk3aRunner(int cameraId, Rk3aPlus *aaaWrapper, SettingsProcessor *settingsProcessor, LensHw *lensController);
    status_t init();
    virtual ~Rk3aRunner();

    void reset();

    status_t run2A(RequestCtrlState &reqState, bool forceUpdated = false);
    void updateInputParams(const AiqInputParams &update) { mLatestInputParams = update; }

private:
    status_t processAeResults(RequestCtrlState &reqState);
    status_t processAwbResults(RequestCtrlState &reqState);

private:
    int mCameraId;

    AiqInputParams mLatestInputParams;
    AiqResults  mLatestResults;

    Rk3aPlus *m3aWrapper;

    /**
     * To be handled by the AE state machine
     */
    RkAEStateMachine     *mAeState;      /**< AE state machine */

    /**
     * To be handled by the AWB state machine
     */
    /**< AWB state machine */
    RkAWBStateMachine    *mAwbState;

    LensHw              *mLensController; /* Rk3aRunner doesn't own mLensController */
    SettingsProcessor *mSettingsProcessor; /* Rk3aRunner doesn't owns this */
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_Rk3aRunner_H_ */
