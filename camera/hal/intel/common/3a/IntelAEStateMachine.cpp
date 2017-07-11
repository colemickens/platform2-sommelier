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

#define LOG_TAG "AEStateMachine"

#include "IntelAEStateMachine.h"
#include "UtilityMacros.h"
#include "PlatformData.h"

NAMESPACE_DECLARATION {
IntelAEStateMachine::IntelAEStateMachine(int aCameraId):
        mCameraId(aCameraId),
        mLastControlMode(0),
        mCurrentAeState(ANDROID_CONTROL_AE_STATE_INACTIVE),
        mCurrentAeMode(nullptr)
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL1);
    mCurrentAeMode = &mAutoMode;
    CLEAR(mLastAeControls);
    mLastAeControls.aeMode = ANDROID_CONTROL_AE_MODE_ON;
}

IntelAEStateMachine::~IntelAEStateMachine()
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL1);
}

/**
 * Process states in input stage before the AE is run.
 * It is initializing the current state if input
 * parameters have an influence.
 *
 * \param[IN] controlMode: control.controlMode
 * \param[IN] aeControls: set of control.<ae>
 */
status_t
IntelAEStateMachine::processState(const uint8_t &controlMode,
                                  const AeControls &aeControls)
{
    status_t status;

    if (controlMode == ANDROID_CONTROL_MODE_OFF) {
        LOG2("%s: Set AE offMode: controlMode = %s, aeMode = %s", __FUNCTION__,
                        META_CONTROL2STR(mode, controlMode),
                        META_CONTROL2STR(aeMode, aeControls.aeMode));
        mCurrentAeMode = &mOffMode;
    } else {
        if (aeControls.aeMode == ANDROID_CONTROL_AE_MODE_OFF) {
            mCurrentAeMode = &mOffMode;
            LOG2("%s: Set AE offMode: controlMode = %s, aeMode = %s",
                 __FUNCTION__,
                 META_CONTROL2STR(mode, controlMode),
                 META_CONTROL2STR(aeMode, aeControls.aeMode));
        } else {
            LOG2("%s: Set AE AutoMode: controlMode = %s, aeMode = %s",
                 __FUNCTION__,
                 META_CONTROL2STR(mode, controlMode),
                 META_CONTROL2STR(aeMode, aeControls.aeMode));
            mCurrentAeMode = &mAutoMode;
        }
    }

    mLastAeControls = aeControls;
    mLastControlMode = controlMode;
    status = mCurrentAeMode->processState(controlMode, aeControls);
    return status;
}

/**
 * Process results and define output state after the AE is run
 *
 * \param[IN] aeResults: from the ae run
 * \param[IN] results: cameraMetadata to write dynamic tags.
 * \param[IN] reqId: request Id
 */
status_t
IntelAEStateMachine::processResult(const ia_aiq_ae_results &aeResults,
                                   CameraMetadata &result,
                                   uint32_t reqId)
{
    status_t status;

    if (CC_UNLIKELY(mCurrentAeMode == nullptr)) {
        LOGE("Invalid AE mode - this could not happen - BUG!");
        return UNKNOWN_ERROR;
    }

    status =  mCurrentAeMode->processResult(aeResults, result, reqId);
    return status;
}

/******************************************************************************
 * AE MODE   -  BASE
 ******************************************************************************/
IntelAEModeBase::IntelAEModeBase():
    mLastControlMode(0),
    mEvChanged(false),
    mLastAeConvergedFlag(false),
    mAeRunCount(0),
    mAeConvergedCount(0),
    mCurrentAeState(ANDROID_CONTROL_AE_STATE_INACTIVE)
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL1);
    CLEAR(mLastAeControls);
}

void
IntelAEModeBase::updateResult(CameraMetadata &results)
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL2);

    LOG2("%s: current AE state is: %s", __FUNCTION__,
         META_CONTROL2STR(aeState, mCurrentAeState));

    //# METADATA_Dynamic control.aeMode done
    results.update(ANDROID_CONTROL_AE_MODE, &mLastAeControls.aeMode, 1);
    //# METADATA_Dynamic control.aeLock done
    results.update(ANDROID_CONTROL_AE_LOCK, &mLastAeControls.aeLock, 1);
    //# METADATA_Dynamic control.aePrecaptureTrigger done
    results.update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                   &mLastAeControls.aePreCaptureTrigger, 1);
    //# METADATA_Dynamic control.aeAntibandingMode done
    results.update(ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                   &mLastAeControls.aeAntibanding, 1);
    //# METADATA_Dynamic control.aeTargetFpsRange done
    results.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                   &mLastAeControls.aeTargetFpsRange[0], 2);
    //# METADATA_Dynamic control.aeState done
    results.update(ANDROID_CONTROL_AE_STATE, &mCurrentAeState, 1);
}

void
IntelAEModeBase::resetState(void)
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL2);
    mCurrentAeState = ANDROID_CONTROL_AE_STATE_INACTIVE;
    mLastAeConvergedFlag = false;
    mAeRunCount = 0;
    mAeConvergedCount = 0;
}

/******************************************************************************
 * AE MODE   -  OFF
 ******************************************************************************/

IntelAEModeOff::IntelAEModeOff():IntelAEModeBase()
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL1);
}

status_t
IntelAEModeOff::processState(const uint8_t &controlMode,
                             const AeControls &aeControls)
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    mLastAeControls = aeControls;
    mLastControlMode = controlMode;

    if (controlMode == ANDROID_CONTROL_MODE_OFF ||
        aeControls.aeMode == ANDROID_CONTROL_AE_MODE_OFF) {
        resetState();
    } else {
        LOGE("AE State machine should not be OFF! - Fix bug");
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t
IntelAEModeOff::processResult(const ia_aiq_ae_results &aeResults,
                              CameraMetadata &result,
                              uint32_t reqId)
{
    UNUSED(aeResults);
    UNUSED(reqId);
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL2);

    mCurrentAeState = ANDROID_CONTROL_AE_STATE_INACTIVE;
    updateResult(result);

    return OK;
}

/******************************************************************************
 * AE MODE   -  AUTO
 ******************************************************************************/

IntelAEModeAuto::IntelAEModeAuto():IntelAEModeBase()
{
    HAL_TRACE_CALL_PRETTY(CAMERA_DEBUG_LOG_LEVEL1);
}

status_t
IntelAEModeAuto::processState(const uint8_t &controlMode,
                              const AeControls &aeControls)
{
    if(controlMode != mLastControlMode) {
        LOG1("%s: control mode has changed %s -> %s, reset AE State",
             __FUNCTION__,
             META_CONTROL2STR(mode, mLastControlMode),
             META_CONTROL2STR(mode, controlMode));
        resetState();
    }

    if (aeControls.aeLock == ANDROID_CONTROL_AE_LOCK_ON) {
        // If ev compensation changes, we have to let the AE run until
        // convergence. Thus we need to figure out changes in compensation and
        // only change the state immediately to locked,
        // IF the EV did not change.
        if (mLastAeControls.evCompensation != aeControls.evCompensation)
            mEvChanged = true;

        if (!mEvChanged)
            mCurrentAeState = ANDROID_CONTROL_AE_STATE_LOCKED;
    } else if (aeControls.aeMode != mLastAeControls.aeMode) {
        resetState();
    } else {
        switch (mCurrentAeState) {
            case ANDROID_CONTROL_AE_STATE_LOCKED:
                mCurrentAeState = ANDROID_CONTROL_AE_STATE_INACTIVE;
                break;
            case ANDROID_CONTROL_AE_STATE_SEARCHING:
            case ANDROID_CONTROL_AE_STATE_INACTIVE:
            case ANDROID_CONTROL_AE_STATE_CONVERGED:
            case ANDROID_CONTROL_AE_STATE_PRECAPTURE:
                if (aeControls.aePreCaptureTrigger ==
                        ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START)
                    mCurrentAeState = ANDROID_CONTROL_AE_STATE_PRECAPTURE;

                if (aeControls.aePreCaptureTrigger ==
                        ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL)
                    mCurrentAeState = ANDROID_CONTROL_AE_STATE_INACTIVE;
                break;
            default:
                LOGE("Invalid AE state: %d !, State set to INACTIVE", mCurrentAeState);
                mCurrentAeState = ANDROID_CONTROL_AE_STATE_INACTIVE;
            break;
        }
    }
    mLastAeControls = aeControls;
    mLastControlMode = controlMode;
    return OK;
}

status_t
IntelAEModeAuto::processResult(const ia_aiq_ae_results &aeResults,
                               CameraMetadata &result,
                               uint32_t reqId)
{
    switch (mCurrentAeState) {
    case ANDROID_CONTROL_AE_STATE_LOCKED:
        //do nothing
        break;
    case ANDROID_CONTROL_AE_STATE_INACTIVE:
    case ANDROID_CONTROL_AE_STATE_SEARCHING:
    case ANDROID_CONTROL_AE_STATE_CONVERGED:
    case ANDROID_CONTROL_AE_STATE_FLASH_REQUIRED:
        if (aeResults.exposures[0].converged == true) {
            mEvChanged = false; // converged -> reset
            if (mLastAeControls.aeLock) {
                mCurrentAeState = ANDROID_CONTROL_AE_STATE_LOCKED;
            } else {
                if (aeResults.flashes[0].status == ia_aiq_flash_status_torch ||
                    aeResults.flashes[0].status == ia_aiq_flash_status_pre) {
                    mCurrentAeState = ANDROID_CONTROL_AE_STATE_FLASH_REQUIRED;
                } else {
                    mCurrentAeState = ANDROID_CONTROL_AE_STATE_CONVERGED;
                }
            }
        } else {
            mCurrentAeState = ANDROID_CONTROL_AE_STATE_SEARCHING;
        }
        break;
    case ANDROID_CONTROL_AE_STATE_PRECAPTURE:
        if (aeResults.exposures[0].converged == true) {
            mEvChanged = false; // converged -> reset
            if (mLastAeControls.aeLock) {
                mCurrentAeState = ANDROID_CONTROL_AE_STATE_LOCKED;
            } else {
                if (aeResults.flashes[0].status == ia_aiq_flash_status_torch
                        || aeResults.flashes[0].status
                                == ia_aiq_flash_status_pre)
                    mCurrentAeState = ANDROID_CONTROL_AE_STATE_FLASH_REQUIRED;
                else
                    mCurrentAeState = ANDROID_CONTROL_AE_STATE_CONVERGED;
            }
        } // here the else is staying at the same state.
        break;
    default:
        LOGE("Invalid AE state: %d !, State set to INACTIVE", mCurrentAeState);
        mCurrentAeState = ANDROID_CONTROL_AE_STATE_INACTIVE;
        break;
    }

    if (aeResults.exposures[0].converged == true) {
        if (mLastAeConvergedFlag == true) {
            mAeConvergedCount++;
            LOG2("%s: AE converged for %d frames (reqId: %d)",
                        __FUNCTION__, mAeConvergedCount, reqId);
        } else {
            mAeConvergedCount = 1;
            LOG1("%s: AE converging -> converged (reqId: %d)"
                 ", after running AE for %d times",
                        __FUNCTION__, reqId, mAeRunCount);
        }
    } else {
        if (mLastAeConvergedFlag == true) {
            LOG1("%s: AE Converged -> converging (reqId: %d)",
                        __FUNCTION__, reqId);
            mAeRunCount = 1;
            mAeConvergedCount = 0;
        } else {
            mAeRunCount++;
            LOG2("%s: AE converging for %d frames, (reqId: %d.",
                        __FUNCTION__, mAeRunCount, reqId);
        }
    }
    mLastAeConvergedFlag = aeResults.exposures[0].converged;

    updateResult(result);

    return OK;
}
} NAMESPACE_DECLARATION_END
