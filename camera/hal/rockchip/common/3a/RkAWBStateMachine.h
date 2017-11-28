/*
 * Copyright (C) 2015-2017 Intel Corporation
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
#ifndef AAA_RKAWBSTATEMACHINE_H_
#define AAA_RKAWBSTATEMACHINE_H_

#include <utils/Errors.h>
#include <camera/camera_metadata.h>
#include "LogHelper.h"
#include "Rk3aPlus.h"

NAMESPACE_DECLARATION {
/**
 * \class RkAWBModeBase
 *
 * Base class for all the Auto white balance modes as defined by the Android
 * camera device V3.x API.
 * Each mode will follow certain state transitions. See documentation for
 * android.control.awbState
 *
 */
class RkAWBModeBase {
public:
    RkAWBModeBase();
    virtual ~RkAWBModeBase() {};

    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls) = 0;


    virtual status_t processResult(const rk_aiq_awb_results &awbResults,
                                   CameraMetadata &results) = 0;

    void resetState(void);
    uint8_t getState() const { return mCurrentAwbState; }
protected:
    void updateResult(CameraMetadata& results);
protected:
    AwbControls  mLastAwbControls;
    uint8_t     mLastControlMode;
    uint8_t     mCurrentAwbState;
};

/**
 * \class RkAWBModeAuto
 * Derived class from RkAWBModeBase for Auto mode
 *
 */
class RkAWBModeAuto: public RkAWBModeBase {
public:
    RkAWBModeAuto();
    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls);
    virtual status_t processResult(const rk_aiq_awb_results &awbResults,
                                  CameraMetadata& result);
};

/**
 * \class RkAWBModeOFF
 * Derived class from RkAWBModeBase for OFF mode
 *
 */
class RkAWBModeOff: public RkAWBModeBase {
public:
    RkAWBModeOff();
    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls);
    virtual status_t processResult(const rk_aiq_awb_results &awbResults,
                                  CameraMetadata& result);
};

/**
 * \class RkAWBStateMachine
 *
 * This class adapts the Android V3 AWB triggers and state transitions to
 * the ones implemented by the Rockchip AIQ algorithm
 * This class is platform independent. Platform specific behaviors should be
 * implemented in derived classes from this one or from the RkAWBModeBase
 *
 */
class RkAWBStateMachine {
public:
    RkAWBStateMachine(int CameraId);
    virtual ~RkAWBStateMachine();

    virtual status_t processState(const uint8_t &controlMode,
                                  const AwbControls &awbControls);

    virtual status_t processResult(const rk_aiq_awb_results &awbResults,
                                   CameraMetadata &results);

    uint8_t getState() const { return mCurrentAwbMode->getState(); }
private:
    // prevent copy constructor and assignment operator
    RkAWBStateMachine(const RkAWBStateMachine& other);
    RkAWBStateMachine& operator=(const RkAWBStateMachine& other);

private: /* members*/
    int mCameraId;
    AwbControls  mLastAwbControls;
    uint8_t     mLastControlMode;
    uint8_t     mCurrentAwbState;
    RkAWBModeBase *mCurrentAwbMode;

    RkAWBModeOff mOffMode;
    RkAWBModeAuto mAutoMode;
};
} NAMESPACE_DECLARATION_END

#endif // AAA_RKAWBSTATEMACHINE_H_
