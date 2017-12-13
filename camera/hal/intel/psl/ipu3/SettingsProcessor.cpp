/*
 * Copyright (C) 2017-2018 Intel Corporation.
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

#define LOG_TAG "SettingsProcessor"

#include "SettingsProcessor.h"
#include "CameraMetadataHelper.h"
#include "PlatformData.h"
#include "IPU3Types.h"
#include "IntelAFStateMachine.h"


#include "LogHelper.h"

namespace android {
namespace camera2 {

SettingsProcessor::SettingsProcessor(int cameraId,
        Intel3aPlus *a3aWrapper, IStreamConfigProvider &aStreamCfgProv) :
        mCameraId(cameraId),
        m3aWrapper(a3aWrapper),
        mMinSensorModeFrameTime(INT32_MAX),
        mStreamCfgProv(aStreamCfgProv),
        mFixedFocus(false)
{
    /**
     * cache some static value for later use
     */
    mAPA = PlatformData::getActivePixelArray(mCameraId);
    cacheStaticMetadata();
    CLEAR(mCurrentFrameParams);
    CLEAR(mSensorDescriptor);
}

SettingsProcessor::~SettingsProcessor()
{
}

status_t SettingsProcessor::init()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    // save state for fix focus;
    mFixedFocus = (m3aWrapper->getMinFocusDistance() == 0.0f);

    return OK;
}

/**
 * processRequestSettings
 *
 * Analyze the request control metadata tags and prepare the configuration for
 * the AIQ algorithm to run.
 * \param settings [IN] settings from the request
 * \param reqAiqCfg [OUT] AIQ configuration
 */
status_t
SettingsProcessor::processRequestSettings(const CameraMetadata &settings,
                                    RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    /**
     *  Process cropping first since it is used by other settings
     *  like AE and AF
     **/
    processCroppingRegion(settings, reqAiqCfg);

    processAeSettings(settings, reqAiqCfg);
    reqAiqCfg.aeState = ALGORITHM_CONFIGURED;

    processAfSettings(settings, reqAiqCfg);
    reqAiqCfg.afState = ALGORITHM_CONFIGURED;

    processAwbSettings(settings, reqAiqCfg);
    reqAiqCfg.awbState = ALGORITHM_CONFIGURED;


    if ((status = processIspSettings(settings, reqAiqCfg)) != OK)
        return status;

    processImageEnhancementSettings(settings, reqAiqCfg);
    processStabilizationSettings(settings, reqAiqCfg);

    status = processHotPixelSettings(settings, reqAiqCfg);

    PAInputParams params;
    params.aiqInputParams = &reqAiqCfg.aiqInputParams;
    m3aWrapper->fillPAInputParams(settings, params);

    SAInputParams saParams;
    saParams.aiqInputParams = &reqAiqCfg.aiqInputParams;
    m3aWrapper->fillSAInputParams(settings, saParams);

    reqAiqCfg.captureSettings->shadingMode = saParams.saMode;
    reqAiqCfg.captureSettings->shadingMapMode = saParams.shadingMapMode;

    return processTestPatternMode(settings, reqAiqCfg);
}

/**
 * processCroppingRegion
 *
 * Checks if cropping region is set in the capture request settings. If it is
 * then fills the corresponding region in the capture settings.
 * if it is not it sets the default value, the Active Pixel Array
 *
 * \param settings[IN] metadata buffer where the settings are stored
 * \param reqCfg[OUT] the cropping region is stored inside the capture settings
 *                    of this structure
 *
 */
void
SettingsProcessor::processCroppingRegion(const CameraMetadata &settings,
                                   RequestCtrlState &reqCfg)
{
    CameraWindow &cropRegion = reqCfg.captureSettings->cropRegion;

    // If crop region not available, fill active array size as the default value
    //# ANDROID_METADATA_Control android.scaler.cropRegion done
    camera_metadata_ro_entry entry = settings.find(ANDROID_SCALER_CROP_REGION);
    /**
     * Cropping region is invalid if width is 0 or if the rectangle is not
     * fully defined (you need 4 values)
     */
    //# ANDROID_METADATA_Dynamic android.scaler.cropRegion done
    if (entry.count < 4 || entry.data.i32[2] == 0) {
        //cropRegion is a reference and will alter captureSettings->cropRegion.
        cropRegion = mAPA;
        int32_t *cropWindow;
        ia_coordinate topLeft = {0, 0};
        cropRegion.init(topLeft,
                         mAPA.width(),  //width
                         mAPA.height(),  //height
                         0);
        // meteringRectangle is filling 4 coordinates and weight (5 values)
        // here crop region only needs the rectangle, so we copy only 4.
        cropWindow = (int32_t*)mAPA.meteringRectangle();
        reqCfg.ctrlUnitResult->update(ANDROID_SCALER_CROP_REGION, cropWindow, 4);
    } else {
        ia_coordinate topLeft = {entry.data.i32[0],entry.data.i32[1]};
        cropRegion.init(topLeft,
                        entry.data.i32[2],  //width
                        entry.data.i32[3],  //height
                        0);
        reqCfg.ctrlUnitResult->update(ANDROID_SCALER_CROP_REGION, entry.data.i32, 4);
    }

    // copy the crop region to the processingSettings so that tasks don't have
    // to break the Law-Of-Demeter.
    reqCfg.processingSettings->cropRegion = cropRegion;
}

status_t SettingsProcessor::processIspSettings(const CameraMetadata &settings,
                                         RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    IspSettings *ispSettings = nullptr;

    if (reqAiqCfg.captureSettings) {
        ispSettings = &reqAiqCfg.captureSettings->ispSettings;
    } else {
        LOGE("Capture Settings is nullptr!, bug");
        return UNKNOWN_ERROR;
    }

    //# ANDROID_METADATA_Control android.edge.strength done
    //# AM FUTURE
    entry = settings.find(ANDROID_EDGE_STRENGTH);
    if (entry.count == 1) {
        // mapping from 1->10 to -127->128
        uint8_t strength = entry.data.u8[0];
        ispSettings->eeSetting.strength = ((strength * UINT8_MAX) / ANDROID_MAX_STRENGTH) - INT8_MIN;
        reqAiqCfg.captureSettings->ispControls.ee.strength = strength;
    } else {
        // does not affect according to ia_isp specs.
        ispSettings->eeSetting.strength = 0;
    }

    //# ANDROID_METADATA_Control android.noiseReduction.mode done
    entry = settings.find(ANDROID_NOISE_REDUCTION_MODE);
    uint8_t noiseReductionMode = 0;
    MetadataHelper::getSetting(mStaticMetadataCache.availableNoiseReductionModes, entry,
                                 &noiseReductionMode);
    reqAiqCfg.captureSettings->ispControls.nr.mode = noiseReductionMode;

    switch (noiseReductionMode) {
        case ANDROID_NOISE_REDUCTION_MODE_OFF:
            ispSettings->nrSetting.feature_level = ia_isp_feature_level_off;
            break;
        case ANDROID_NOISE_REDUCTION_MODE_FAST:
            // the speed of execution is the same for high or low quality
            // therefore we apply also high quality in fast.
            ispSettings->nrSetting.feature_level = ia_isp_feature_level_high;
            break;
        case ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY:
            ispSettings->nrSetting.feature_level = ia_isp_feature_level_high;
            break;
        case ANDROID_NOISE_REDUCTION_MODE_MINIMAL:
            ispSettings->nrSetting.feature_level = ia_isp_feature_level_low;
            break;
        case ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG:
            ispSettings->nrSetting.feature_level = ia_isp_feature_level_low;
            break;
        default:
            LOGE("ERROR: Unknown noise reduction mode %d", noiseReductionMode);
            return BAD_VALUE;
    }
    //# ANDROID_METADATA_Control android.noiseReduction.strength done
    //# AM FUTURE
    entry = settings.find(ANDROID_NOISE_REDUCTION_STRENGTH);
    if (entry.count == 1) {
        // mapping from 1->10 to -127->128
        uint8_t strength = entry.data.u8[0];
        ispSettings->nrSetting.strength = ((strength * UINT8_MAX) / ANDROID_MAX_STRENGTH) - INT8_MIN;
        reqAiqCfg.captureSettings->ispControls.nr.strength = strength;
    } else {
        // does not affect according to ia_isp specs.
        ispSettings->nrSetting.strength = 0;
    }

    //# ANDROID_METADATA_Control android.control.effectMode done
    entry = settings.find(ANDROID_CONTROL_EFFECT_MODE);
    uint8_t effectMode = 0;
    MetadataHelper::getSetting(mStaticMetadataCache.availableEffectModes, entry, &effectMode);
    reqAiqCfg.captureSettings->ispControls.effect = effectMode;

    switch (effectMode) {
        case ANDROID_CONTROL_EFFECT_MODE_OFF:
            ispSettings->effects = ia_isp_effect_none;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_MONO:
            ispSettings->effects = ia_isp_effect_grayscale;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
            ispSettings->effects = ia_isp_effect_negative;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
            ispSettings->effects = ia_isp_effect_sepia;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_AQUA:
            ispSettings->effects = ia_isp_effect_aqua;
            break;
        case ANDROID_CONTROL_EFFECT_MODE_SOLARIZE:
        case ANDROID_CONTROL_EFFECT_MODE_POSTERIZE:
        case ANDROID_CONTROL_EFFECT_MODE_WHITEBOARD:
        case ANDROID_CONTROL_EFFECT_MODE_BLACKBOARD:
        default:
            LOGE("ERROR: Unknown effect mode %d", effectMode);
            return BAD_VALUE;
    }
    return OK;
}

void SettingsProcessor::cacheStaticMetadata()
{
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    mStaticMetadataCache.availableEffectModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_CONTROL_AVAILABLE_EFFECTS);
    mStaticMetadataCache.availableNoiseReductionModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES);
    mStaticMetadataCache.availableTonemapModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES);
    mStaticMetadataCache.availableVideoStabilization =
            MetadataHelper::getMetadataEntry(meta, ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES);
    mStaticMetadataCache.availableOpticalStabilization =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION);
    mStaticMetadataCache.currentAperture =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_APERTURES);
    mStaticMetadataCache.flashInfoAvailable =
            MetadataHelper::getMetadataEntry(meta, ANDROID_FLASH_INFO_AVAILABLE);
    mStaticMetadataCache.lensShadingMapSize =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_SHADING_MAP_SIZE);
    mStaticMetadataCache.currentFocalLength =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
    mStaticMetadataCache.availableHotPixelMapModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES);
    mStaticMetadataCache.availableHotPixelModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES);
    mStaticMetadataCache.availableEdgeModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_EDGE_AVAILABLE_EDGE_MODES);
    mStaticMetadataCache.maxAnalogSensitivity =
            MetadataHelper::getMetadataEntry(meta, ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY);
    mStaticMetadataCache.pipelineDepth =
            MetadataHelper::getMetadataEntry(meta, ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    mStaticMetadataCache.lensSupported =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE);
    mStaticMetadataCache.availableTestPatternModes =
            MetadataHelper::getMetadataEntry(meta, ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES);
}

status_t
SettingsProcessor::processAfSettings(const CameraMetadata  &settings,
                               RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    AfInputParams inParams;
    inParams.aiqInputParams = &reqAiqCfg.aiqInputParams;
    inParams.afControls = &reqAiqCfg.aaaControls.af;

    m3aWrapper->fillAfInputParams(&settings, inParams);

    /**
     * If we are in fixed Focus, we do not need to return any af window, since
     * available af window is 0.
     */
    if (mFixedFocus)
        return status;

    /**
     * We pass the AF region directly from the input params
     * to the capture settings.
     * We clip the AfRegion with the Cropping region as required by V3 API
     */
    if (reqAiqCfg.aiqInputParams.afRegion.isValid()) {
        /**
         * clip the AF metering region with the cropping region
         * cropping region is in Android coordinates, so it is the AF region
         * at this point.
         */
        reqAiqCfg.captureSettings->afRegion = reqAiqCfg.aiqInputParams.afRegion;

        reqAiqCfg.captureSettings->afRegion.clip(reqAiqCfg.captureSettings->cropRegion);

        /**
         * Convert to IA coordinates before passing it to AF algo
         */
        const CameraWindow &fromW = reqAiqCfg.captureSettings->afRegion;
        CameraWindow toW;
        m3aWrapper->convertFromAndroidToIaCoordinates(fromW, toW);
        /**
         * setting the metering mode to touch focus
         */
        reqAiqCfg.aiqInputParams.afParams.focus_metering_mode = ia_aiq_af_metering_mode_touch;

        reqAiqCfg.aiqInputParams.afParams.focus_rect->top = toW.top();
        reqAiqCfg.aiqInputParams.afParams.focus_rect->left = toW.left();
        reqAiqCfg.aiqInputParams.afParams.focus_rect->bottom = toW.bottom();
        reqAiqCfg.aiqInputParams.afParams.focus_rect->right = toW.right();

    } else {
        reqAiqCfg.captureSettings->afRegion.reset();
        // This is the normal case when there is no AF region defined.
        reqAiqCfg.captureSettings->afRegion = reqAiqCfg.captureSettings->cropRegion;
    }
    return status;
}

/**
 * This function fills ISP settings with manual image enhancement settings
 * (brightness, contrast, hue, saturation and sharpness) coming from the app,
 * in case they are supported by HAL.
 *
 * \param settings [IN] settings from the request
 * \param reqAiqCfg [OUT] AIQ configuration
 */
status_t SettingsProcessor::processImageEnhancementSettings(const CameraMetadata &settings,
                                                      RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    IspSettings *ispSettings = nullptr;

    if (reqAiqCfg.captureSettings) {
        ispSettings = &reqAiqCfg.captureSettings->ispSettings;
    } else {
        LOGE("Capture Settings is nullptr!, bug");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * This function reads the COM_INTEL_IMAGE_ENHANCE values, maps them to
 * the range that ia_aiq expects and updates metadata.
 *
 * \param[in] settings Settings from the request
 * \param[in] enhancementName Enhancement name
 * \param[in,out] reqAiqCfg AIQ configuration
 * \return enhancement value in ia_aiq range
 */
char SettingsProcessor::mapImageEnhancementSettings(const CameraMetadata &settings,
                                              const int enhancementName,
                                              RequestCtrlState &reqAiqCfg)
{
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    entry = settings.find(enhancementName);
    if (entry.count == 1) {
        int enhancementValue = entry.data.i32[0];
        // The result can be updated immadiately since the enhancement values
        // will not change
        reqAiqCfg.ctrlUnitResult->update(enhancementName,
                                         &enhancementValue,
                                         1);
        if (abs(enhancementValue) <= UI_IMAGE_ENHANCEMENT_MAX) {
            return(m3aWrapper->mapUiImageEnhancement2Aiq(enhancementValue));
        } else {
            LOGE("Enhancement value %d outside expected range [%d,%d]",
                 enhancementValue, -UI_IMAGE_ENHANCEMENT_MAX,
                 UI_IMAGE_ENHANCEMENT_MAX);
        }
    }
    return 0;
}


status_t
SettingsProcessor::processAeSettings(const CameraMetadata&  settings,
                               RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    AeInputParams aeInputParams;
    aeInputParams.aiqInputParams = &reqAiqCfg.aiqInputParams;
    aeInputParams.aaaControls = &reqAiqCfg.aaaControls;
    aeInputParams.croppingRegion = &reqAiqCfg.captureSettings->cropRegion;
    aeInputParams.aeRegion = &reqAiqCfg.captureSettings->aeRegion;
    aeInputParams.sensorDescriptor = &mSensorDescriptor;

    m3aWrapper->fillAeInputParams(&settings, aeInputParams);

    if (aeInputParams.aiqInputParams) {
        /*
         * apply the sensor limits reported from the exposure sensor descriptor
         *
         * The exposure sensor descriptor is updated every time we change sensor
         * mode.
         * Each sensor mode has associated a maximum fps. We should not let AE
         * to produce values that drive the sensor at a higher speed.
         *
         * This operation is already done inside fillAeInputParams, but
         * unfortunately the input parameter is an int
         * (AeInputParams.maxSupportedFps) therefore we apply the limit here
         * with more precision.
         *
         * In other PSL the AeInputParams.maxSupportedFps passed to 3A is coming
         * from the reported min stream duration in static metadata.
         *
         * In our case we use the limit reported by the sensor mode selected.
         * The value mMinSensorModeFrameTime is updated after every stream
         * config.
         */
        ia_aiq_ae_input_params *aeParams = nullptr;
        aeParams = &aeInputParams.aiqInputParams->aeInputParams;
        aeParams->flicker_reduction_mode = ia_aiq_ae_flicker_reduction_off;
        if (aeParams->manual_limits->manual_frame_time_us_min < mMinSensorModeFrameTime) {
            aeParams->manual_limits->manual_frame_time_us_min = mMinSensorModeFrameTime;
        }
        if (aeParams->manual_limits->manual_frame_time_us_max < mMinSensorModeFrameTime) {
            aeParams->manual_limits->manual_frame_time_us_max = mMinSensorModeFrameTime;
        }
    }

    return status;
}

status_t
SettingsProcessor::handleNewSensorDescriptor(ControlUnit::Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mCurrentFrameParams = msg.data.sensor.frameParams;
    mSensorDescriptor = msg.data.sensor.exposureDesc;

    /*
     * store the minimum frame time for this sensor mode.
     * This is the maximum fps that the sensor mode supports.
     * Use this to limit any frame rate requests from client.
     * See fillAeInputParams() to understand how this value is used.
     * min frame duration is:
     *       pix_per_line * lines_per_frame
     *   ----------------------------------------
     *               pixel_clock
     */
    mMinSensorModeFrameTime = (mSensorDescriptor.pixel_periods_per_line *
                               mSensorDescriptor.line_periods_per_field)/
                                       mSensorDescriptor.pixel_clock_freq_mhz;

    LOG1("---- New Sensor descriptor information received -----");

    // todo revert this when graph config works
    // The current frame parameters are all wrong, due to incomplete graph
    // config implementation which seems to be in a constant flux. Fetch
    // the sensor crop area from media ctl while things are as they are
    const MediaCtlConfig *mediaCtlConfig = mStreamCfgProv.getMediaCtlConfig(IStreamConfigProvider::CIO2);
    for (unsigned int i = 0; i < mediaCtlConfig->mSelectionParams.size(); i++) {
        MediaCtlSelectionParams param = mediaCtlConfig->mSelectionParams[i];
        if (strstr(param.entityName.c_str(), "pixel array")) {
            mCurrentFrameParams.cropped_image_width = param.width;
            mCurrentFrameParams.cropped_image_height = param.height;
            mCurrentFrameParams.horizontal_crop_offset = param.top;
            mCurrentFrameParams.vertical_crop_offset = param.left;
        }
    }

    LOG1("Frame Params: crop offset: %dx%d crop rect: %dx%d"
         " v-scale: %d/%d h-scale: %d/%d",
         mCurrentFrameParams.horizontal_crop_offset,
         mCurrentFrameParams.vertical_crop_offset,
         mCurrentFrameParams.cropped_image_width,
         mCurrentFrameParams.cropped_image_height,
         mCurrentFrameParams.horizontal_scaling_numerator,
         mCurrentFrameParams.horizontal_scaling_denominator,
         mCurrentFrameParams.vertical_scaling_numerator,
         mCurrentFrameParams.vertical_scaling_denominator);

    LOG1("Sensor descriptor: pix-clock: %f Mhz ppl: %d lpf: %d lpvb: %d "
            "integration time min(margin) fine: %d (%d) coarse:%d(%d)",
            mSensorDescriptor.pixel_clock_freq_mhz,
            mSensorDescriptor.pixel_periods_per_line,
            mSensorDescriptor.line_periods_per_field,
            mSensorDescriptor.line_periods_vertical_blanking,
            mSensorDescriptor.fine_integration_time_min,
            mSensorDescriptor.fine_integration_time_max_margin,
            mSensorDescriptor.coarse_integration_time_min,
            mSensorDescriptor.coarse_integration_time_max_margin);
    return OK;
}

status_t
SettingsProcessor::processAwbSettings(const CameraMetadata  &settings,
                                RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    AwbInputParams awbInputParams;
    awbInputParams.aiqInputParams = &reqAiqCfg.aiqInputParams;
    awbInputParams.aaaControls = &reqAiqCfg.aaaControls;

    m3aWrapper->fillAwbInputParams(&settings,
                                   awbInputParams);

    return status;
}

status_t
SettingsProcessor::processStabilizationSettings(const CameraMetadata &settings,
                                          RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    camera_metadata_ro_entry entry;
    //# ANDROID_METADATA_Control android.control.videoStabilizationMode done
    entry = settings.find(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE);
    MetadataHelper::getSetting(mStaticMetadataCache.availableVideoStabilization, entry,
                                 &(reqAiqCfg.captureSettings->videoStabilizationMode));

    //# ANDROID_METADATA_Control android.lens.opticalStabilizationMode done
    entry = settings.find(ANDROID_LENS_OPTICAL_STABILIZATION_MODE);
    MetadataHelper::getSetting(mStaticMetadataCache.availableOpticalStabilization, entry,
                                 &(reqAiqCfg.captureSettings->opticalStabilizationMode));
    return OK;
}

status_t SettingsProcessor::processHotPixelSettings(const CameraMetadata &settings,
                                              RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    camera_metadata_ro_entry entry;
    //# ANDROID_METADATA_Control android.statistics.hotPixelMapMode done
    entry = settings.find(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE);
    MetadataHelper::getSetting(mStaticMetadataCache.availableHotPixelMapModes, entry,
                                 &(reqAiqCfg.captureSettings->hotPixelMapMode));
    //# ANDROID_METADATA_Control android.hotPixel.mode done
    entry = settings.find(ANDROID_HOT_PIXEL_MODE);
    MetadataHelper::getSetting(mStaticMetadataCache.availableHotPixelModes, entry,
                                 &(reqAiqCfg.captureSettings->hotPixelMode));
    return OK;
}

status_t SettingsProcessor::processTestPatternMode(const CameraMetadata &settings,
                                             RequestCtrlState &reqAiqCfg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    camera_metadata_ro_entry entry;

    entry = settings.find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    MetadataHelper::getSetting(mStaticMetadataCache.availableTestPatternModes, entry,
                                 &(reqAiqCfg.captureSettings->testPatternMode));

    return OK;
}

} /* namespace camera2 */
} /* namespace android */
