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
#ifndef AAA_INTELAESTATEMACHINE_H_
#define AAA_INTELAESTATEMACHINE_H_

#include <utils/Errors.h>
#include <camera/camera_metadata.h>
#include "LogHelper.h"
#include "Intel3aPlus.h"

NAMESPACE_DECLARATION {
/**
 * \class IntelAEModeBase
 *
 * Base class for all the Autoexposure modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.aeState
 *
 */
class IntelAEModeBase {
public:
    IntelAEModeBase();
    virtual ~IntelAEModeBase() {};

    virtual status_t processState(const uint8_t &controlMode,
                                  const AeControls &aeControls) = 0;


    virtual status_t processResult(const ia_aiq_ae_results &aeResults,
                                   CameraMetadata &results,
                                   uint32_t reqId) = 0;

    void resetState(void);
    uint8_t getState() const { return mCurrentAeState; }
protected:
    void updateResult(CameraMetadata &results);
protected:
    AeControls  mLastAeControls;
    uint8_t     mLastControlMode;
    bool        mEvChanged; /**< set and kept to true when ev changes until
                                 converged */

    bool        mLastAeConvergedFlag;
    uint8_t     mAeRunCount;
    uint8_t     mAeConvergedCount;
    uint8_t     mCurrentAeState;
};

/**
 * \class IntelAEModeAuto
 * Derived class from IntelAEModeBase for Auto mode
 *
 */
class IntelAEModeAuto: public IntelAEModeBase {
public:
    IntelAEModeAuto();
    virtual status_t processState(const uint8_t &controlMode,
                                  const AeControls &aeControls);
    virtual status_t processResult(const ia_aiq_ae_results & aeResults,
                                  CameraMetadata& result,
                                  uint32_t reqId);
};

/**
 * \class IntelAEModeOFF
 * Derived class from IntelAEModeBase for OFF mode
 *
 */
class IntelAEModeOff: public IntelAEModeBase {
public:
    IntelAEModeOff();
    virtual status_t processState(const uint8_t &controlMode,
                                  const AeControls &aeControls);
    virtual status_t processResult(const ia_aiq_ae_results & aeResults,
                                  CameraMetadata& result,
                                  uint32_t reqId);
};

/**
 * \class IntelAEStateMachine
 *
 * This class adapts the Android V3 AE triggers and state transitions to
 * the ones implemented by the Intel AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the IntelAEModeBase
 *
 */
class IntelAEStateMachine {
public:
    IntelAEStateMachine(int CameraId);
    virtual ~IntelAEStateMachine();

    virtual status_t processState(const uint8_t &controlMode,
                                  const AeControls &aeControls);

    virtual status_t processResult(const ia_aiq_ae_results &aeResults,
                                   CameraMetadata &results,
                                   uint32_t reqId);

    uint8_t getState() const { return mCurrentAeMode->getState(); }
private:
    // prevent copy constructor and assignment operator
    IntelAEStateMachine(const IntelAEStateMachine &other);
    IntelAEStateMachine& operator=(const IntelAEStateMachine &other);

private: /* members*/
    int mCameraId;
    AeControls  mLastAeControls;
    uint8_t     mLastControlMode;
    uint8_t     mCurrentAeState;
    IntelAEModeBase *mCurrentAeMode;

    IntelAEModeOff mOffMode;
    IntelAEModeAuto mAutoMode;
};

} NAMESPACE_DECLARATION_END
#endif // AAA_INTELAESTATEMACHINE_H_
