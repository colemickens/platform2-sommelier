/*
 * Copyright (C) 2015-2017 Intel Corporation
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
#ifndef AAA_INTELAWBSTATEMACHINE_H_
#define AAA_INTELAWBSTATEMACHINE_H_

#include <utils/Errors.h>
#include <camera/camera_metadata.h>
#include "LogHelper.h"
#include "Intel3aPlus.h"

namespace cros {
namespace intel {
/**
 * \class IntelAWBModeBase
 *
 * Base class for all the Auto white balance modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.awbState
 *
 */
class IntelAWBModeBase {
public:
    IntelAWBModeBase();
    virtual ~IntelAWBModeBase() {};

    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls) = 0;


    virtual status_t processResult(const ia_aiq_awb_results &awbResults,
                                   android::CameraMetadata &results) = 0;

    void resetState(void);
    uint8_t getState() const { return mCurrentAwbState; }
protected:
    void updateResult(android::CameraMetadata& results);
protected:
    AwbControls  mLastAwbControls;
    uint8_t     mLastControlMode;
    uint8_t     mCurrentAwbState;
};

/**
 * \class IntelAWBModeAuto
 * Derived class from IntelAWBModeBase for Auto mode
 *
 */
class IntelAWBModeAuto: public IntelAWBModeBase {
public:
    IntelAWBModeAuto();
    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls);
    virtual status_t processResult(const ia_aiq_awb_results &awbResults,
                                  android::CameraMetadata& result);
};

/**
 * \class IntelAWBModeOFF
 * Derived class from IntelAWBModeBase for OFF mode
 *
 */
class IntelAWBModeOff: public IntelAWBModeBase {
public:
    IntelAWBModeOff();
    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls);
    virtual status_t processResult(const ia_aiq_awb_results &awbResults,
                                  android::CameraMetadata& result);
};

/**
 * \class IntelAWBStateMachine
 *
 * This class adapts the Android V3 AWB triggers and state transitions to
 * the ones implemented by the Intel AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the IntelAWBModeBase
 *
 */
class IntelAWBStateMachine {
public:
    IntelAWBStateMachine(int CameraId);
    virtual ~IntelAWBStateMachine();

    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls);

    virtual status_t processResult(const ia_aiq_awb_results &awbResults,
                                   android::CameraMetadata &results);

    uint8_t getState() const { return mCurrentAwbMode->getState(); }
private:
    // prevent copy constructor and assignment operator
    IntelAWBStateMachine(const IntelAWBStateMachine& other);
    IntelAWBStateMachine& operator=(const IntelAWBStateMachine& other);

private: /* members*/
    int mCameraId;
    AwbControls  mLastAwbControls;
    uint8_t     mLastControlMode;
    uint8_t     mCurrentAwbState;
    IntelAWBModeBase *mCurrentAwbMode;

    IntelAWBModeOff mOffMode;
    IntelAWBModeAuto mAutoMode;
};
} /* namespace intel */
} /* namespace cros */
#endif // AAA_INTELAWBSTATEMACHINE_H_
