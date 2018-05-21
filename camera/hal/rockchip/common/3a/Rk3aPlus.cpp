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

#define LOG_TAG "Rk3aPlus"

#include <utils/Errors.h>
#include <math.h>
#include <limits.h> // SCHAR_MAX, SCHAR_MIN, INT_MAX
#include <sys/stat.h>

#include "PlatformData.h"
#include "CameraMetadataHelper.h"
#include "LogHelper.h"
#include "Utils.h"
#include "Rk3aPlus.h"

NAMESPACE_DECLARATION {
#define RK_3A_TUNING_FILE_PATH  "/etc/camera/rkisp1/"

Rk3aPlus::Rk3aPlus(int camId):
        Rk3aCore(camId),
        mCameraId(camId),
        mMinFocusDistance(0.0f),
        mMinAeCompensation(0),
        mMaxAeCompensation(0),
        mMinSensitivity(0),
        mMaxSensitivity(0),
        mMinExposureTime(0),
        mMaxExposureTime(0),
        mMaxFrameDuration(0)
{
    LOG1("@%s", __FUNCTION__);
}


status_t Rk3aPlus::initAIQ(const char* sensorName)
{
    status_t status = OK;
    /* get AIQ xml path */
    const CameraCapInfo* cap = PlatformData::getCameraCapInfo(mCameraId);
    std::string iq_file = cap->getIqTuningFile();
    std::string iq_file_path(RK_3A_TUNING_FILE_PATH);
    std::string iq_file_full_path = iq_file_path + iq_file;
    struct stat fileInfo;

    CLEAR(fileInfo);
    if (stat(iq_file_full_path .c_str(), &fileInfo) < 0) {
        if (errno == ENOENT) {
            LOGI("sensor tuning file missing: \"%s\"!", sensorName);
            return NAME_NOT_FOUND;
        } else {
            LOGE("ERROR querying sensor tuning filestat for \"%s\": %s!",
                 iq_file_full_path.c_str(), strerror(errno));
            return UNKNOWN_ERROR;
        }
    }

    status = init(iq_file_full_path.c_str());

    if (status == OK) {
        //TODO: do other init
    }

    return status;
}

rk_aiq_frame_use
Rk3aPlus::getFrameUseFromIntent(const CameraMetadata * settings)
{
    camera_metadata_ro_entry entry;
    rk_aiq_frame_use frameUse = rk_aiq_frame_use_preview;
    //# METADATA_Control control.captureIntent done
    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count == 1) {
        uint8_t captureIntent = entry.data.u8[0];
        switch (captureIntent) {
            case ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM:
                frameUse = rk_aiq_frame_use_preview;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
                frameUse = rk_aiq_frame_use_preview;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
                frameUse = rk_aiq_frame_use_still;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
                frameUse = rk_aiq_frame_use_video;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
                frameUse = rk_aiq_frame_use_video;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
                frameUse = rk_aiq_frame_use_continuous;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
                frameUse = rk_aiq_frame_use_still;
                break;
            default:
                LOGE("ERROR @%s: Unknow frame use %d", __FUNCTION__, captureIntent);
                break;
        }
    }
    return frameUse;
}

/**
 * Parses the request setting to find one of the 3 metering regions
 *
 * CONTROL_AE_REGIONS
 * CONTROL_AWB_REGIONS
 * CONTROL_AF_REGIONS
 *
 * It then initializes a CameraWindow structure. If no metering region is found
 * the CameraWindow is initialized empty. Users of this method can check this
 * by calling CameraWindow::isValid().
 *
 * \param[in] settings request settings to parse
 * \param[in] tagID one of the 3 metadata tags for the metering regions
 *                   (AE,AWB or AF)
 * \param[out] meteringWindow initialized region.
 *
 */
void Rk3aPlus::parseMeteringRegion(const CameraMetadata *settings,
                                   int tagId, CameraWindow *meteringWindow)
{
    camera_metadata_ro_entry_t entry;
    ia_coordinate topLeft, bottomRight;
    CLEAR(topLeft);
    CLEAR(bottomRight);
    int weight = 0;

    if (tagId == ANDROID_CONTROL_AE_REGIONS ||
        tagId == ANDROID_CONTROL_AWB_REGIONS ||
        tagId == ANDROID_CONTROL_AF_REGIONS) {
        entry = settings->find(tagId);
        if (entry.count >= 5) {
            topLeft.x = entry.data.i32[0];
            topLeft.y = entry.data.i32[1];
            bottomRight.x = entry.data.i32[2];
            bottomRight.y = entry.data.i32[3];
            weight = entry.data.i32[4];
            // TODO support more than one metering region
        } else
            LOGE("invalid control entry count %d", entry.count);
    } else {
        LOGE("Unsupported tag ID (%d) is given", tagId);
    }

    meteringWindow->init(topLeft, bottomRight, weight);
}

/**
 * \brief Converts AE related metadata into AeInputParams
 *
 * \param[in]  settings request settings in Google format
 * \param[out] aeInputParams all parameters for ae processing
 *
 * \return success or error.
 */
status_t Rk3aPlus::fillAeInputParams(const CameraMetadata *settings,
                                     struct AeInputParams *aeInputParams)
{
    LOG2("@%s", __FUNCTION__);

    if (aeInputParams->sensorDescriptor == nullptr || settings == nullptr) {
        LOGE("%s: invalid sensorDescriptor %p settings %p!", __FUNCTION__,
             aeInputParams->sensorDescriptor, settings);
        return BAD_VALUE;
    }

    AeControls *aeCtrl = &aeInputParams->aaaControls->ae;
    AiqInputParams *aiqInputParams = aeInputParams->aiqInputParams;
    rk_aiq_exposure_sensor_descriptor *sensorDescriptor;

    sensorDescriptor = aeInputParams->sensorDescriptor;

    if (aeCtrl == nullptr || aiqInputParams == nullptr || sensorDescriptor == nullptr) {
        LOGE("one input parameter is nullptr");
        LOGE("aeCtrl = %p, aiqInputParams = %p, sensorDescriptor = %p", aeCtrl,
                                                                        aiqInputParams,
                                                                        sensorDescriptor);
        return UNKNOWN_ERROR;
    }

    //# METADATA_Control control.aeLock done
    camera_metadata_ro_entry entry = settings->find(ANDROID_CONTROL_AE_LOCK);
    if (entry.count == 1) {
        aeCtrl->aeLock = entry.data.u8[0];
        aiqInputParams->aeLock = (aeCtrl->aeLock == ANDROID_CONTROL_AE_LOCK_ON);
    }

    // num_exposures
    aiqInputParams->aeParams.num_exposures = NUM_EXPOSURES;

    /* frame_use
     *  BEWARE - THIS VALUE WILL NOT WORK WITH AIQ WHICH RUNS PRE-CAPTURE
     *  WITH STILL FRAME_USE, WHILE THE HAL GETS PREVIEW INTENTS DURING PRE-
     *  CAPTURE!!!
     */
    aiqInputParams->aeParams.frame_use = getFrameUseFromIntent(settings);

    // ******** manual_limits (defaults)
    aiqInputParams->aeParams.manual_limits->manual_exposure_time_us_min = -1;
    aiqInputParams->aeParams.manual_limits->manual_exposure_time_us_max = -1;
    aiqInputParams->aeParams.manual_limits->manual_frame_time_us_min = -1;
    aiqInputParams->aeParams.manual_limits->manual_frame_time_us_max = -1;
    aiqInputParams->aeParams.manual_limits->manual_iso_min = -1;
    aiqInputParams->aeParams.manual_limits->manual_iso_max = -1;

    // ******** flash_mode is unsupported now, so set ture off to aiq parameter.
    aiqInputParams->aeParams.flash_mode = rk_aiq_flash_mode_off;

    //# METADATA_Control control.mode done
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        uint8_t controlMode = entry.data.u8[0];

        aeInputParams->aaaControls->controlMode = controlMode;
        if (controlMode == ANDROID_CONTROL_MODE_AUTO ||
            controlMode == ANDROID_CONTROL_MODE_USE_SCENE_MODE)
            aiqInputParams->aeParams.operation_mode = rk_aiq_ae_operation_mode_automatic;
        else
            aiqInputParams->aeParams.operation_mode = rk_aiq_ae_operation_mode_off;
    }

    // ******** metering_mode
    // TODO: implement the metering mode. For now the metering mode is fixed
    // to whole frame
    aiqInputParams->aeParams.metering_mode = rk_aiq_ae_metering_mode_evaluative;

    // ******** priority_mode
    // TODO: check if there is something that can be mapped to the priority mode
    // maybe NIGHT_PORTRAIT to highlight for example?
    aiqInputParams->aeParams.priority_mode = rk_aiq_ae_priority_mode_normal;

    // ******** flicker_reduction_mode
    //# METADATA_Control control.aeAntibandingMode done
    entry = settings->find(ANDROID_CONTROL_AE_ANTIBANDING_MODE);
    if (entry.count == 1) {
        uint8_t flickerMode = entry.data.u8[0];
        aeCtrl->aeAntibanding = flickerMode;

        switch (flickerMode) {
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
                aiqInputParams->aeParams.flicker_reduction_mode = rk_aiq_ae_flicker_reduction_off;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
                aiqInputParams->aeParams.flicker_reduction_mode = rk_aiq_ae_flicker_reduction_50hz;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
                aiqInputParams->aeParams.flicker_reduction_mode = rk_aiq_ae_flicker_reduction_60hz;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
                aiqInputParams->aeParams.flicker_reduction_mode = rk_aiq_ae_flicker_reduction_auto;
                break;
            default:
                LOGE("ERROR @%s: Unknow flicker mode %d", __FUNCTION__, flickerMode);
                return BAD_VALUE;
        }
    }

    *aiqInputParams->aeParams.sensor_descriptor = *sensorDescriptor;

    CameraWindow *aeRegion = aeInputParams->aeRegion;
    CameraWindow *croppingRegion = aeInputParams->croppingRegion;
    if (aeRegion && croppingRegion) {
        // ******** exposure_window
        //# METADATA_Control control.aeRegions done
        parseMeteringRegion(settings, ANDROID_CONTROL_AE_REGIONS, aeRegion);
        if (aeRegion->isValid()) {
            // Clip the region to the crop rectangle
            if (croppingRegion->isValid())
                aeRegion->clip(*croppingRegion);

            aiqInputParams->aeParams.window->h_offset = aeRegion->left();
            aiqInputParams->aeParams.window->v_offset = aeRegion->top();
            aiqInputParams->aeParams.window->width = aeRegion->width();
            aiqInputParams->aeParams.window->height = aeRegion->height();
        }
    }
    // ******** exposure_coordinate
    /*
     * MANUAL AE CONTROL
     */
    if (aeInputParams->aaaControls->controlMode == ANDROID_CONTROL_MODE_OFF ||
        aeCtrl->aeMode == ANDROID_CONTROL_AE_MODE_OFF) {
        // ******** manual_exposure_time_us
        //# METADATA_Control sensor.exposureTime done
        entry = settings->find(ANDROID_SENSOR_EXPOSURE_TIME);
        if (entry.count == 1) {
            int64_t timeMicros = entry.data.i64[0] / 1000;
            if (timeMicros > 0) {
                if (timeMicros > mMaxExposureTime / 1000) {
                    LOGE("exposure time %" PRId64 " ms is bigger than the max exposure time %" PRId64 " ms",
                        timeMicros, mMaxExposureTime / 1000);
                    return BAD_VALUE;
                } else if (timeMicros < mMinExposureTime / 1000) {
                    LOGE("exposure time %" PRId64 " ms is smaller than the min exposure time %" PRId64 " ms",
                        timeMicros, mMinExposureTime / 1000);
                    return BAD_VALUE;
                }
                aiqInputParams->aeParams.manual_exposure_time_us[0] =
                  (int)timeMicros;
                aiqInputParams->aeParams.manual_limits->
                  manual_exposure_time_us_min = (int)timeMicros;
                aiqInputParams->aeParams.manual_limits->
                  manual_exposure_time_us_max = (int)timeMicros;
            } else {
                // Don't constrain AIQ.
                aiqInputParams->aeParams.manual_exposure_time_us = nullptr;
                aiqInputParams->aeParams.manual_limits->
                    manual_exposure_time_us_min = -1;
                aiqInputParams->aeParams.manual_limits->
                    manual_exposure_time_us_max = -1;
            }
        }

        // ******** manual frame time --> frame rate
        //# METADATA_Control sensor.frameDuration done
        entry = settings->find(ANDROID_SENSOR_FRAME_DURATION);
        if (entry.count == 1) {
            int64_t timeMicros = entry.data.i64[0] / 1000;
            if (timeMicros > 0) {
                if (timeMicros > mMaxFrameDuration / 1000) {
                    LOGE("frame duration %" PRId64 " ms is bigger than the max frame duration %" PRId64 " ms",
                        timeMicros, mMaxFrameDuration / 1000);
                    return BAD_VALUE;
                }
                aiqInputParams->aeParams.manual_limits->
                  manual_frame_time_us_min = (int)timeMicros;
                aiqInputParams->aeParams.manual_limits->
                  manual_frame_time_us_max = (int)timeMicros;
            } else {
                // Don't constrain AIQ.
                aiqInputParams->aeParams.manual_limits->
                    manual_frame_time_us_min = -1;
                aiqInputParams->aeParams.manual_limits->
                    manual_frame_time_us_max = -1;
            }
        }
        // ******** manual_analog_gain
        aiqInputParams->aeParams.manual_analog_gain = nullptr;

        // ******** manual_iso
        //# METADATA_Control sensor.sensitivity done
        entry = settings->find(ANDROID_SENSOR_SENSITIVITY);
        if (entry.count == 1) {
            int32_t iso = entry.data.i32[0];
            if (iso >= mMinSensitivity && iso <= mMaxSensitivity) {
                aiqInputParams->aeParams.manual_iso[0] = iso;
                aiqInputParams->aeParams.manual_limits->
                    manual_iso_min = aiqInputParams->aeParams.manual_iso[0];
                aiqInputParams->aeParams.manual_limits->
                    manual_iso_max = aiqInputParams->aeParams.manual_iso[0];
            } else
                aiqInputParams->aeParams.manual_iso = nullptr;
        }
        // fill target fps range, it needs to be proper in results anyway
        entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        if (entry.count == 2) {
            aeCtrl->aeTargetFpsRange[0] = entry.data.i32[0];
            aeCtrl->aeTargetFpsRange[1] = entry.data.i32[1];
        }

    } else {
        /*
         *  AUTO AE CONTROL
         */
        // ******** ev_shift
        //# METADATA_Control control.aeExposureCompensation done
        entry = settings->find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
        if (entry.count == 1) {
            int32_t evCompensation = CLIP(entry.data.i32[0] + aeInputParams->extraEvShift,
                                          mMaxAeCompensation, mMinAeCompensation);

            aeCtrl->evCompensation = evCompensation;

            float step = PlatformData::getStepEv(mCameraId);
            aiqInputParams->aeParams.ev_shift = evCompensation * step;
        } else {
            aiqInputParams->aeParams.ev_shift = 0.0f;
        }
        aiqInputParams->aeParams.manual_exposure_time_us = nullptr;
        aiqInputParams->aeParams.manual_analog_gain = nullptr;
        aiqInputParams->aeParams.manual_iso = nullptr;

        // ******** target fps
        int32_t maxSupportedFps = INT_MAX;
        if (aeInputParams->maxSupportedFps != 0)
            maxSupportedFps = aeInputParams->maxSupportedFps;
        //# METADATA_Control control.aeTargetFpsRange done
        entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        if (entry.count == 2) {
            int32_t minFps = MIN(entry.data.i32[0], maxSupportedFps);
            int32_t maxFps = MIN(entry.data.i32[1], maxSupportedFps);

            aeCtrl->aeTargetFpsRange[0] = minFps;
            aeCtrl->aeTargetFpsRange[1] = maxFps;

            aiqInputParams->aeParams.manual_limits->manual_frame_time_us_min =
                    (long) ((1.0f / maxFps) * 1000000);
            aiqInputParams->aeParams.manual_limits->manual_frame_time_us_max =
                    (long) ((1.0f / minFps) * 1000000);
        }

        //# METADATA_Control control.aePrecaptureTrigger done
        entry = settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
        if (entry.count == 1) {
            aeCtrl->aePreCaptureTrigger = entry.data.u8[0];
        }
    }
    return NO_ERROR;
}

/**
 * fillAwbInputParams
 *
 * Converts the capture request settings in format into input parameters
 * for the AWB algorithm and Parameter Adaptor that is in charge of color
 * correction.
 *
 * It also provides the AWB mode that is used in PSL code.
 * we do the parsing here so that it is done only once.
 *
 * \param[in] settings: Camera metadata with the capture request settings.
 * \param[out] awbInputParams: parameters for aiq and other awb processing.
 *
 * \return BAD_VALUE if settings was nullptr.
 * \return NO_ERROR in normal situation.
 */
status_t Rk3aPlus::fillAwbInputParams(const CameraMetadata *settings,
                                      AwbInputParams *awbInputParams)
{
    rk_aiq_awb_input_params *awbCfg;
    AwbControls *awbCtrl;

    if (settings == nullptr
        || awbInputParams->aaaControls == nullptr
        || awbInputParams->aiqInputParams == nullptr) {
        LOGE("Input Param is nullptr!");
        LOGE("settings = %p, aaaControl = %p, aiqInput = %p",
              settings, awbInputParams->aaaControls,
              awbInputParams->aiqInputParams);

        return BAD_VALUE;
    }

    awbCfg = &awbInputParams->aiqInputParams->awbParams;
    awbCtrl = &awbInputParams->aaaControls->awb;

    // AWB lock
    camera_metadata_ro_entry entry;
    //# METADATA_Control control.awbLock done
    entry = settings->find(ANDROID_CONTROL_AWB_LOCK);
    if (entry.count == 1) {
        awbCtrl->awbLock = entry.data.u8[0];
        if (awbCtrl->awbLock == ANDROID_CONTROL_AWB_LOCK_ON)
            awbInputParams->aiqInputParams->awbLock = true;
    }

    /* frame_use
     *  BEWARE - THIS VALUE MAY NOT WORK WITH AIQ WHICH RUNS PRE-CAPTURE
     *  WITH STILL FRAME_USE, WHILE THE HAL GETS PREVIEW INTENTS DURING PRE-
     *  CAPTURE!!!
     */
    awbCfg->frame_use = getFrameUseFromIntent(settings);

    awbCfg->manual_cct_range = nullptr;

    CameraWindow awbRegion;
    //# METADATA_Control control.awbRegion done
    entry = settings->find(ANDROID_CONTROL_AWB_REGIONS);
    if (entry.count) {
        parseMeteringRegion(settings, ANDROID_CONTROL_AWB_REGIONS, &awbRegion);
    }

    if (awbRegion.isValid()) {
        awbCfg->window->h_offset = awbRegion.left();
        awbCfg->window->v_offset = awbRegion.top();
        awbCfg->window->width = awbRegion.width();
        awbCfg->window->height = awbRegion.height();
    } else {
        awbCfg->window = nullptr;
    }
    /*
     * MANUAL COLOR CORRECTION
     */
    awbCtrl->colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_FAST;
    //# METADATA_Control colorCorrection.mode done
    entry = settings->find(ANDROID_COLOR_CORRECTION_MODE);
    if (entry.count == 1) {
        awbCtrl->colorCorrectionMode = entry.data.u8[0];
    }

    awbCtrl->colorCorrectionAberrationMode = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST;
    //# METADATA_Control colorCorrection.aberrationMode done
    entry = settings->find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
    if (entry.count == 1) {
        awbCtrl->colorCorrectionAberrationMode = entry.data.u8[0];
    }

    // if awbMode is not OFF, then colorCorrection mode TRANSFORM_MATRIX should
    // be ignored and overwrittern to FAST.
    if (awbCtrl->awbMode != ANDROID_CONTROL_AWB_MODE_OFF &&
        awbCtrl->colorCorrectionMode == ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX) {
        awbCtrl->colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_FAST;
    }

    if (awbCtrl->awbMode == ANDROID_CONTROL_AWB_MODE_OFF) {
        //# METADATA_Control colorCorrection.transform done
        entry = settings->find(ANDROID_COLOR_CORRECTION_TRANSFORM);
        if (entry.count == 9) {
            for (size_t i = 0; i < entry.count; i++) {
                int32_t numerator = entry.data.r[i].numerator;
                int32_t denominator = entry.data.r[i].denominator;
                awbInputParams->aiqInputParams->manualColorTransform[i] = (float) numerator / denominator;
            }
        }

        //# METADATA_Control colorCorrection.gains done
        entry = settings->find(ANDROID_COLOR_CORRECTION_GAINS);
        if (entry.count == 4) {
            // The color gains from application is in order of RGGB
            awbInputParams->aiqInputParams->manualColorGains.r = entry.data.f[0];
            awbInputParams->aiqInputParams->manualColorGains.gr = entry.data.f[1];
            awbInputParams->aiqInputParams->manualColorGains.gb = entry.data.f[2];
            awbInputParams->aiqInputParams->manualColorGains.b = entry.data.f[3];
        }
    }

    //# METADATA_Control control.awbRegions done
    //# METADATA_Dynamic control.awbRegions done
    //# AM Not Supported by 3a
    return NO_ERROR;
}
} NAMESPACE_DECLARATION_END

