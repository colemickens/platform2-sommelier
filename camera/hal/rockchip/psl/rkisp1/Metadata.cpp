/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "Metadata"

#include "Metadata.h"
#include "ControlUnit.h"
#include "LogHelper.h"
#include "SettingsProcessor.h"
#include "CameraMetadataHelper.h"

namespace android {
namespace camera2 {

Metadata::Metadata(int cameraId, Rk3aPlus *a3aWrapper):
        mMaxCurvePoints(0),
        mRGammaLut(nullptr),
        mGGammaLut(nullptr),
        mBGammaLut(nullptr),
        mCameraId(cameraId)
{
    CLEAR(mSensorDescriptor);
}


Metadata::~Metadata()
{
    DELETE_ARRAY_AND_NULLIFY(mRGammaLut);
    DELETE_ARRAY_AND_NULLIFY(mGGammaLut);
    DELETE_ARRAY_AND_NULLIFY(mBGammaLut);
}

status_t Metadata::init()
{
    return initTonemaps();
}

void
Metadata::writeAwbMetadata(RequestCtrlState &reqState)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    /*
     * Update the manual color correction parameters
     * For the mode assume that we behave and we do as we are told
     */
     reqState.ctrlUnitResult->update(ANDROID_COLOR_CORRECTION_MODE,
                                     &reqState.aaaControls.awb.colorCorrectionMode,
                                     1);
     reqState.ctrlUnitResult->update(ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                                     &reqState.aaaControls.awb.colorCorrectionAberrationMode,
                                     1);
     /*
      * TODO: Consider moving this to common code in 3A class
      */
     rk_aiq_awb_results &awbResults = reqState.captureSettings->aiqResults.awbResults;
     float gains[4] = {1.0, 1.0, 1.0, 1.0};
     gains[0] = awbResults.awb_gain_cfg.awb_gains.red_gain;
     gains[1] = awbResults.awb_gain_cfg.awb_gains.green_r_gain;
     gains[2] = awbResults.awb_gain_cfg.awb_gains.green_b_gain;
     gains[3] = awbResults.awb_gain_cfg.awb_gains.blue_gain;
     reqState.ctrlUnitResult->update(ANDROID_COLOR_CORRECTION_GAINS, gains, 4);

     /*
      * store the results in row major order
      */
     camera_metadata_rational_t transformMatrix[9];
     const int32_t COLOR_TRANSFORM_PRECISION = 10000;
     for (int i = 0; i < 9; i++) {
             transformMatrix[i].numerator =
                     (int32_t)(awbResults.ctk_config.ctk_matrix.coeff[i] * COLOR_TRANSFORM_PRECISION);
             transformMatrix[i].denominator = COLOR_TRANSFORM_PRECISION;

     }

     reqState.ctrlUnitResult->update(ANDROID_COLOR_CORRECTION_TRANSFORM,
                                     transformMatrix, 9);
}

/**
 * Update the Jpeg metadata
 * Only copying from control to dynamic
 */
void Metadata::writeJpegMetadata(RequestCtrlState &reqState) const
{
    if (reqState.request == nullptr) {
        LOGE("nullptr request in RequestCtrlState - BUG.");
        return;
    }

    const CameraMetadata *settings = reqState.request->getSettings();

    if (settings == nullptr) {
        LOGE("No settings for JPEG in request - BUG.");
        return;
    }

    // TODO: Put JPEG settings to ProcessingUnitSettings, when implemented

    camera_metadata_ro_entry entry = settings->find(ANDROID_JPEG_GPS_COORDINATES);
    if (entry.count == 3) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_GPS_COORDINATES, entry.data.d, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_GPS_PROCESSING_METHOD);
    if (entry.count > 0) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_GPS_PROCESSING_METHOD, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_GPS_TIMESTAMP);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_GPS_TIMESTAMP, entry.data.i64, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_ORIENTATION);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_ORIENTATION, entry.data.i32, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_QUALITY);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_QUALITY, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    if (entry.count == 1) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_THUMBNAIL_QUALITY, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count == 2) {
        reqState.ctrlUnitResult->update(ANDROID_JPEG_THUMBNAIL_SIZE, entry.data.i32, entry.count);
    }
}

void Metadata::writeMiscMetadata(RequestCtrlState &reqState) const
{
    uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    reqState.ctrlUnitResult->update(ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    uint8_t flashModeValue = ANDROID_FLASH_MODE_OFF;
    reqState.ctrlUnitResult->update(ANDROID_FLASH_MODE, &flashModeValue, 1);

    reqState.ctrlUnitResult->update(ANDROID_TONEMAP_MODE,
                                    &reqState.captureSettings->tonemapMode, 1);

    reqState.ctrlUnitResult->update(ANDROID_HOT_PIXEL_MODE,
                                    &reqState.captureSettings->hotPixelMode, 1);

    reqState.ctrlUnitResult->update(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
                                    &reqState.captureSettings->hotPixelMapMode, 1);

    uint8_t fdValue = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    reqState.ctrlUnitResult->update(ANDROID_STATISTICS_FACE_DETECT_MODE,
        &fdValue, 1);

    int faceIds[1] = {0};
    reqState.ctrlUnitResult->update(ANDROID_STATISTICS_FACE_IDS,
                                    faceIds, 1);

    // Since there's only one fixed set of lens parameters, this state will
    // always be STATIONARY.
    uint8_t lensState = ANDROID_LENS_STATE_STATIONARY;
    reqState.ctrlUnitResult->update(ANDROID_LENS_STATE, &lensState, 1);
}

void Metadata::writeLSCMetadata(std::shared_ptr<RequestCtrlState> &reqState) const
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    reqState->ctrlUnitResult->update(ANDROID_SHADING_MODE,
                                    &reqState->captureSettings->shadingMode,
                                    1);

    reqState->ctrlUnitResult->update(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                                    &reqState->captureSettings->shadingMapMode,
                                    1);
}

void Metadata::writeLensMetadata(RequestCtrlState &reqState) const
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    // from static metadata in different places. Use same result data for both.
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry_t currentAperture =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_APERTURES);
    camera_metadata_ro_entry_t currentFocalLength =
            MetadataHelper::getMetadataEntry(meta, ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);

    if (currentAperture.count) {
        reqState.ctrlUnitResult->update(ANDROID_LENS_APERTURE,
                currentAperture.data.f,
                currentAperture.count);
    }
    if (currentFocalLength.count) {
        reqState.ctrlUnitResult->update(ANDROID_LENS_FOCAL_LENGTH,
                currentFocalLength.data.f,
                currentFocalLength.count);
    }

    float filterDensityNotSupported = 0.0f;
    reqState.ctrlUnitResult->update(ANDROID_LENS_FILTER_DENSITY,
                                    &filterDensityNotSupported,
                                    1);
}

void Metadata::writeSensorMetadata(RequestCtrlState &reqState)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    const CameraMetadata *settings = reqState.request->getSettings();
    camera_metadata_ro_entry entry;
    int64_t exposureTime = 0;
    uint16_t pixels_per_line = 0;
    uint16_t lines_per_frame = 0;
    int64_t manualExpTime = 1;
    rk_aiq_ae_results *AeExpResult = nullptr;

    CLEAR(entry);

    if (CC_UNLIKELY(settings == nullptr)) {
        LOGE("no settings in request - BUG");
        return;
    }

    if (reqState.aaaControls.ae.aeMode != ANDROID_CONTROL_AE_MODE_OFF) {
        /**
         * If we assume parameter accuracy the results for this request are already
         * in the reqState.
         * It would be safer to take this from the embda once we have it
         */
        AeExpResult = &reqState.captureSettings->aiqResults.aeResults;

        // Calculate frame duration from AE results and sensor descriptor
        pixels_per_line = AeExpResult->sensor_exposure.line_length_pixels;
        lines_per_frame = AeExpResult->sensor_exposure.frame_length_lines;

        /*
         * Android wants the frame duration in nanoseconds
         */
        int64_t frameDuration = (pixels_per_line * lines_per_frame) /
                                mSensorDescriptor.pixel_clock_freq_mhz;
        frameDuration *= 1000;
        reqState.ctrlUnitResult->update(ANDROID_SENSOR_FRAME_DURATION,
                                             &frameDuration, 1);

        /*
         * AE reports exposure in usecs but Android wants it in nsecs
         * In manual mode, use input value if delta to expResult is small.
         */
        exposureTime = AeExpResult->exposure.exposure_time_us;
        rk_aiq_ae_input_params &inParams = reqState.aiqInputParams.aeParams;

        if (inParams.manual_exposure_time_us != nullptr)
            manualExpTime = inParams.manual_exposure_time_us[0];

        if (exposureTime == 0 ||
            (manualExpTime > 0 &&
            fabs((float)exposureTime/manualExpTime - 1) < ONE_PERCENT)) {

            if (exposureTime == 0)
                LOGW("sensor exposure time is Zero, copy input value");
            // copy input value
            exposureTime = manualExpTime;
        }
        exposureTime = exposureTime * 1000; // to ns.
        reqState.ctrlUnitResult->update(ANDROID_SENSOR_EXPOSURE_TIME,
                                         &exposureTime, 1);
    }

    int32_t value = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    entry = settings->find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    if (entry.count == 1)
        value = entry.data.i32[0];

    reqState.ctrlUnitResult->update(ANDROID_SENSOR_TEST_PATTERN_MODE,
                                     &value, 1);
}

status_t Metadata::initTonemaps()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    // Get the max tonemap points
    const camera_metadata_t *meta =
        PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry_t ro_entry;
    CLEAR(ro_entry);
    find_camera_metadata_ro_entry(meta,
            ANDROID_TONEMAP_MAX_CURVE_POINTS, &ro_entry);
    if (ro_entry.count == 1) {
        mMaxCurvePoints = ro_entry.data.i32[0];
    } else {
        LOGW("No max curve points in camera profile xml");
    }

    mRGammaLut = new float[mMaxCurvePoints * 2];
    mGGammaLut = new float[mMaxCurvePoints * 2];
    mBGammaLut = new float[mMaxCurvePoints * 2];

    // Initialize P_IN, P_OUT values [(P_IN, P_OUT), ..]
    for (unsigned int i = 0; i < mMaxCurvePoints; i++) {
        mRGammaLut[i * 2] = (float) i / (mMaxCurvePoints - 1);
        mRGammaLut[i * 2 + 1] = (float) i / (mMaxCurvePoints - 1);
        mGGammaLut[i * 2] = (float) i / (mMaxCurvePoints - 1);
        mGGammaLut[i * 2 + 1] = (float) i / (mMaxCurvePoints - 1);
        mBGammaLut[i * 2] = (float) i / (mMaxCurvePoints - 1);
        mBGammaLut[i * 2 + 1] = (float) i / (mMaxCurvePoints - 1);
    }
    return OK;
}

status_t Metadata::fillTonemapCurve(RequestCtrlState &reqState)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    /* ia_aiq_gbce_results &gbceResults = reqState.captureSettings->aiqResults.gbceResults; */
    rk_aiq_goc_config &results = reqState.captureSettings->aiqResults.miscIspResults.gbce_config.goc_config;

    int multiplier = 1;
    // TODO: apply the curves from request to gbce results
    if (reqState.tonemapContrastCurve) {
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_RED,
                reqState.rGammaLut,
                reqState.rGammaLutSize);
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_GREEN,
                reqState.gGammaLut,
                reqState.gGammaLutSize);
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_BLUE,
                reqState.bGammaLut,
                reqState.bGammaLutSize);
    } else {
        if (mMaxCurvePoints < results.gamma_y.gamma_y_cnt && mMaxCurvePoints > 0) {
            multiplier = results.gamma_y.gamma_y_cnt / mMaxCurvePoints;
            LOG2("Not enough curve points. Linear interpolation is used.");
        } else {
            mMaxCurvePoints = results.gamma_y.gamma_y_cnt;
            if (mMaxCurvePoints > CIFISP_GAMMA_OUT_MAX_SAMPLES)
                mMaxCurvePoints = CIFISP_GAMMA_OUT_MAX_SAMPLES;
        }

        if (mRGammaLut == nullptr ||
            mGGammaLut == nullptr ||
            mBGammaLut == nullptr) {
            LOGE("Lut tables are not initialized.");
            return UNKNOWN_ERROR;
        }

        unsigned short gamma_y_max = mMaxCurvePoints > 0 ? results.gamma_y.gamma_y[mMaxCurvePoints - 1] :
            results.gamma_y.gamma_y[0];
        for (uint32_t i=0; i < mMaxCurvePoints; i++) {
            if (mMaxCurvePoints > 1)
                mRGammaLut[i * 2] = (float) i / (mMaxCurvePoints - 1);
            mRGammaLut[i * 2 + 1] = (float)results.gamma_y.gamma_y[i * multiplier] / gamma_y_max;
            if (mMaxCurvePoints > 1)
                mGGammaLut[i * 2] = (float) i / (mMaxCurvePoints - 1);
            mGGammaLut[i * 2 + 1] = (float)results.gamma_y.gamma_y[i * multiplier] / gamma_y_max;
            if (mMaxCurvePoints > 1)
                mBGammaLut[i * 2] = (float) i / (mMaxCurvePoints - 1);
            mBGammaLut[i * 2 + 1] = (float)results.gamma_y.gamma_y[i * multiplier] / gamma_y_max;
        }
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_RED,
                mRGammaLut,
                mMaxCurvePoints * 2);
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_GREEN,
                mGGammaLut,
                mMaxCurvePoints * 2);
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_BLUE,
                mBGammaLut,
                mMaxCurvePoints * 2);
    }

    if (reqState.captureSettings->tonemapMode == ANDROID_TONEMAP_MODE_GAMMA_VALUE) {
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_GAMMA,
                &reqState.captureSettings->gammaValue,
                1);
    }

    if (reqState.captureSettings->tonemapMode == ANDROID_TONEMAP_MODE_PRESET_CURVE) {
        reqState.ctrlUnitResult->update(ANDROID_TONEMAP_PRESET_CURVE,
                &reqState.captureSettings->presetCurve,
                1);
    }

    return NO_ERROR;
}

void
Metadata::FillSensorDescriptor(const ControlUnit::MessageSensorMode &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mSensorDescriptor = msg.exposureDesc;
}

} /* namespace camera2 */
} /* namespace android */
