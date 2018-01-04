/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#ifndef AAA_RK3ACORE_H_
#define AAA_RK3ACORE_H_

#include <rk_aiq.h>
#include <string>
#include <utils/Errors.h>

#include "3ATypes.h"
#include "CameraWindow.h"
#include "UtilityMacros.h"
#include "Rk3aControls.h"

#include "Rk3aAiq.h"

NAMESPACE_DECLARATION {
static const unsigned int NUM_EXPOSURES = 1;   /*!> Number of frames AIQ algorithm
                                                    provides output for */
static const unsigned int COLOR_TRANSFORM_SIZE = 9;

/**
 * \struct AiqInputParams
 */
struct AiqInputParams {
    void init();
    void restorePointers();
    AiqInputParams &operator=(const AiqInputParams &other);
    rk_aiq_ae_input_params  aeInputParams;
    rk_aiq_awb_input_params awbParams;
    rk_aiq_misc_isp_input_params miscParams;
    bool aeLock;
    /* Below structs are part of AE, AWB and misc ISP input parameters. */
    bool awbLock;
    /*
     * Manual color correction.
     */
    rk_aiq_color_channels manualColorGains;
    float manualColorTransform[COLOR_TRANSFORM_SIZE];
    rk_aiq_exposure_sensor_descriptor sensorDescriptor;
    rk_aiq_window exposureWindow;
    rk_aiq_window awbWindow;
    rk_aiq_ae_manual_limits aeManualLimits;
    long manual_exposure_time_us[NUM_EXPOSURES];
    float manual_analog_gain[NUM_EXPOSURES];
    short manual_iso[NUM_EXPOSURES];

    /*!< rk_aiq_awb_input_params pointer contents */
    rk_aiq_awb_manual_cct_range manualCctRange;
};

/**
 * \struct AiqResults
 * The private structs are part of AE, AWB and misc ISP results.
 * They need to be separately introduced to store the contents of the results
 * that the AIQ algorithms return as pointers.
 */
struct AiqResults {
    unsigned long long frame_id;
    rk_aiq_ae_results aeResults;
    rk_aiq_awb_results awbResults;
    rk_aiq_misc_isp_results miscIspResults;
};

struct AeInputParams {
    rk_aiq_exposure_sensor_descriptor   *sensorDescriptor;
    AiqInputParams                      *aiqInputParams;
    AAAControls                         *aaaControls;
    CameraWindow                        *croppingRegion;
    CameraWindow                        *aeRegion;
    int                                 extraEvShift;
    int                                 maxSupportedFps;
    AeInputParams()                     { CLEAR(*this); }
};

struct AwbInputParams {
    AiqInputParams                      *aiqInputParams;
    AAAControls                         *aaaControls;
    AwbInputParams()                    { CLEAR(*this); }
};

/**
 * \class Rk3aCore
 *
 * Rk3aCore is wrapper of Rockchip 3A Library (known as librk_aiq).
 * The purpose of this class is to
 * - call librk_aiq functions such as run 3A functions
 */
class Rk3aCore {
public:
    Rk3aCore(int camId);
    status_t init(const char* xmlFilePath);
    void deinit();

    status_t setStatistics(rk_aiq_statistics_input_params *ispStatistics,
                           rk_aiq_exposure_sensor_descriptor *sensor_desc);

    status_t runAe(rk_aiq_statistics_input_params *ispStatistics,
                   rk_aiq_ae_input_params *aeInputParams,
                   rk_aiq_ae_results *aeResults);

    status_t runAwb(rk_aiq_statistics_input_params *ispStatistics,
                    rk_aiq_awb_input_params *awbInputParams,
                    rk_aiq_awb_results *awbResults);

    status_t runMisc(rk_aiq_statistics_input_params  *ispStatistics,
                     rk_aiq_misc_isp_input_params *miscInputParams,
                     rk_aiq_misc_isp_results *miscResults);

private:
    // prevent copy constructor and assignment operator
    Rk3aCore(const Rk3aCore& other);
    Rk3aCore& operator=(const Rk3aCore& other);

// private members
private:
    int mCameraId;

private:
    Rk3aAiq mAiq;
}; //  class Rk3aCore

} NAMESPACE_DECLARATION_END
#endif  //  AAA_RK3ACORE_H_
