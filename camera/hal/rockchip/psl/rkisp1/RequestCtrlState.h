/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#ifndef PSL_RKISP1_REQUESTCTRLSTATE_H_
#define PSL_RKISP1_REQUESTCTRLSTATE_H_

#include "Camera3Request.h"
#include "CaptureUnitSettings.h"
#include "GraphConfig.h"
#include "ProcUnitSettings.h"
#include "CaptureUnit.h"
#include "Rk3aPlus.h"

namespace android {
namespace camera2 {

#define TONEMAP_MAX_CURVE_POINTS    1024

/**
 * \enum AlgorithmState
 * Describes the state for all the camera control algorithms (AE, AWB)
 * in ControlUnit.
 */
enum AlgorithmState {
    ALGORITHM_NOT_CONFIG,   /**< init state  */
    ALGORITHM_CONFIGURED,   /**< request is analyzed AIQ is configured  */
    ALGORITHM_READY,        /**< input parameters READY */
    ALGORITHM_RUN           /**< algorithm run already, output settings
                                 available */
};

/**
 * \struct RequestCtrlState
 * Contains the AIQ configuration derived from analyzing the user request
 * settings. This configuration will be applied before running 3A algorithms
 * It also tracks the status of each algorithm for this request
 */
struct RequestCtrlState {
    void init(Camera3Request *req, std::shared_ptr<GraphConfig> aGraphConfig);
    static void reset(RequestCtrlState* me);
    void initAAAControls();
    CameraMetadata *ctrlUnitResult; /**< metadata results written in the
                                         context of the ControlUnit */
    Camera3Request *request;        /**< user request associated to this AIQ
                                         configuration */

    AiqInputParams      aiqInputParams;
    AAAControls   aaaControls;

    std::shared_ptr<CaptureUnitSettings> captureSettings; /**< Results from 3A calculations */
    std::shared_ptr<ProcUnitSettings> processingSettings; /**< Per request parameters
                                                        for the processing unit */
    std::shared_ptr<GraphConfig> graphConfig; /**< Details of the graph configuration for this request */

    AlgorithmState  aeState;
    AlgorithmState  awbState;

    bool tonemapContrastCurve;
    float rGammaLut[TONEMAP_MAX_CURVE_POINTS];
    float gGammaLut[TONEMAP_MAX_CURVE_POINTS];
    float bGammaLut[TONEMAP_MAX_CURVE_POINTS];
    unsigned int rGammaLutSize;
    unsigned int gGammaLutSize;
    unsigned int bGammaLutSize;

    bool statsArrived;
    uint8_t framesArrived;
    bool shutterDone;
    bool blackLevelOff;

    uint8_t androidAeState;  /**< Current AE state, based on the
                                  AE settings and AE results */
    uint8_t intent;          /**< Capture intent, needed for precapture */
    bool analysisEnabled;    /**< Enables/disables:
                                      - multi frame hint
                                      - smart scene detection
                                      - HDR preferred exposures */
};

} // namespace camera2
} // namespace android

#endif /* PSL_RKISP1_REQUESTCTRLSTATE_H_ */
