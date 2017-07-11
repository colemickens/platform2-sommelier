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
#ifndef AAA_INTELAFSTATEMACHINE_H_
#define AAA_INTELAFSTATEMACHINE_H_

#include <utils/Errors.h>
#include "Intel3aPlus.h"
#include <camera/camera_metadata.h>
#include <ia_aiq.h>
#include "LogHelper.h"
#include "PerformanceTraces.h"

NAMESPACE_DECLARATION {
typedef nsecs_t usecs_t;

/**
 * \class IntelAfModeBase
 *
 * Base class for all the AutoFocus modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.afState
 *
 */
class IntelAfModeBase {
public:
    IntelAfModeBase();
    virtual ~IntelAfModeBase() {};

    virtual status_t processTriggers(const uint8_t &afTrigger,
                                     const uint8_t &afMode,
                                     int preCaptureId,
                                     ia_aiq_af_input_params& afInputParams);
    virtual status_t processResult(ia_aiq_af_results& afResults,
                                   CameraMetadata& result) = 0;
    void resetState(void);
    void resetTrigger(usecs_t triggerTime);
    int getState() { return mCurrentAfState; }
    void updateResult(CameraMetadata& results);
protected:
    void checkIfFocusTimeout();
protected:
    AfControls mLastAfControls;
    uint8_t mCurrentAfState;
    uint8_t  mLensState;
    usecs_t  mLastActiveTriggerTime;   /**< in useconds */
    uint32_t mFramesSinceTrigger;
};

/**
 * \class IntelAFModeAuto
 * Derived class from IntelAFModeBase for Auto mode
 *
 */
class IntelAFModeAuto: public IntelAfModeBase {
public:
    IntelAFModeAuto();
    virtual status_t processTriggers(const uint8_t &afTrigger,
                                     const uint8_t &afMode,
                                     int preCaptureId,
                                     ia_aiq_af_input_params& afInputParams);
    virtual status_t processResult(ia_aiq_af_results& afResults,
                                  CameraMetadata& result);
};

/**
 * \class IntelAFModeContinuousPicture
 * Derived class from IntelAFModeBase for Continuous AF mode
 *
 */
class IntelAFModeContinuousPicture: public IntelAfModeBase {
public:
    IntelAFModeContinuousPicture();
    virtual status_t processTriggers(const uint8_t &afTrigger,
                                     const uint8_t &afMode,
                                     int preCaptureId,
                                     ia_aiq_af_input_params& afInputParams);
    virtual status_t processResult(ia_aiq_af_results& afResults,
                                  CameraMetadata& result);
};

/**
 * \class IntelAFModeOff
 * Derived class from IntelAFModeBase for OFF mode
 *
 */
class IntelAFModeOff: public IntelAfModeBase {
public:
    IntelAFModeOff();
    virtual status_t processTriggers(const uint8_t &afTrigger,
                                     const uint8_t &afMode,
                                     int preCaptureId,
                                     ia_aiq_af_input_params& afInputParams);
    virtual status_t processResult(ia_aiq_af_results& afResults,
                                  CameraMetadata& result);
};

/**
 * \class IntelAFStateMachine
 *
 * This class adapts the Android V3 AF triggers and state transitions to
 * the ones implemented by the Intel AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the IntelAFModeBase
 *
 */
class IntelAFStateMachine {
public:
    IntelAFStateMachine(int CameraId, const Intel3aPlus &aaa);
    virtual ~IntelAFStateMachine();

    virtual status_t processTriggers(const uint8_t &afTrigger,
                                     const uint8_t &afMode,
                                     int preCaptureId,
                                     ia_aiq_af_input_params &afInputParams);

    virtual status_t processResult(ia_aiq_af_results &afResults,
                                   ia_aiq_af_input_params &afInputParams,
                                   CameraMetadata& result);

    virtual status_t updateDefaults(const ia_aiq_af_results &afResults,
                                    const ia_aiq_af_input_params &afInputParams,
                                    CameraMetadata &result,
                                    bool fixedFocus = false) const;

private:
    // prevent copy constructor and assignment operator
    IntelAFStateMachine(const IntelAFStateMachine& other);
    IntelAFStateMachine& operator=(const IntelAFStateMachine& other);

    void focusDistanceResult(const ia_aiq_af_results &afResults,
                             const ia_aiq_af_input_params &afInputParams,
                             CameraMetadata &result) const;

private: /* members*/
    int mCameraId;
    AfControls      mLastAfControls;
    IntelAfModeBase *mCurrentAfMode;
    uint8_t         mCurrentAfState;

    std::vector<uint8_t> mAvailableModes;

    IntelAFModeOff mOffMode;
    IntelAFModeAuto mAutoMode;

    IntelAFModeContinuousPicture mContinuousPictureMode;
    const Intel3aPlus &m3A;
};
} NAMESPACE_DECLARATION_END

#endif // AAA_INTELAFSTATEMACHINE_H_
