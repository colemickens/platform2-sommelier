/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#define LOG_TAG "Intel3aPlus"

#include "Intel3aPlus.h"

#include <utils/Errors.h>
#include <math.h>
#include <ia_exc.h>
#include <limits.h> // SCHAR_MAX, SCHAR_MIN, INT_MAX

#include "PlatformData.h"
#include "CameraMetadataHelper.h"
#include "LogHelper.h"
#include "Utils.h"

NAMESPACE_DECLARATION {
const float EPSILON = 0.00001f;


Intel3aPlus::Intel3aPlus(int camId):
        Intel3aCore(camId),
        mCameraId(camId),
        mMinFocusDistance(0.0f),
        mMinAeCompensation(0),
        mMaxAeCompensation(0),
        mMinSensitivity(0),
        mMaxSensitivity(0),
        mMinExposureTime(0),
        mMaxExposureTime(0),
        mMaxFrameDuration(0),
        mPseudoIsoRatio(1.0),
        mSupportIsoMap(false)
{
    LOG1("@%s", __FUNCTION__);
}


status_t Intel3aPlus::initAIQ(int maxGridW,
                              int maxGridH,
                              ia_binary_data nvmData,
                              const char* sensorName)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    status = init(maxGridW, maxGridH, nvmData, sensorName);
    CheckError(status != NO_ERROR, status, "@%s, init() fails", __FUNCTION__);

    /**
     * Cache all the values we are going to need from the static metadata
     */
    const camera_metadata_t *currentMeta = nullptr;
    currentMeta = PlatformData::getStaticMetadata(mCameraId);
    mAvailableAFModes.clear();
    camera_metadata_ro_entry_t ro_entry;
    CLEAR(ro_entry);

    /**
     * available AF modes
     */
    find_camera_metadata_ro_entry(currentMeta,
               ANDROID_CONTROL_AF_AVAILABLE_MODES, &ro_entry);
    for (size_t i = 0; i < ro_entry.count; i++) {
        mAvailableAFModes.push_back(ro_entry.data.u8[i]);
    }

    find_camera_metadata_ro_entry(currentMeta,
            ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &ro_entry);
    if (ro_entry.count == 1)
        mMinFocusDistance = ro_entry.data.f[0];

    find_camera_metadata_ro_entry(currentMeta,
            ANDROID_CONTROL_AE_COMPENSATION_RANGE, &ro_entry);
    if (ro_entry.count == 2) {
        mMinAeCompensation = ro_entry.data.i32[0];
        mMaxAeCompensation = ro_entry.data.i32[1];
    }

    /* get minimum sensitivity */
    int count = 0;
    int *sensitivityRange = (int32_t*)MetadataHelper::getMetadataValues(
                                      currentMeta,
                                      ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
                                      TYPE_INT32,
                                      &count);
    if (count >= 2 && sensitivityRange != nullptr) {
        mMinSensitivity = sensitivityRange[0];
        mMaxSensitivity = sensitivityRange[1];
        LOG2("mMinSensitivity:%d mMaxSensitivity:%d", mMinSensitivity, mMaxSensitivity);
    }

    /* get min/max exposure time */
    count = 0;
    int64_t *exposureTimeRange = (int64_t*)MetadataHelper::getMetadataValues(
                                      currentMeta,
                                      ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE,
                                      TYPE_INT64,
                                      &count);
    if (count >= 2 && exposureTimeRange != nullptr) {
        mMinExposureTime = exposureTimeRange[0];
        mMaxExposureTime = exposureTimeRange[1];
        LOG2("mMinExposureTime:%" PRId64 " mMaxExposureTime:%" PRId64, mMinExposureTime, mMaxExposureTime);
    }

    /* get max frame duration */
    count = 0;
    int64_t *maxFrameDuration = (int64_t*)MetadataHelper::getMetadataValues(
                                      currentMeta,
                                      ANDROID_SENSOR_INFO_MAX_FRAME_DURATION,
                                      TYPE_INT64,
                                      &count);
    if (count >= 1 && maxFrameDuration != nullptr) {
        mMaxFrameDuration = *maxFrameDuration;
        LOG2("mMaxFrameDuration:%" PRId64, mMaxFrameDuration);
    }

    return status;
}

/**
 * setSupportIsoMap
 * Called with value true if iso mapping shall be used
 */
void Intel3aPlus::setSupportIsoMap(bool support)
{
    mSupportIsoMap = support;

    if (mSupportIsoMap)
        initIsoMappingRatio();
}

/**
 * init mPseudoIsoRatio
 * mPseudoIsoRatio is used for mapping between UI ISO and real ISO
 * mPseudoIsoRatio = (maxSensitivity - mMinSensitivity) / (maxIso - baseISO);
 * the mapping method is
 * (UI ISO - mMinSensitivity) / (real ISO -baseISO)
 * = (maxSensitivity - mMinSensitivity) / (maxIso - baseISO);
 */
void Intel3aPlus::initIsoMappingRatio()
{
    if (mSupportIsoMap == false)
        return;

    const ia_cmc_t* cmc = getCmc();
    CheckError(cmc == nullptr, VOID_VALUE, "@%s, cmc is nullptr", __FUNCTION__);
    CheckError(cmc->cmc_sensitivity == nullptr, VOID_VALUE, "@%s, cmc_sensitivity is nullptr", __FUNCTION__);

    /* Get base/module ISO */
    int32_t baseIso = cmc->cmc_sensitivity->base_iso;

    /* Get the max analog sensitivity */
    double maxAnalogIso = 0;
    if (cmc->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion != nullptr) {
        float maxAnalogGain = 0.0;
        unsigned short analogGainCode = 0;
        mExc.AnalogGainToSensorUnits(&(cmc->cmc_parsed_analog_gain_conversion),
                                           1000, &analogGainCode);
        mExc.SensorUnitsToAnalogGain(&(cmc->cmc_parsed_analog_gain_conversion),
                                           analogGainCode,
                                           &maxAnalogGain);
        // caclulate the iso base on max analog gain
        maxAnalogIso = maxAnalogGain * baseIso;
    }

    /* Get the max digital gain */
    double maxDigitalGain = 1.0;
    if (cmc->cmc_parsed_digital_gain.cmc_digital_gain != nullptr) {
        int fractionBits =
            cmc->cmc_parsed_digital_gain.cmc_digital_gain->digital_gain_fraction_bits;
        maxDigitalGain =
            cmc->cmc_parsed_digital_gain.cmc_digital_gain->digital_gain_max / pow(2, fractionBits);
    }

    double maxIso = maxAnalogIso * maxDigitalGain;
    if (maxIso - baseIso > EPSILON) {
        mPseudoIsoRatio = ((double)(mMaxSensitivity - mMinSensitivity)) / (maxIso - baseIso);
    } else {
        LOGE("Max ISO is not greater than base ISO, configuration error!");
        mSupportIsoMap = false;  // Disable ISO map mode.
    }

    LOG2("%s: maxAnalogIso: %lf, maxDigitalGain: %lf, baseIso: %d, mPseudoIsoRatio: %lf",
         __FUNCTION__, maxAnalogIso, maxDigitalGain, baseIso, mPseudoIsoRatio);
}

/**
 * get the real ISO AIQ supported
 * mapping as following
 * (UI ISO - mMinSensitivity) /(real ISO -baseISO)
 * = (maxSensitivity - mMinSensitivity) / (maxIso - baseISO)
 * = mPseudoIsoRatio
 * so real ISO = (UI ISO - mMinSensitivity)/mPseudoIsoRatio + baseISO;
 */
int32_t Intel3aPlus::mapUiIso2RealIso (int32_t iso)
{
    if (mSupportIsoMap == false)
        return iso;

    CheckError(fabs(mPseudoIsoRatio) < EPSILON, iso, "@%s, mPseudoIsoRatio < EPSILON", __FUNCTION__);

    const ia_cmc_t* cmc = getCmc();
    CheckError(cmc == nullptr, iso, "@%s, cmc is nullptr", __FUNCTION__);
    CheckError(cmc->cmc_sensitivity == nullptr, iso, "@%s, cmc_sensitivity is nullptr", __FUNCTION__);

    int baseIso = cmc->cmc_sensitivity->base_iso;
    if (iso < mMinSensitivity) {
        LOGW("Limiting UI ISO. Should be larger than %d", mMinSensitivity);
        return baseIso;
    }
    int rIso = round((iso - mMinSensitivity) / mPseudoIsoRatio);
    rIso += baseIso;  // Make sure this is a pure int addition.

    LOG2("%s: iso: %d real iso: %d", __FUNCTION__, iso, rIso);

    return rIso;
}

/**
 * get the UI ISO that mapped to real ISO
 * mapping as following
 * (UI ISO - mMinSensitivity) /(real ISO -base ISO)
 * = (maxSensitivity - mMinSensitivity) / (maxIso - baseISO)
 * = mPseudoIsoRatio
 * so UI ISO = (real ISO -base ISO) * mPseudoIsoRatio + mMinSensitivity;
 */
int32_t Intel3aPlus::mapRealIso2UiIso (int32_t iso)
{
    if (mSupportIsoMap == false)
        return iso;

    CheckError(fabs(mPseudoIsoRatio) < EPSILON, iso, "@%s, mPseudoIsoRatio < EPSILON", __FUNCTION__);

    const ia_cmc_t* cmc = getCmc();
    CheckError(cmc == nullptr, iso, "@%s, cmc is nullptr", __FUNCTION__);
    CheckError(cmc->cmc_sensitivity == nullptr, iso, "@%s, cmc_sensitivity is nullptr", __FUNCTION__);

    int baseIso = cmc->cmc_sensitivity->base_iso;
    if (iso < baseIso) {
        LOGW("Limiting real ISO. Should be larger than %d", baseIso);
        return mMinSensitivity;
    }
    int uiIso = round((iso - baseIso) * mPseudoIsoRatio);
    uiIso += mMinSensitivity;  // Make sure this is a pure int addition.

    LOG2("%s: iso:%d UI iso:%d", __FUNCTION__, iso, uiIso);

    return uiIso;
}

/**
 * This function maps an image enhancement value from range [-10,10]
 * into the range [-128,127] that ia_aiq takes as input
 *
 * \param[in] uiValue Enhancement value coming from the app
 * \return Enhancement value in ia_aiq range
 */
char Intel3aPlus::mapUiImageEnhancement2Aiq(int uiValue)
{
    float step = (SCHAR_MAX - SCHAR_MIN) / UI_IMAGE_ENHANCEMENT_STEPS;
    return (char)(SCHAR_MIN + step * (uiValue + UI_IMAGE_ENHANCEMENT_MAX));
}


status_t Intel3aPlus::fillPAInputParams(const CameraMetadata &settings,
                                        PAInputParams &paInputParams) const
{
    LOG2("@%s", __FUNCTION__);
    if (paInputParams.aiqInputParams == nullptr) {
        LOGE("Null pointer in FPAIP");
        return BAD_VALUE;
    }

    camera_metadata_ro_entry entry;
    CLEAR(entry);
    // black level lock
    bool blackLevelLock = false;
    //# METADATA_Control blackLevel.lock done
    entry = settings.find(ANDROID_BLACK_LEVEL_LOCK);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        if (value == ANDROID_BLACK_LEVEL_LOCK_ON)
            blackLevelLock = true;
    }
    paInputParams.aiqInputParams->blackLevelLock = blackLevelLock;
    return OK;
}

status_t Intel3aPlus::fillSAInputParams(const CameraMetadata &settings,
                                        SAInputParams &saInputParams) const
{
    LOG2("@%s", __FUNCTION__);
    if (saInputParams.aiqInputParams == nullptr) {
        LOGE("Null pointer in FSAIP");
        return BAD_VALUE;
    }

    camera_metadata_ro_entry entry;
    CLEAR(entry);

    //# METADATA_Control shading.mode done
    uint8_t saMode = ANDROID_SHADING_MODE_FAST;
    entry = settings.find(ANDROID_SHADING_MODE);
    if (entry.count == 1) {
        saMode = entry.data.u8[0];
    }
    saInputParams.saMode = saMode;

    //# METADATA_Control statistics.lensShadingMapMode done
    uint8_t shadingMapMode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
    entry = settings.find(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE);
    if (entry.count == 1) {
        shadingMapMode = entry.data.u8[0];
    }
    saInputParams.shadingMapMode = shadingMapMode;

    return OK;
}

/**
 * \brief Converts AE related metadata into ia_aiq_ae_input_params
 *
 * \param[in]  settings request settings in Google format
 * \param[out] aeInputParams all parameters for ae processing
 *
 * \return success or error.
 */
status_t Intel3aPlus::fillAeInputParams(const CameraMetadata *settings,
                                        AeInputParams &aeInputParams)
{
    LOG2("@%s", __FUNCTION__);

    if (aeInputParams.sensorDescriptor == nullptr || settings == nullptr) {
        LOGE("%s: sensorDescriptor %p settings %p!", __FUNCTION__, aeInputParams.sensorDescriptor, settings);
        return BAD_VALUE;
    }

    camera_metadata_ro_entry entry;
    AeControls *aeCtrl = &aeInputParams.aaaControls->ae;
    AiqInputParams *aiqInputParams = aeInputParams.aiqInputParams;
    ia_aiq_exposure_sensor_descriptor *sensorDescriptor;

    sensorDescriptor = aeInputParams.sensorDescriptor;

    if (aeCtrl == nullptr || aiqInputParams == nullptr || sensorDescriptor == nullptr) {
        LOGE("one input parameter is nullptr");
        LOGE("aeCtrl = %p, aiqInputParams = %p, sensorDescriptor = %p", aeCtrl,
                                                                        aiqInputParams,
                                                                        sensorDescriptor);
        return UNKNOWN_ERROR;
    }

    //# METADATA_Control control.aeLock done
    entry = settings->find(ANDROID_CONTROL_AE_LOCK);
    if (entry.count == 1) {
        aeCtrl->aeLock = entry.data.u8[0];
        if (aeCtrl->aeLock == ANDROID_CONTROL_AE_LOCK_ON)
            aiqInputParams->aeLock = true;
    }

    // num_exposures
    aiqInputParams->aeInputParams.num_exposures = 1;

    /* frame_use
     *  BEWARE - THIS VALUE WILL NOT WORK WITH AIQ WHICH RUNS PRE-CAPTURE
     *  WITH STILL FRAME_USE, WHILE THE HAL GETS PREVIEW INTENTS DURING PRE-
     *  CAPTURE!!!
     */
    aiqInputParams->aeInputParams.frame_use = getFrameUseFromIntent(settings);

    // ******** aec_features
    aiqInputParams->aeInputParams.aec_features->backlight_compensation =
                                        ia_aiq_ae_feature_setting_disabled;
    aiqInputParams->aeInputParams.aec_features->face_utilization =
                                        ia_aiq_ae_feature_setting_disabled;
    aiqInputParams->aeInputParams.aec_features->fill_in_flash =
                                        ia_aiq_ae_feature_setting_disabled;
    aiqInputParams->aeInputParams.aec_features->motion_blur_control =
                                        ia_aiq_ae_feature_setting_disabled;
    aiqInputParams->aeInputParams.aec_features->red_eye_reduction_flash =
                                        ia_aiq_ae_feature_setting_disabled;

    // ******** manual_limits (defaults)
    aiqInputParams->aeInputParams.manual_limits->manual_exposure_time_min = -1;
    aiqInputParams->aeInputParams.manual_limits->manual_exposure_time_max = -1;
    aiqInputParams->aeInputParams.manual_limits->manual_frame_time_us_min = -1;
    aiqInputParams->aeInputParams.manual_limits->manual_frame_time_us_max = -1;
    aiqInputParams->aeInputParams.manual_limits->manual_iso_min = -1;
    aiqInputParams->aeInputParams.manual_limits->manual_iso_max = -1;

    // ******** flash_mode  is not support, so set ture off to aiq parameter.
    aiqInputParams->aeInputParams.flash_mode = ia_aiq_flash_mode_off;
    aiqInputParams->aeInputParams.aec_features->red_eye_reduction_flash
            = ia_aiq_ae_feature_setting_disabled;

    //# METADATA_Control control.mode done
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        uint8_t controlMode = entry.data.u8[0];

        //Enforce control mode to AUTO
        controlMode = ANDROID_CONTROL_MODE_AUTO;

        aeInputParams.aaaControls->controlMode = controlMode;

        //TODO FIX VALUES
        switch (controlMode) {
            case ANDROID_CONTROL_MODE_AUTO:
                aiqInputParams->aeInputParams.operation_mode = ia_aiq_ae_operation_mode_automatic;
                break;
            default:
                LOGE("ERROR @%s: Unknown control mode %d", __FUNCTION__, controlMode);
                return BAD_VALUE;
        }
    }

    // ******** metering_mode
    // TODO: implement the metering mode. For now the metering mode is fixed
    // to whole frame
    aiqInputParams->aeInputParams.metering_mode = ia_aiq_ae_metering_mode_evaluative;

    // ******** priority_mode
    // TODO: check if there is something that can be mapped to the priority mode
    // maybe NIGHT_PORTRAIT to highlight for example?
    aiqInputParams->aeInputParams.priority_mode = ia_aiq_ae_priority_mode_normal;

    // ******** flicker_reduction_mode
    //# METADATA_Control control.aeAntibandingMode done
    entry = settings->find(ANDROID_CONTROL_AE_ANTIBANDING_MODE);
    if (entry.count == 1) {
        uint8_t flickerMode = entry.data.u8[0];
        aeCtrl->aeAntibanding = flickerMode;

        switch (flickerMode) {
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
                aiqInputParams->aeInputParams.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_off;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
                aiqInputParams->aeInputParams.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_50hz;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
                aiqInputParams->aeInputParams.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_60hz;
                break;
            case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
                aiqInputParams->aeInputParams.flicker_reduction_mode = ia_aiq_ae_flicker_reduction_auto;
                break;
            default:
                LOGE("ERROR @%s: Unknow flicker mode %d", __FUNCTION__, flickerMode);
                return BAD_VALUE;
        }
    }

    aiqInputParams->aeInputParams.sensor_descriptor->pixel_clock_freq_mhz =
                                        sensorDescriptor->pixel_clock_freq_mhz;
    aiqInputParams->aeInputParams.sensor_descriptor->pixel_periods_per_line =
                                        sensorDescriptor->pixel_periods_per_line;
    aiqInputParams->aeInputParams.sensor_descriptor->line_periods_per_field =
                                        sensorDescriptor->line_periods_per_field;
    aiqInputParams->aeInputParams.sensor_descriptor->line_periods_vertical_blanking =
                                        sensorDescriptor->line_periods_vertical_blanking;
    aiqInputParams->aeInputParams.sensor_descriptor->fine_integration_time_min =
                                        sensorDescriptor->fine_integration_time_min;
    aiqInputParams->aeInputParams.sensor_descriptor->fine_integration_time_max_margin =
                                        sensorDescriptor->fine_integration_time_max_margin;
    aiqInputParams->aeInputParams.sensor_descriptor->coarse_integration_time_min =
                                        sensorDescriptor->coarse_integration_time_min;
    aiqInputParams->aeInputParams.sensor_descriptor->coarse_integration_time_max_margin =
                                        sensorDescriptor->coarse_integration_time_max_margin;

    CameraWindow *aeRegion = aeInputParams.aeRegion;
    CameraWindow *croppingRegion = aeInputParams.croppingRegion;
    // ******** exposure_window
    //# METADATA_Control control.aeRegions done
    parseMeteringRegion(settings,ANDROID_CONTROL_AE_REGIONS, *aeRegion);
    if (aeRegion->isValid()) {
        CameraWindow dst;
        // Clip the region to the crop rectangle
        if (croppingRegion->isValid())
            aeRegion->clip(*croppingRegion);
        convertFromAndroidToIaCoordinates(*aeRegion, dst);

        updateMinAEWindowSize(dst);

        aiqInputParams->aeInputParams.exposure_window->left = dst.left();
        aiqInputParams->aeInputParams.exposure_window->top = dst.top();
        aiqInputParams->aeInputParams.exposure_window->right = dst.right();
        aiqInputParams->aeInputParams.exposure_window->bottom = dst.bottom();
    }

    // ******** exposure_coordinate
    // TODO: set when needed
    aiqInputParams->aeInputParams.exposure_coordinate = nullptr;

    /*
     * MANUAL AE CONTROL
     */
    if (aeInputParams.aaaControls->controlMode == ANDROID_CONTROL_MODE_OFF ||
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
                aiqInputParams->aeInputParams.manual_exposure_time_us[0] =
                  (int)timeMicros;
                aiqInputParams->aeInputParams.manual_limits->
                  manual_exposure_time_min = (int)timeMicros;
                aiqInputParams->aeInputParams.manual_limits->
                  manual_exposure_time_max = (int)timeMicros;
            } else {
                // Don't constrain AIQ.
                aiqInputParams->aeInputParams.manual_exposure_time_us = nullptr;
                aiqInputParams->aeInputParams.manual_limits->
                    manual_exposure_time_min = -1;
                aiqInputParams->aeInputParams.manual_limits->
                    manual_exposure_time_max = -1;
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
                aiqInputParams->aeInputParams.manual_limits->
                  manual_frame_time_us_min = (int)timeMicros;
                aiqInputParams->aeInputParams.manual_limits->
                  manual_frame_time_us_max = (int)timeMicros;
            } else {
                // Don't constrain AIQ.
                aiqInputParams->aeInputParams.manual_limits->
                    manual_frame_time_us_min = -1;
                aiqInputParams->aeInputParams.manual_limits->
                    manual_frame_time_us_max = -1;
            }
        }
        // ******** manual_analog_gain
        aiqInputParams->aeInputParams.manual_analog_gain = nullptr;

        // ******** manual_iso
        //# METADATA_Control sensor.sensitivity done
        entry = settings->find(ANDROID_SENSOR_SENSITIVITY);
        if (entry.count == 1) {
            int32_t iso = entry.data.i32[0];
            if (iso >= mMinSensitivity && iso <= mMaxSensitivity) {
                aiqInputParams->aeInputParams.manual_iso[0] = mapUiIso2RealIso(iso);
                aiqInputParams->aeInputParams.manual_limits->
                    manual_iso_min = aiqInputParams->aeInputParams.manual_iso[0];
                aiqInputParams->aeInputParams.manual_limits->
                    manual_iso_max = aiqInputParams->aeInputParams.manual_iso[0];
            } else
                aiqInputParams->aeInputParams.manual_iso = nullptr;
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
            int32_t evCompensation = CLIP(entry.data.i32[0] + aeInputParams.extraEvShift,
                                        mMaxAeCompensation, mMinAeCompensation);

            aeCtrl->evCompensation = evCompensation;

            float step = PlatformData::getStepEv(mCameraId);
            aiqInputParams->aeInputParams.ev_shift = evCompensation * step;
        } else {
            aiqInputParams->aeInputParams.ev_shift = 0.0f;
        }
        aiqInputParams->aeInputParams.manual_exposure_time_us = nullptr;
        aiqInputParams->aeInputParams.manual_analog_gain = nullptr;
        aiqInputParams->aeInputParams.manual_iso = nullptr;

        // ******** target fps
        int32_t maxSupportedFps = INT_MAX;
        if (aeInputParams.maxSupportedFps != 0)
            maxSupportedFps = aeInputParams.maxSupportedFps;
        //# METADATA_Control control.aeTargetFpsRange done
        entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        if (entry.count == 2) {
            int32_t minFps = MIN(entry.data.i32[0], maxSupportedFps);
            int32_t maxFps = MIN(entry.data.i32[1], maxSupportedFps);
            aeCtrl->aeTargetFpsRange[0] = minFps;
            aeCtrl->aeTargetFpsRange[1] = maxFps;
            /*
             * There is computational accuracy in 3a library, we can not get the perfectly matched frame
             * duration if the fps range is a fix value(eg:30~30). So we calulate the frame length line
             * align to one whole line to add a margin value for manual_frame_time_us_max.
             */
            aiqInputParams->aeInputParams.manual_limits->manual_frame_time_us_min =
                    (long) std::ceil((1.0f / maxFps) * 1000000);
            float lineDurationUs = sensorDescriptor->pixel_periods_per_line
                    / sensorDescriptor->pixel_clock_freq_mhz;
            float marginFll = std::ceil((1.0f / minFps) * 1000000 / lineDurationUs);
            aiqInputParams->aeInputParams.manual_limits->manual_frame_time_us_max =
                    (long) std::ceil(marginFll * lineDurationUs);
        }

        //# METADATA_Control control.aePrecaptureTrigger done
        entry = settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
        if (entry.count == 1) {
            aeCtrl->aePreCaptureTrigger = entry.data.u8[0];
        }

    }
    return NO_ERROR;
}

void Intel3aPlus::updateMinAEWindowSize(CameraWindow &dst)
{
    int minWidth = (IA_COORDINATE_WIDTH + 10) / 10;
    int minHeight = (IA_COORDINATE_HEIGHT + 10) / 10;

    if (dst.width() * dst.height() >= minWidth * minHeight) return;

    ia_coordinate center = dst.center();
    ia_coordinate topLeft = { dst.left(), dst.top() };
    ia_coordinate bottomright = { dst.right(), dst.bottom() };
    if (dst.width() < minWidth) {
        if (dst.center().x < minWidth / 2) {
            center.x = minWidth / 2;
        } else if (dst.center().x > (IA_COORDINATE_WIDTH - minWidth / 2)) {
            center.x = IA_COORDINATE_WIDTH - minWidth / 2;
        }

        topLeft.x = center.x - minWidth / 2;
        bottomright.x = center.x + minWidth / 2;
    }

    if (dst.height() < minHeight) {
        if (dst.center().y < minHeight / 2) {
            center.y = minHeight / 2;
        } else if (dst.center().y > (IA_COORDINATE_HEIGHT - minHeight / 2)) {
            center.y = IA_COORDINATE_HEIGHT - minHeight / 2;
        }

        topLeft.y = center.y - minHeight / 2;
        bottomright.y = center.y + minHeight / 2;
    }

    LOG2("change window from [%d,%d,%d,%d] to [%d,%d,%d,%d]",
          dst.left(), dst.top(), dst.right(), dst.bottom(),
          topLeft.x, topLeft.y, bottomright.x, bottomright.y);

    dst.init(topLeft, bottomright, dst.weight());
}

/**
 * Fills the input parameters for the AF algorithm from the capture request
 * settings.
 * Not all the input parameters will be filled. This class is supposed to
 * be common for all PSL's that use Intel AIQ.
 * There are some input parameters that will be filled by the PSL specific
 * code.
 * The field initialize here are the mandatory ones:
 * frame_use: derived from the control.captureIntent
 * focus_mode: derived from control.afMode
 * focus_range: Focusing range. Only valid when focus_mode is
 *              ia_aiq_af_operation_mode_auto.
 * focus_metering_mode:  Metering mode (multispot, touch).
 * flash_mode:  User setting for flash.
 * trigger_new_search: if new AF search is needed, FALSE otherwise.
 *                     Host is responsible for flag cleaning.
 *
 * There are two mandatory fields that will be filled by the PSL code:
 * lens_position:  Current lens position.
 * lens_movement_start_timestamp: Lens movement start timestamp in us.
 *                          Timestamp is compared against statistics timestamp
 *                          to determine if lens was moving during statistics
 *                          collection.
 *
 * the OPTIONAL fields:
 * manual_focus_parameters: Manual focus parameters (manual lens position,
 *                          manual focusing distance). Used only if focus mode
 *                          'ia_aiq_af_operation_mode_manual' is used. Implies
 *                          that CONTROL_AF_MODE_OFF is used.
 *
 * focus_rect: Not filled here. The reason is that not all platforms implement
 *             touch focus using this rectangle. PSL is responsible for filling
 *             this rectangle or setting it to nullptr.
 *
 * \param[in]  settings capture request metadata settings
 * \param[out] afInputParams struct with the input parameters for the
 *              3A algorithms and also other specific settings parsed
 *              in this method
 *
 * \return OK
 */
status_t Intel3aPlus::fillAfInputParams(const CameraMetadata *settings,
                                        AfInputParams &afInputParams)
{
    status_t status = OK;
    ia_aiq_af_input_params defaultAfCfg;
    uint8_t tmp;

    CLEAR(defaultAfCfg);

    /*
     * To ensure backward compatibility we only take the parameters that are
     * provided, if they are not, we still parse them, but they go nowhere.
     */
    tmp = ANDROID_CONTROL_AF_MODE_OFF;
    uint8_t &afMode = (afInputParams.afControls) ?
                       afInputParams.afControls->afMode :
                       tmp;
    tmp = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    uint8_t &trigger = (afInputParams.afControls) ?
                       afInputParams.afControls->afTrigger :
                       tmp;
    ia_aiq_af_input_params &afCfg = (afInputParams.aiqInputParams) ?
                                    afInputParams.aiqInputParams->afParams :
                                    defaultAfCfg;

    /* frame_use
     *  BEWARE - THIS VALUE WILL NOT WORK WITH AIQ WHICH RUNS PRE-CAPTURE
     *  WITH ia_aiq_frame_use_still, WHEN HAL GETS PREVIEW INTENTS
     *  DURING PRE-CAPTURE!!!
     */
    afCfg.frame_use = getFrameUseFromIntent(settings);

    parseAfTrigger(*settings, afCfg, trigger);

    status = parseAFMode(settings, afCfg, afMode);
    // never fails

    if (afMode == ANDROID_CONTROL_AF_MODE_OFF) {
        status = parseFocusDistance(*settings, afCfg);
        if (status != NO_ERROR) {
            afCfg.manual_focus_parameters = nullptr;
            LOGE("Focus distance parsing failed");
        }
    } else {
        // nullptr settings in non-manual mode, just in case
        afCfg.manual_focus_parameters = nullptr;
    }

    /* flash mode not support, set default value for aiq af*/
    afCfg.flash_mode = ia_aiq_flash_mode_off;

    /**
     * AF region parsing
     * we only support one for the time being
     */
    if (afInputParams.aiqInputParams != nullptr) {
        //# METADATA_Control control.afRegions done
        parseMeteringRegion(settings, ANDROID_CONTROL_AF_REGIONS,
                            afInputParams.aiqInputParams->afRegion);
    } else {
        LOGW("aiqInputParams is nullptr, cannot update AF region.");
    }

    return OK;
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
status_t Intel3aPlus::fillAwbInputParams(const CameraMetadata *settings,
                                         AwbInputParams &awbInputParams)
{
    ia_aiq_awb_input_params *awbCfg;
    AwbControls *awbCtrl;

    if (settings == nullptr
        || awbInputParams.aaaControls == nullptr
        || awbInputParams.aiqInputParams == nullptr) {
        LOGE("Input Param is nullptr!");
        LOGE("settings = %p, aaaControl = %p, aiqInput = %p",
              settings, awbInputParams.aaaControls,
              awbInputParams.aiqInputParams);

        return BAD_VALUE;
    }

    awbCfg = &awbInputParams.aiqInputParams->awbParams;
    awbCtrl = &awbInputParams.aaaControls->awb;

    // AWB lock
    camera_metadata_ro_entry entry;
    //# METADATA_Control control.awbLock done
    entry = settings->find(ANDROID_CONTROL_AWB_LOCK);
    if (entry.count == 1) {
        awbCtrl->awbLock = entry.data.u8[0];
        if (awbCtrl->awbLock == ANDROID_CONTROL_AWB_LOCK_ON)
            awbInputParams.aiqInputParams->awbLock = true;
    }

    /* frame_use
     *  BEWARE - THIS VALUE MAY NOT WORK WITH AIQ WHICH RUNS PRE-CAPTURE
     *  WITH STILL FRAME_USE, WHILE THE HAL GETS PREVIEW INTENTS DURING PRE-
     *  CAPTURE!!!
     */
    awbCfg->frame_use = getFrameUseFromIntent(settings);

    awbCfg->manual_cct_range = nullptr;

    awbCfg->manual_white_coordinate = nullptr; // TODO manual overrides
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
                awbInputParams.aiqInputParams->manualColorTransform[i] = (float) numerator / denominator;
            }
        }

        //# METADATA_Control colorCorrection.gains done
        entry = settings->find(ANDROID_COLOR_CORRECTION_GAINS);
        if (entry.count == 4) {
            // The color gains from application is in order of RGGB
            awbInputParams.aiqInputParams->manualColorGains.r = entry.data.f[0];
            awbInputParams.aiqInputParams->manualColorGains.gr = entry.data.f[1];
            awbInputParams.aiqInputParams->manualColorGains.gb = entry.data.f[2];
            awbInputParams.aiqInputParams->manualColorGains.b = entry.data.f[3];
        }
    }

    //# METADATA_Control control.awbRegions done
    //# METADATA_Dynamic control.awbRegions done
    //# AM Not Supported by 3a
    return NO_ERROR;
}

/**
 * Parses the request setting to find one of the 3 metering regions
 *
 * CONTROL_AE_REGIONS
 * CONTROL_AW_REGIONS
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
void Intel3aPlus::parseMeteringRegion(const CameraMetadata *settings,
                                      int tagId, CameraWindow &meteringWindow)
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
        }
    } else {
        LOGE("trying to use %s incorrectly (BUG)", __FUNCTION__);
    }

    meteringWindow.init(topLeft,bottomRight, weight);
}

ia_aiq_frame_use
Intel3aPlus::getFrameUseFromIntent(const CameraMetadata * settings)
{
    camera_metadata_ro_entry entry;
    ia_aiq_frame_use frameUse = ia_aiq_frame_use_preview;
    //# METADATA_Control control.captureIntent done
    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count == 1) {
        uint8_t captureIntent = entry.data.u8[0];
        switch (captureIntent) {
            case ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM:
                frameUse = ia_aiq_frame_use_preview;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
                frameUse = ia_aiq_frame_use_preview;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
                frameUse = ia_aiq_frame_use_still;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
                frameUse = ia_aiq_frame_use_video;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
                frameUse = ia_aiq_frame_use_video;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
                frameUse = ia_aiq_frame_use_continuous;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
                frameUse = ia_aiq_frame_use_still;
                break;
            default:
                LOGE("ERROR @%s: Unknow frame use %d", __FUNCTION__, captureIntent);
                break;
        }
    }
    return frameUse;
}

void
Intel3aPlus::parseAfTrigger(const CameraMetadata &settings,
                            ia_aiq_af_input_params &afInputParams,
                            uint8_t &trigger) const
{
    camera_metadata_ro_entry entry;
    //# METADATA_Control control.afTrigger done
    entry = settings.find(ANDROID_CONTROL_AF_TRIGGER);
    if (entry.count == 1) {
        trigger = entry.data.u8[0];
        if (trigger == ANDROID_CONTROL_AF_TRIGGER_START) {
            afInputParams.trigger_new_search = true;
        } else if (trigger == ANDROID_CONTROL_AF_TRIGGER_CANCEL) {
            afInputParams.trigger_new_search = false;
        }
        // Otherwise should be IDLE; no effect.
    } else {
        // trigger not present in settigns, default to IDLE
        trigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    }
}

/**
 * parseAFMode
 *
 * Map between the request metadata setting CONTROL_AF_MODE to input
 * parameters for AF algorithm.
 * This settings affects focus_mode and focus_range.
 * focus_metering mode is initialize to a default value. It is up to the PSL
 * implementation to change this.
 * See the comments on AiqInputParams.afRegion for more details on the rational
 *
 * \param[in] settings capture requests settings
 * \param[out] afInputParams parameters for the AF algorithm
 * \param[out] afMode result AF mode parsed from settings
 * \return OK: even if no correct settings are found, defaults are used.
 */
status_t
Intel3aPlus::parseAFMode(const CameraMetadata *settings,
                         ia_aiq_af_input_params &afInputParams,
                         uint8_t &afMode) const

{
    camera_metadata_ro_entry entry;
    afMode = -1;

    uint8_t controlMode;
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        controlMode = entry.data.u8[0];
    } else {
        LOGW("Control mode not set using AUTO mode");
        controlMode = ANDROID_CONTROL_MODE_AUTO;
    }

    if (controlMode == ANDROID_CONTROL_MODE_OFF) {
        afMode = ANDROID_CONTROL_AF_MODE_OFF;
    } else {
        //# METADATA_Control control.afMode done
        entry = settings->find(ANDROID_CONTROL_AF_MODE);
        if (entry.count == 1) {
            afMode = entry.data.u8[0];
        } else {
            afMode = mAvailableAFModes[0];
        }
        if (!afModeIsAvailable(afMode)) {
            LOGW("Trying to request an unsupported AF mode %s, defaulting to %s",
                    META_CONTROL2STR(afMode, afMode),
                    META_CONTROL2STR(afMode, mAvailableAFModes[0]));
            afMode = mAvailableAFModes[0];
        }
    }

    setAfMode(afInputParams, afMode);
    return OK;
}

void Intel3aPlus::setAfMode(ia_aiq_af_input_params &afInputParams,
                            uint8_t afMode) const
{
    switch (afMode) {
    /**
     * For our AF algorithm the Continuous modes defined by platform require
     * the same configuration
     */
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            afInputParams.focus_mode = ia_aiq_af_operation_mode_auto;
            afInputParams.focus_range = ia_aiq_af_range_normal;
            afInputParams.focus_metering_mode = ia_aiq_af_metering_mode_auto;
            break;
        case ANDROID_CONTROL_AF_MODE_MACRO:
            // TODO: can switch to operation_mode_auto,
            // when frame_use is not reset by value from getFrameUseFromIntent();
            afInputParams.focus_mode = ia_aiq_af_operation_mode_manual;
            afInputParams.focus_range = ia_aiq_af_range_macro;
            afInputParams.focus_metering_mode = ia_aiq_af_metering_mode_auto;
            break;
        case ANDROID_CONTROL_AF_MODE_EDOF:
            afInputParams.focus_mode = ia_aiq_af_operation_mode_hyperfocal;
            afInputParams.focus_range = ia_aiq_af_range_extended;
            afInputParams.focus_metering_mode = ia_aiq_af_metering_mode_auto;
            break;
        case ANDROID_CONTROL_AF_MODE_OFF:
            // Generally the infinity focus is obtained as 0.0f manual
            afInputParams.focus_mode = ia_aiq_af_operation_mode_manual;
            afInputParams.focus_range = ia_aiq_af_range_extended;
            afInputParams.focus_metering_mode = ia_aiq_af_metering_mode_auto;
            break;
        case ANDROID_CONTROL_AF_MODE_AUTO:
            // TODO: switch to operation_mode_auto, similar to MACRO AF
            afInputParams.focus_mode = ia_aiq_af_operation_mode_manual;
            afInputParams.focus_range = ia_aiq_af_range_extended;
            afInputParams.focus_metering_mode = ia_aiq_af_metering_mode_auto;
            break;
        default:
            LOGE("ERROR @%s: Unknown focus mode %d- using auto",
                    __FUNCTION__, afMode);
            afInputParams.focus_mode = ia_aiq_af_operation_mode_auto;
            afInputParams.focus_range = ia_aiq_af_range_extended;
            afInputParams.focus_metering_mode = ia_aiq_af_metering_mode_auto;
            break;
    }
}

/**
 * \brief Parses and sets manual focus settings
 *
 * Parses request's settings for LENS_FOCUS_DISTANCE
 * and populates AIQ manual focus settings
 *
 * \param[in] settings current camera request settings
 * \param[in,out] afCfg current AF config with
 *  ia_aiq_af_input_params::manual_focus_parameters set
 *
 *
 * \returns NO_ERROR, if focus distance could be set without any errors
 *          UNKNOWN_ERROR if metadata parsing fails
 *
 * NOTE overrides the current ia_aiq_af_input_params::focus_mode
 * setting with \a ia_aiq_af_operation_mode_infinity, if
 * 0.0f (=infinity) focus distance value is set by the application
 */
status_t
Intel3aPlus::parseFocusDistance(const CameraMetadata &settings,
                                ia_aiq_af_input_params &afCfg) const
{
    status_t status = NO_ERROR;

    float focusDist = 0.0;
    unsigned focusInMm = 0;

    if (afCfg.manual_focus_parameters == nullptr) {
        LOGW("nullptr manual focus params in parsing. BUG.");
        return BAD_VALUE;
    }

    // Set AIQ manual action to 'none' by default
    afCfg.manual_focus_parameters->manual_focus_action
        = ia_aiq_manual_focus_action_none;

    bool parseResult
        = MetadataHelper::getMetadataValue(settings,
                                           ANDROID_LENS_FOCUS_DISTANCE,
                                           focusDist, 1);

    if (parseResult) {
        // We are required to clamp focus distance
        // between [0.0, minFocusDist].
        // the value from app is diopters, focusDist =  1/f so minFocus is big..
        if (focusDist > mMinFocusDistance) {
            focusDist = mMinFocusDistance;
        } else if (focusDist < 0.0f) {
            focusDist = 0.0f;
        }

        if (focusDist != 0.0f) {
            focusInMm = 1000 * (1.0f / focusDist);
            afCfg.manual_focus_parameters->manual_focus_action
                = ia_aiq_manual_focus_action_set_distance;
        } else {
            // 0.0f focus distance means infinity
            afCfg.focus_mode = ia_aiq_af_operation_mode_infinity;
        }
    } else {
        status = UNKNOWN_ERROR;
    }

    afCfg.manual_focus_parameters->manual_focus_distance = focusInMm;

    return status;
}

/**
 *  Hyperfocal distance is the closest distance at which a lens can be focused
 *  while keeping objects at infinity acceptably sharp. When the lens is focused
 *  at this distance, all objects at distances from half of the hyperfocal
 *  distance out to infinity will be acceptably sharp.
 *
 *  The equation used for this is:
 *        f*f
 *  H = -------
 *        N*c
 *
 *  where:
 *   f is the focal length
 *   N is the f-number (f/D for aperture diameter D)
 *   c is the Circle Of Confusion (COC)
 *
 *   the COC is calculated as the pixel width of 2 pixels
 *
 * \param[in] cmc: reference to a valid Camera Module Characterization structure
 * \returns the hyperfocal distance in mm. It is ensured it will never be 0 to
 *          avoid division by 0. If any of the required CMC items is missing
 *          it will return the default value 5m
 */
float Intel3aPlus::calculateHyperfocalDistance(ia_cmc_t &cmc)
{
    return Intel3aCore::calculateHyperfocalDistance(cmc);
}

/***** Deep Copy Functions ******/

status_t Intel3aPlus::deepCopyAiqResults(AiqResults &dst,
                                         const AiqResults &src,
                                         bool onlyCopyUpdatedSAResults)
{
    return Intel3aCore::deepCopyAiqResults(dst, src, onlyCopyUpdatedSAResults);
}

status_t Intel3aPlus::deepCopyAEResults(ia_aiq_ae_results *dst,
                                        const ia_aiq_ae_results *src)
{
    return Intel3aCore::deepCopyAEResults(dst, src);
}

status_t Intel3aPlus::deepCopyGBCEResults(ia_aiq_gbce_results *dst,
                                          const ia_aiq_gbce_results *src)
{
    return Intel3aCore::deepCopyGBCEResults(dst, src);
}

status_t Intel3aPlus::deepCopyPAResults(ia_aiq_pa_results *dst,
                                        const ia_aiq_pa_results *src)
{
    return Intel3aCore::deepCopyPAResults(dst, src);
}

status_t Intel3aPlus::deepCopySAResults(ia_aiq_sa_results *dst,
                                        const ia_aiq_sa_results *src)
{
    return Intel3aCore::deepCopySAResults(dst, src);
}

/**
 * afModeIsAvailable
 *
 * Checks whether the mode passed as input parameter is in the list of available
 * AF modes declared statically
 *
 * \param afMode[IN]: Auto Focus mode ot check
 *
 * \return true if mode is in the list of available modes
 * \return false if it is not
 */
bool
Intel3aPlus::afModeIsAvailable(uint8_t afMode) const
{
    for (size_t i = 0; i < mAvailableAFModes.size(); i++) {
        if (mAvailableAFModes[i] == afMode)
            return true;
    }
    return false;
}

status_t Intel3aPlus::reFormatLensShadingMap(const LSCGrid &inputLscGrid,
                                             float *dstLscGridRGGB)
{
    return Intel3aCore::reFormatLensShadingMap(inputLscGrid,
                                               dstLscGridRGGB);
}

status_t Intel3aPlus::storeLensShadingMap(const LSCGrid &inputLscGrid,
                                          LSCGrid &resizeLscGrid,
                                          float *dstLscGridRGGB)
{
    return Intel3aCore::storeLensShadingMap(inputLscGrid,
                                            resizeLscGrid,
                                            dstLscGridRGGB);
}
} NAMESPACE_DECLARATION_END

