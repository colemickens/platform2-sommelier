/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

Metadata::Metadata(int cameraId, Intel3aPlus *a3aWrapper):
        mMaxCurvePoints(0),
        mRGammaLut(nullptr),
        mGGammaLut(nullptr),
        mBGammaLut(nullptr),
        mCameraId(cameraId),
        m3aWrapper(a3aWrapper)
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
     ia_aiq_pa_results &paResults = reqState.captureSettings->aiqResults.paResults;
     float gains[4];
     gains[0] = paResults.color_gains.r;
     gains[1] = paResults.color_gains.gr;
     gains[2] = paResults.color_gains.gb;
     gains[3] = paResults.color_gains.b;
     reqState.ctrlUnitResult->update(ANDROID_COLOR_CORRECTION_GAINS, gains, 4);

     /*
      * store the results in row major order
      */
     camera_metadata_rational_t transformMatrix[9];
     const int32_t COLOR_TRANSFORM_PRECISION = 10000;
     for (int i = 0; i < 3; i++) {
         for (int j = 0; j < 3; j++) {
             transformMatrix[j + 3*i].numerator =
                     (int32_t)(paResults.color_conversion_matrix[i][j] * COLOR_TRANSFORM_PRECISION);
             transformMatrix[j + 3*i].denominator = COLOR_TRANSFORM_PRECISION;
         }
     }
     reqState.ctrlUnitResult->update(ANDROID_COLOR_CORRECTION_TRANSFORM,
                                     transformMatrix, 9);
}

void Metadata::writePAMetadata(RequestCtrlState &reqState)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    uint8_t blLock = 0;

    if (reqState.blackLevelOff) {
        blLock = ANDROID_BLACK_LEVEL_LOCK_OFF;
        reqState.ctrlUnitResult->update(ANDROID_BLACK_LEVEL_LOCK, &blLock, 1);
    } else {
        blLock = reqState.aiqInputParams.blackLevelLock ?
                    ANDROID_BLACK_LEVEL_LOCK_ON : ANDROID_BLACK_LEVEL_LOCK_OFF;
        reqState.ctrlUnitResult->update(ANDROID_BLACK_LEVEL_LOCK, &blLock, 1);
    }
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
    int32_t sensitivity = 0;
    uint16_t pixels_per_line = 0;
    uint16_t lines_per_frame = 0;
    int64_t manualExpTime = 1;
    ia_aiq_ae_exposure_result *AeExpResult = nullptr;

    CLEAR(entry);

    if (CC_UNLIKELY(settings == nullptr)) {
        LOGE("no settings in request - BUG");
        return;
    }

    /**
     * If we assume parameter accuracy the results for this request are already
     * in the reqState.
     * It would be safer to take this from the embda once we have it
     */
    AeExpResult = reqState.captureSettings->aiqResults.aeResults.exposures;
    if (CC_UNLIKELY(AeExpResult == nullptr)) {
        LOGW("AE exposure results are not present!");
        return;
    }

    // Calculate frame duration from AE results and sensor descriptor
    pixels_per_line = AeExpResult[0].sensor_exposure->line_length_pixels;
    lines_per_frame = AeExpResult[0].sensor_exposure->frame_length_lines;

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
    exposureTime = AeExpResult[0].exposure->exposure_time_us;
    ia_aiq_ae_input_params &inParams = reqState.aiqInputParams.aeInputParams;

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

    /*
     * Android wants the sensitivity in iso arithmetic unit.
     */
    int16_t manualIso = 100;
    if (reqState.aiqInputParams.aeInputParams.manual_iso != nullptr)
        manualIso = reqState.aiqInputParams.aeInputParams.manual_iso[0];

    int32_t inputSensitivity = m3aWrapper->mapRealIso2UiIso(manualIso);
    sensitivity = m3aWrapper->mapRealIso2UiIso(AeExpResult[0].exposure->iso);

    if (sensitivity == 0) {
        LOGW("Sensor sensitivity result is Zero, copy metadata input value");
        sensitivity = inputSensitivity;
    } else if (inputSensitivity > 0 && abs(inputSensitivity - sensitivity) <= DELTA_ISO) {
        // due to rounding issue, aec can give a slight difference than manual request
        sensitivity = inputSensitivity;
    }

    reqState.ctrlUnitResult->update(ANDROID_SENSOR_SENSITIVITY,
                                         &sensitivity, 1);

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

void
Metadata::FillSensorDescriptor(const ControlUnit::Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mSensorDescriptor = msg.data.sensor.exposureDesc;
}

status_t Metadata::fillTonemapCurve(RequestCtrlState &reqState)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    ia_aiq_gbce_results &gbceResults = reqState.captureSettings->aiqResults.gbceResults;

    int multiplier = 1;
    // TODO: apply the curves from request to gbce results
    if (reqState.tonemapContrastCurve) {
        if (reqState.rGammaLut != nullptr) {
            reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_RED,
                    reqState.rGammaLut,
                    reqState.rGammaLutSize);
        }
        if (reqState.gGammaLut != nullptr) {
            reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_GREEN,
                    reqState.gGammaLut,
                    reqState.gGammaLutSize);
        }
        if (reqState.bGammaLut != nullptr) {
            reqState.ctrlUnitResult->update(ANDROID_TONEMAP_CURVE_BLUE,
                    reqState.bGammaLut,
                    reqState.bGammaLutSize);
        }
    } else {
        if (mMaxCurvePoints < gbceResults.gamma_lut_size && mMaxCurvePoints > 0) {
            multiplier = gbceResults.gamma_lut_size / mMaxCurvePoints;
            LOG2("Not enough curve points. Linear interpolation is used.");
        } else {
            mMaxCurvePoints = gbceResults.gamma_lut_size;
        }

        if (mRGammaLut == nullptr ||
            mGGammaLut == nullptr ||
            mBGammaLut == nullptr) {
            LOGE("Lut tables are not initialized.");
            return UNKNOWN_ERROR;
        }

        for (unsigned int i=0; i < mMaxCurvePoints; i++) {
            mRGammaLut[i * 2 + 1] = gbceResults.r_gamma_lut[i * multiplier];
            mGGammaLut[i * 2 + 1] = gbceResults.g_gamma_lut[i * multiplier];
            mBGammaLut[i * 2 + 1] = gbceResults.b_gamma_lut[i * multiplier];
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

} /* namespace camera2 */
} /* namespace android */
