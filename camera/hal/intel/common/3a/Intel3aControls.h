/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#ifndef AAA_INTEL3ACONTROLS_H_
#define AAA_INTEL3ACONTROLS_H_

namespace android {
namespace camera2 {
/**
 * \struct AeControls
 *
 * Control Modes saved and passed back to control unit after reading
 *
 */
struct AeControls {
    uint8_t aeMode;                 /**< AE_MODE */
    uint8_t aeLock;                 /**< AE_LOCK */
    uint8_t aePreCaptureTrigger;    /**< PRECAPTURE_TRIGGER */
    uint8_t aeAntibanding;          /**< AE_ANTIBANDING */
    int32_t evCompensation;         /**< AE_EXPOSURE_COMPENSATION */
    int32_t aeTargetFpsRange[2];    /**< AE_TARGET_FPS_RANGE */
};

/**
 * \struct AwbControls
 *
 * Control Modes saved and passed back to control unit after reading
 *
 */
struct AwbControls {
    uint8_t awbMode;                        /**< AWB_MODE */
    uint8_t awbLock;                        /**< AWB_LOCK */
    uint8_t colorCorrectionMode;            /**< COLOR_CORRECTION_MODE */
    uint8_t colorCorrectionAberrationMode;  /**< COLOR_CORRECTION_ABERRATION_MODE */
};

/**
 * \struct AfControls
 *
 * Control Modes saved and passed back to control unit after reading
 *
 */
struct AfControls {
    uint8_t afMode; /**< AF_MODE */
    uint8_t afTrigger; /**< AF_TRIGGER */
};

struct AAAControls {
    uint8_t controlMode;    /**< MODE */
    AeControls  ae;
    AfControls  af;
    AwbControls awb;
};

} /* namespace camera2 */
} /* namespace android */
#endif //AAA_INTEL3ACONTROLS_H_
