/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#define LOG_TAG "Intel3aCore"

#include "Intel3aCore.h"

#include <utils/Errors.h>
#include <math.h>
#include <limits.h> // SCHAR_MAX, SCHAR_MIN
#include <ia_cmc_parser.h>
#include <ia_exc.h>
#include <ia_log.h>
#include <ia_mkn_encoder.h>
#include <cameranvm.h>

#include "PlatformData.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "Utils.h"

NAMESPACE_DECLARATION {
__attribute__((constructor(102))) void initIaLog() {
    // set up logging functions for CCA library
    ia_env env;
    env.vdebug = &LogHelper::cca_print_debug;
    env.verror = &LogHelper::cca_print_error;
    env.vinfo = &LogHelper::cca_print_info;
    ia_log_init(&env);
}

__attribute__((destructor(102))) void deInitIaLog() {
    ia_log_deinit();
}

void AiqInputParams::init()
{
    CLEAR(aeInputParams);
    CLEAR(afParams);
    CLEAR(awbParams);
    CLEAR(gbceParams);
    CLEAR(paParams);
    CLEAR(saParams);
    CLEAR(sensorDescriptor);
    CLEAR(exposureWindow);
    CLEAR(exposureCoordinate);
    CLEAR(aeFeatures);
    CLEAR(aeManualLimits);
    CLEAR(manualFocusParams);
    CLEAR(focusRect);
    CLEAR(manualCctRange);
    CLEAR(manualWhiteCoordinate);
    CLEAR(afRegion);
    CLEAR(awbResults);
    CLEAR(colorGains);
    CLEAR(exposureParams);
    CLEAR(sensorFrameParams);
    aeLock = false;
    awbLock = false;
    blackLevelLock = false;
    reset();
}

void AiqInputParams::reset()
{
    aeInputParams.sensor_descriptor = &sensorDescriptor;
    aeInputParams.exposure_window = &exposureWindow;
    aeInputParams.exposure_coordinate = &exposureCoordinate;
    aeInputParams.aec_features = &aeFeatures;
    aeInputParams.manual_limits = &aeManualLimits;
    aeInputParams.manual_exposure_time_us = &manual_exposure_time_us[0];
    aeInputParams.manual_analog_gain = &manual_analog_gain[0];
    aeInputParams.manual_iso = &manual_iso[0];

    afParams.focus_rect = &focusRect;
    afParams.manual_focus_parameters = &manualFocusParams;

    awbParams.manual_cct_range = &manualCctRange;
    awbParams.manual_white_coordinate = &manualWhiteCoordinate;

    paParams.awb_results = &awbResults;
    paParams.color_gains = &colorGains;
    paParams.exposure_params = &exposureParams;

    saParams.awb_results = &awbResults;
    saParams.sensor_frame_params = &sensorFrameParams;
}

AiqInputParams &AiqInputParams::operator=(const AiqInputParams &other)
{
    LOG2("@%s", __FUNCTION__);
    if (this == &other)
        return *this;

    MEMCPY_S(this,
             sizeof(AiqInputParams),
             &other,
             sizeof(AiqInputParams));
    reset();

    /* Exposure coordinate is nullptr in other than SPOT mode. */
    if (other.aeInputParams.exposure_coordinate == nullptr)
        aeInputParams.exposure_coordinate = nullptr;

    /* focus_rect and manual_focus_parameters may be nullptr */
    if (other.afParams.focus_rect == nullptr)
        afParams.focus_rect = nullptr;
    if (other.afParams.manual_focus_parameters == nullptr)
        afParams.manual_focus_parameters = nullptr;

    /* manual_cct_range and manual_white_coordinate may be nullptr */
    if (other.awbParams.manual_cct_range == nullptr)
        awbParams.manual_cct_range = nullptr;
    if (other.awbParams.manual_white_coordinate == nullptr)
        awbParams.manual_white_coordinate = nullptr;

    return *this;
}

status_t AiqResults::allocateLsc(size_t lscSize)
{
    mChannelR  = new float[lscSize];
    mChannelGR = new float[lscSize];
    mChannelGB = new float[lscSize];
    mChannelB  = new float[lscSize];

    return OK;
}

AiqResults::AiqResults() :
        requestId(0),
        detectedSceneMode(ia_aiq_scene_mode_none),
        mChannelR(nullptr),
        mChannelGR(nullptr),
        mChannelGB(nullptr),
        mChannelB(nullptr)
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(awbResults);
    CLEAR(aeResults);
    CLEAR(gbceResults);
    CLEAR(paResults);
    CLEAR(mExposureResults);
    CLEAR(mWeightGrid);
    CLEAR(mGenericExposure);
    CLEAR(mSensorExposure);
    CLEAR(afResults);
    CLEAR(saResults);
    CLEAR_N(mFlashes, NUM_FLASH_LEDS);
}


AiqResults::~AiqResults()
{
    DELETE_ARRAY_AND_NULLIFY(mChannelR);
    DELETE_ARRAY_AND_NULLIFY(mChannelGR);
    DELETE_ARRAY_AND_NULLIFY(mChannelGB);
    DELETE_ARRAY_AND_NULLIFY(mChannelB);
}

void AiqResults::init()
{
    CLEAR(aeResults.num_exposures);
    CLEAR(aeResults.lux_level_estimate);
    CLEAR(aeResults.multiframe);
    CLEAR(aeResults.flicker_reduction_mode);
    CLEAR(aeResults.aperture_control);
    CLEAR(mExposureResults);
    CLEAR(mWeightGrid);
    CLEAR(mFlashes);
    CLEAR(mGenericExposure);
    CLEAR(mSensorExposure);

    /*AE results init */
    aeResults.exposures = &mExposureResults;
    aeResults.weight_grid = &mWeightGrid;
    aeResults.weight_grid->weights = mGrid;
    aeResults.flashes = mFlashes;
    aeResults.exposures->exposure = &mGenericExposure;
    aeResults.exposures->sensor_exposure = &mSensorExposure;
    /* GBCE results init */
    gbceResults.gamma_lut_size = 0;
    gbceResults.r_gamma_lut = mRGammaLut;
    gbceResults.g_gamma_lut = mGGgammaLut;
    gbceResults.b_gamma_lut = mBGammaLut;

    CLEAR(afResults);

    /* Shading Adaptor results init */
    CLEAR(saResults);
    saResults.channel_r = mChannelR;
    saResults.channel_gr = mChannelGR;
    saResults.channel_gb = mChannelGB;
    saResults.channel_b = mChannelB;
}

AiqResults &AiqResults::operator=(const AiqResults& other)
{
    Intel3aCore::deepCopyAiqResults(*this, other);
    requestId = other.requestId;
    return *this;
}

Intel3aCore::Intel3aCore(int camId):
        mCmc(nullptr),
        mIaAiqHandle(nullptr),
        mMkn(nullptr),
        mCameraId(camId),
        mHyperFocalDistance(2.5f),
        mEnableAiqdDataSave(false)
{
    LOG1("@%s", __FUNCTION__);
}


status_t Intel3aCore::init(int maxGridW,
                              int maxGridH,
                              ia_binary_data nvmData,
                              const char* sensorName)
{
    LOG1("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_binary_data cpfData;
    ia_binary_data *outputNvmData = nullptr;
    const AiqConf* aiqConf = PlatformData::getAiqConfiguration(mCameraId);
    if (CC_UNLIKELY(aiqConf == nullptr)) {
        LOGE("CPF file was not initialized ");
        return NO_INIT;
    }
    cpfData.data = aiqConf->ptr();
    cpfData.size = aiqConf->size();

    if (sensorName != nullptr) {
        if (nvmData.data == nullptr || nvmData.size == 0) {
            LOG1("NVM data not available for %s - not necessarily an error", sensorName);
        } else {
            LOG1("sensorname: %s, nvmData.size: %d", sensorName, nvmData.size);
            /**
             * Convert the NVM data from an unknown binary format into Intel Format.
             * Sensor name is given as a parameter so that a correct parser is chosen.
             */
            cameranvm_create(sensorName, &nvmData, nullptr, &outputNvmData);
        }
    }
    // No need to report the case where sensor name is nullptr since many of the PSLs
    // don't use that.

    mMkn = ia_mkn_init(ia_mkn_cfg_compression,
                       MAKERNOTE_SECTION1_SIZE,
                       MAKERNOTE_SECTION2_SIZE);
    if (mMkn == nullptr) {
        LOGE("Error in initing makernote");
        status = UNKNOWN_ERROR;
    }

    iaErr = ia_mkn_enable(mMkn, true);
    if (iaErr != ia_err_none) {
        status = convertError(iaErr);
        LOGE("Error in enabling makernote: %d", status);
    }

    mCmc = aiqConf->getCMCHandler();
    if (CC_UNLIKELY(mCmc == nullptr)) {
        LOGE("%s: CMC handler nullptr, not initialized", __FUNCTION__);
        cameranvm_delete(outputNvmData);
        return NO_INIT;
    }

    ia_binary_data aiqdData;
    CLEAR(aiqdData);
    const ia_binary_data *pAiqdData = nullptr;

    if (mEnableAiqdDataSave && sensorName != nullptr) {
        /* fill in aiqd info to do 3A calculation */
        if (PlatformData::readAiqdData(mCameraId, &aiqdData))
            pAiqdData = &aiqdData;
    }

    mIaAiqHandle = ia_aiq_init((ia_binary_data*)&(cpfData),
                                outputNvmData,
                                pAiqdData,
                                maxGridW,
                                maxGridH,
                                NUM_EXPOSURES,
                                mCmc,
                                mMkn);

    if (!mIaAiqHandle) {
        LOGE("Error in IA AIQ init");
        status = UNKNOWN_ERROR;
    }

    mHyperFocalDistance = calculateHyperfocalDistance(*mCmc);
    /**
     * Cache all the values we are going to need from the static metadata
     */

    mActivePixelArray = PlatformData::getActivePixelArray(mCameraId);
    if (CC_UNLIKELY(!mActivePixelArray.isValid()))
        status = UNKNOWN_ERROR;

    return status;
}

void Intel3aCore::deinit()
{
    LOG1("@%s", __FUNCTION__);

    if (mEnableAiqdDataSave)
        saveAiqdData();

    ia_aiq_deinit(mIaAiqHandle);
    mIaAiqHandle = nullptr;
    ia_mkn_uninit(mMkn);
    mMkn = nullptr;
}

/**
 * Converts ia_aiq errors into generic status_t
 */
status_t Intel3aCore::convertError(ia_err iaErr)
{
    switch (iaErr) {
    case ia_err_none:
        return NO_ERROR;
    case ia_err_general:
        return UNKNOWN_ERROR;
    case ia_err_nomemory:
        return NO_MEMORY;
    case ia_err_data:
        return BAD_VALUE;
    case ia_err_internal:
        return INVALID_OPERATION;
    case ia_err_argument:
        return BAD_VALUE;
    default:
        return UNKNOWN_ERROR;
    }
}

/**
 * This function maps an image enhancement value from range [-10,10]
 * into the range [-128,127] that ia_aiq takes as input
 *
 * \param[in] uiValue Enhancement value coming from the app
 * \return Enhancement value in ia_aiq range
 */
char Intel3aCore::mapUiImageEnhancement2Aiq(int uiValue)
{
    float step = (SCHAR_MAX - SCHAR_MIN) / UI_IMAGE_ENHANCEMENT_STEPS;
    return (char)(SCHAR_MIN + step * (uiValue + UI_IMAGE_ENHANCEMENT_MAX));
}

void Intel3aCore::convertFromAndroidToIaCoordinates(const CameraWindow &srcWindow,
                                                    CameraWindow &toWindow)
{
    const ia_coordinate_system iaCoord = {IA_COORDINATE_TOP, IA_COORDINATE_LEFT,
                                          IA_COORDINATE_BOTTOM, IA_COORDINATE_RIGHT};
    ia_coordinate_system androidCoord = {mActivePixelArray.top(),
                                         mActivePixelArray.left(),
                                         mActivePixelArray.bottom(),
                                         mActivePixelArray.right()};
    ia_coordinate topleft = {0, 0};
    ia_coordinate bottomright = {0, 0};

    topleft.x = srcWindow.left();
    topleft.y = srcWindow.top();
    bottomright.x = srcWindow.right();
    bottomright.y = srcWindow.bottom();

    topleft = ia_coordinate_convert(&androidCoord,
                                    &iaCoord, topleft);
    bottomright = ia_coordinate_convert(&androidCoord,
                                        &iaCoord, bottomright);

    toWindow.init(topleft, bottomright, srcWindow.weight());
}

void Intel3aCore::convertFromIaToAndroidCoordinates(const CameraWindow &srcWindow,
                                                    CameraWindow &toWindow)
{
    const ia_coordinate_system iaCoord = {IA_COORDINATE_TOP, IA_COORDINATE_LEFT,
                                          IA_COORDINATE_BOTTOM, IA_COORDINATE_RIGHT};
    ia_coordinate_system androidCoord = {mActivePixelArray.top(),
                                         mActivePixelArray.left(),
                                         mActivePixelArray.bottom(),
                                         mActivePixelArray.right()};
    ia_coordinate topleft = {0, 0};
    ia_coordinate bottomright = {0, 0};

    topleft.x = srcWindow.left();
    topleft.y = srcWindow.top();
    bottomright.x = srcWindow.right();
    bottomright.y = srcWindow.bottom();

    topleft = ia_coordinate_convert(&iaCoord,
                                    &androidCoord, topleft);
    bottomright = ia_coordinate_convert(&iaCoord,
                                        &androidCoord, bottomright);

    toWindow.init(topleft, bottomright, srcWindow.weight());
}

status_t Intel3aCore::setStatistics(ia_aiq_statistics_input_params *ispStatistics)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    if (ispStatistics != nullptr) {
          iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
          status |= convertError(iaErr);
          if (CC_UNLIKELY(status != NO_ERROR)) {
              LOGE("Error setting statistics before 3A");
          }
    }
    return status;
}

/**
 * \brief Wrapper for ia_mkn_add_record. Adds or updates a data record in the makernote.
 *
 * \param[in] data_format_id     Mandatory.\n
 *                               Record data format ID.
 * \param[in] data_name_id       Mandatory.\n
 *                               Record name ID.
 * \param[in] data               Mandatory.\n
 *                               Pointer to record data to be copied into the makernote. Size of data to be copied is calculated
 *                               from on DFID and number of elements.
 * \param[in] num_elements       Mandatory.\n
 *                               Number of elements to store.
 * \param[in] key                Mandatory.\n
 *                               Packing key (16 bytes). nullptr means 'no packing'.
 * \return                       Error code.
 */
status_t Intel3aCore::addMakerNoteRecord(ia_mkn_dfid mkn_data_format_id,
                                ia_mkn_dnid mkn_data_name_id,
                                const void *data,
                                unsigned int num_elements,
                                const char *key)
{
    if (!data || !mMkn)
        return BAD_VALUE;

    int ret = ia_mkn_add_record(mMkn, mkn_data_format_id, mkn_data_name_id, data,
                                num_elements, key);

    return (ret != 0) ? UNKNOWN_ERROR : OK;
}

/**
 * getMakerNote
 *
 * Retrieve the maker note information from the 3A library and copies it to
 * the provided binary blob.
 *  Section 1 or 2 (which could represent e.g. JPEG EXIF or RAW Header data).
 *  Notice that if Section 2 is selected, an output makernote data will contain both Section 1 and Section 2 parts
 *
 * \param[in] aTarget target section to be retrieved (see note above)
 * \param[out] aBlob binary blob to store the makernote
 *
 * \return BAD_VALUE    If the provided blob doesn't have enough space
 * \return OK           if everything is fine
 */
status_t Intel3aCore::getMakerNote(ia_mkn_trg aTarget, ia_binary_data &aBlob)
{
    ia_binary_data mkn;
    if (aBlob.data == nullptr)
        return BAD_VALUE;

    mkn = ia_mkn_prepare(mMkn, aTarget);

    if (mkn.size > aBlob.size) {
        LOGE(" Provided buffer is too small (%d) for maker note (%d)",
                aBlob.size, mkn.size);
        return BAD_VALUE;
    }

    MEMCPY_S(aBlob.data, aBlob.size, mkn.data, mkn.size);

    aBlob.size = mkn.size;
    return OK;
}

status_t Intel3aCore::runAe(ia_aiq_statistics_input_params *ispStatistics,
                            ia_aiq_ae_input_params *aeInputParams,
                            ia_aiq_ae_results* aeResults)
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_aiq_ae_results *new_ae_results = nullptr;
    if (CC_UNLIKELY(mIaAiqHandle == nullptr)) {
        LOGE("ia_aiq_handle does not exist.");
        return NO_INIT;
    }

    /**
     * First set statistics if provided
     */
    if (ispStatistics != nullptr) {
        iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error setting statistics before 3A");
        }
    }
    // ToDo: cases to be considered in 3ACU
    //    - invalidated (empty ae results)
    //    - AE locked
    //    - AF assist light case (set the statistics from before assist light)


    if (aeInputParams != nullptr &&
            aeInputParams->manual_exposure_time_us &&
            aeInputParams->manual_analog_gain &&
            aeInputParams->manual_iso) {
        LOG2("AEC manual_exposure_time_us: %ld manual_analog_gain: %f manual_iso: %d",
                                        aeInputParams->manual_exposure_time_us[0],
                                        aeInputParams->manual_analog_gain[0],
                                        aeInputParams->manual_iso[0]);
        LOG2("AEC frame_use: %d", aeInputParams->frame_use);
        if (aeInputParams->sensor_descriptor != nullptr) {
            LOG2("AEC line_periods_per_field: %d",
                  aeInputParams->sensor_descriptor->line_periods_per_field);
        }
    }
    {
        PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_ae_run", 1);
        iaErr = ia_aiq_ae_run(mIaAiqHandle, aeInputParams, &new_ae_results);
    }
    status |= convertError(iaErr);
    if (status != NO_ERROR) {
        LOGE("Error running AE");
    }

    //storing results;
    if (CC_LIKELY(new_ae_results != nullptr)) {
        status = deepCopyAEResults(aeResults, new_ae_results);
    }

    if (CC_UNLIKELY(status != NO_ERROR)) {
       LOGE("Error running AE %d",status);
    }
    return status;
}

status_t Intel3aCore::runAf(ia_aiq_statistics_input_params *ispStatistics,
                            ia_aiq_af_input_params *afInputParams,
                            ia_aiq_af_results *afResults)
{
    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_aiq_af_results *new_af_results = nullptr;
    if (CC_UNLIKELY(mIaAiqHandle == nullptr)) {
        LOGE("ia_aiq_handle does not exist.");
        return NO_INIT;
    }

    /**
     * First set statistics if provided
     */
    if (ispStatistics != nullptr) {
        iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error setting statistics before AF");
        }
    }
    {
        PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_af_run", 1);
        iaErr = ia_aiq_af_run(mIaAiqHandle, afInputParams, &new_af_results);
    }
    status |= convertError(iaErr);
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGE("Error running AF %d ia_err %d", status, iaErr);
    } else {
        //storing results;
        if (CC_LIKELY(new_af_results != nullptr))
            MEMCPY_S(afResults,
                     sizeof(ia_aiq_af_results),
                     new_af_results,
                     sizeof(ia_aiq_af_results));
    }

    return status;
}

status_t Intel3aCore::runAwb(ia_aiq_statistics_input_params *ispStatistics,
                            ia_aiq_awb_input_params *awbInputParams,
                            ia_aiq_awb_results* awbResults)
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_aiq_awb_results *new_awb_results = nullptr;
    /**
     * First set statistics if provided
     */
    if (ispStatistics != nullptr) {
        iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error setting statistics before 3A");
        }
    }

    if (CC_LIKELY(mIaAiqHandle)) {
        if (awbInputParams != nullptr) {
            // Todo: manual AWB
        }
        {
            PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_awb_run", 1);
            iaErr = ia_aiq_awb_run(mIaAiqHandle, awbInputParams, &new_awb_results);
        }
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error running AWB");
        }
    } else {
        status = UNKNOWN_ERROR;
        LOGE("ia_aiq_handle does not exist.");
    }

    //storing results;
    if (CC_LIKELY(new_awb_results != nullptr))
        MEMCPY_S(awbResults,
                 sizeof(ia_aiq_awb_results),
                 new_awb_results,
                 sizeof(ia_aiq_awb_results));

    if (CC_UNLIKELY(status != NO_ERROR)) {
       LOGE("Error running AWB %d",status);
    }
    return status;
}

/**
 * Runs the Global Brightness and Contrast Enhancement algorithm
 */
status_t Intel3aCore::runGbce(ia_aiq_statistics_input_params *ispStatistics,
                              ia_aiq_gbce_input_params *gbceInputParams,
                              ia_aiq_gbce_results *gbceResults)
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_aiq_gbce_results *new_gbce_results = nullptr;
    /**
     * First set statistics if provided
     */
    if (ispStatistics != nullptr) {
        iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error setting statistics before run GBCE");
        }
    }

    if (CC_LIKELY(mIaAiqHandle)) {
        if (gbceInputParams != nullptr) {
            // Todo: manual GBCE?
        }
        {
            PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_gbce_run", 1);
            iaErr = ia_aiq_gbce_run(mIaAiqHandle, gbceInputParams, &new_gbce_results);
        }
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error running GBCE");
        }
    } else {
        status = UNKNOWN_ERROR;
        LOGE("ia_aiq_handle does not exist.");
    }

    //storing results;
    if (CC_LIKELY(new_gbce_results != nullptr))
        status = deepCopyGBCEResults(gbceResults, new_gbce_results);

    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGE("Error running GBCE %d",status);
    }
    return status;
}

status_t Intel3aCore::runDsd(ia_aiq_dsd_input_params *dsdInputParams,
                             ia_aiq_scene_mode &detectedSceneMode)
{
    LOG2("@%s", __FUNCTION__);
    detectedSceneMode = ia_aiq_scene_mode_none;

    if (CC_UNLIKELY(!mIaAiqHandle)) {
        LOGE("ia_aiq_handle does not exist.");
        return UNKNOWN_ERROR;
    }

    if (CC_UNLIKELY(!dsdInputParams)) {
        LOGE("No dsd input params.");
        return UNKNOWN_ERROR;
    }
    ia_err ret;
    {
        PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_dsd_run", 1);
        ret = ia_aiq_dsd_run(mIaAiqHandle,
                                dsdInputParams,
                                &detectedSceneMode);
    }
    if (ret != ia_err_none) {
        return convertError(ret);
    }

    LOG2("Success, detected scene mode: %d", detectedSceneMode);
    return NO_ERROR;
}

/**
 * Runs the Parameter adaptor stage
 */
status_t Intel3aCore::runPa(ia_aiq_statistics_input_params *ispStatistics,
                            ia_aiq_pa_input_params *paInputParams,
                            ia_aiq_pa_results *paResults)
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_aiq_pa_results *new_pa_results = nullptr;
    if (CC_UNLIKELY(!mIaAiqHandle)) {
        LOGE("ia_aiq_handle does not exist.");
        return  UNKNOWN_ERROR;
    }
    /**
     * First set statistics if provided
     */
    if (ispStatistics != nullptr) {
        iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error setting statistics before PA run");
        }
    }
    {
        PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_pa_run", 1);
        iaErr = ia_aiq_pa_run(mIaAiqHandle, paInputParams, &new_pa_results);
    }
    status |= convertError(iaErr);
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGE("Error running PA");
    }

    //storing results;
    status = deepCopyPAResults(paResults, new_pa_results);

    if (CC_UNLIKELY(status != NO_ERROR)) {
       LOGE("Error running PA %d",status);
    }
    return status;
}

/**
 * Runs the Shading adaptor stage
 * This is the stage that produces the LSC table
 */
status_t Intel3aCore::runSa(ia_aiq_statistics_input_params *ispStatistics,
                            ia_aiq_sa_input_params *saInputParams,
                            ia_aiq_sa_results *saResults)
{
    LOG2("@%s", __FUNCTION__);

    status_t status = NO_ERROR;
    ia_err iaErr = ia_err_none;
    ia_aiq_sa_results *new_sa_results = nullptr;
    if (CC_UNLIKELY(!mIaAiqHandle)) {
        LOGE("ia_aiq_handle does not exist.");
        return  UNKNOWN_ERROR;
    }
    /**
     * First set statistics if provided
     */
    if (ispStatistics != nullptr) {
        iaErr = ia_aiq_statistics_set(mIaAiqHandle, ispStatistics);
        status |= convertError(iaErr);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Error setting statistics before SA run");
        }
    }

    {
        PERFORMANCE_HAL_ATRACE_PARAM1("ia_aiq_sa_run", 1);
        iaErr = ia_aiq_sa_run(mIaAiqHandle, saInputParams, &new_sa_results);
    }

    status |= convertError(iaErr);
    if ((CC_UNLIKELY(status != NO_ERROR))) {
        LOGE("Error running SA");
    }

    //storing results;
    status = deepCopySAResults(saResults, new_sa_results);

    if (CC_UNLIKELY(status != NO_ERROR)) {
       LOGE("Error running SA %d",status);
    }
    return status;
}

/**
 *
 * Calculate the Depth of field (DOF) for a given AF Result.
 *
 * The Formulas to calculate the near and afar DOF are:
 *          H * s
 * Dn = ---------------
 *         H + (s-f)
 *
 *          H * s
 * Df =  ------------
 *         H - (s-f)
 *
 * Where:
 * H is the hyperfocal distance (that we get from CPF) (it cannot be 0)
 * s is the distance to focused object (current focus distance)
 * f is the focal length
 *
 * \param[in] afResults with current focus distance in mm
 * \param[out] dofNear: DOF for near limit in mm
 * \param[out] dofFar: DOF for far limit in mm
 *
 * \return OK
 */
status_t Intel3aCore::calculateDepthOfField(const ia_aiq_af_results &afResults,
                                            float &dofNear, float &dofFar) const
{
    float focalLengthMillis = 2.3f;
    float num = 0.0f;
    float denom = 1.0f;
    float focusDistance = 1.0f * afResults.current_focus_distance;
    const float DEFAULT_DOF = 5000.0f;
    dofNear = DEFAULT_DOF;
    dofFar = DEFAULT_DOF;
    cmc_optomechanics_t *optoInfo = mCmc->cmc_parsed_optics.cmc_optomechanics;
    if (CC_UNLIKELY(focusDistance == 0.0f)) {
        // Not reporting error since this may be normal in fixed focus sensors
        return OK;
    }

    if (optoInfo) {
        // focal length is stored in CMC in hundreds of millimeters
        focalLengthMillis = (float)optoInfo->effect_focal_length / 100;
    }
    num = mHyperFocalDistance * focusDistance;
    denom = (mHyperFocalDistance + focusDistance - focalLengthMillis);
    if (denom != 0.0f) {
        dofNear = num / denom;
    }

    denom = (mHyperFocalDistance - focusDistance + focalLengthMillis);
    if (denom != 0.0f) {
        dofFar = num / denom;
    }

    return OK;
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
float Intel3aCore::calculateHyperfocalDistance(ia_cmc_t &cmc)
{
    float hyperfocalDistanceMillis = 0;
    float pixelSizeMicro = 100.0f; // size of pixels in um, default to avoid
                                   // division by 0
    float focalLengthMillis = 0.0f;
    float fNumber = 0.0f;
    const float DEFAULT_HYPERFOCAL_DISTANCE = 5000.0f;
    const int CIRCLE_OF_CONFUSION_IN_PIXELS = 2;

    cmc_optomechanics_t *optoInfo = cmc.cmc_parsed_optics.cmc_optomechanics;
    if (optoInfo) {
        // Pixel size is stored in CMC in hundreds of micrometers
        pixelSizeMicro = optoInfo->sensor_pix_size_h / 100;
        // focal length is stored in CMC in hundreds of millimeters
        focalLengthMillis = (float)optoInfo->effect_focal_length / 100;
    }
    // fixed aperture, the fn should be divided 100 because the value
    // is multiplied 100 in cmc
    if (!cmc.cmc_parsed_optics.lut_apertures) {
        LOGW("lut apertures is not provided in the cmc. Using default");
        return DEFAULT_HYPERFOCAL_DISTANCE;
    }

    fNumber = (float)cmc.cmc_parsed_optics.lut_apertures[0] / 100;
    if (fNumber == 0.0f)  // to avoid division by 0 later
        return DEFAULT_HYPERFOCAL_DISTANCE;

    // assuming square pixel
    float cocMicros = pixelSizeMicro * CIRCLE_OF_CONFUSION_IN_PIXELS;

    hyperfocalDistanceMillis = 1000 * (focalLengthMillis * focalLengthMillis) /
                                      (fNumber * cocMicros);
    return hyperfocalDistanceMillis != 0.0f ?
           hyperfocalDistanceMillis : DEFAULT_HYPERFOCAL_DISTANCE;
}

status_t Intel3aCore::convertExposureSensorUnitToAndroid(const ia_aiq_ae_results *results,
                                                         const ia_aiq_exposure_sensor_descriptor *sensorDescriptor,
                                                         int64_t &exposureTime)
{
    status_t status = NO_ERROR;
    int usExposure = 0;
    if (CC_UNLIKELY(results == nullptr || results->exposures == nullptr ||
                    results->exposures->sensor_exposure  == nullptr ||
                    sensorDescriptor == nullptr)) {
        LOGE("%s: one of the pointer to data is nullptr", __FUNCTION__);
        return BAD_VALUE;
    }

    status = ia_exc_sensor_units_to_exposure_time(sensorDescriptor,
                                                  results->exposures->sensor_exposure->coarse_integration_time,
                                                  results->exposures->sensor_exposure->fine_integration_time,
                                                  &usExposure);
    // ExposureTime in nanoseconds
    exposureTime = usExposure * 1000;

    return status;
}

status_t Intel3aCore::convertGainSensorUnitToAndroidISO(const ia_aiq_ae_results *results, int32_t &iso) {

    status_t status = NO_ERROR;
    if (CC_UNLIKELY(results == nullptr || results->exposures == nullptr ||
                    results->exposures->sensor_exposure == nullptr)) {
        LOGE("%s: one of the pointer to data is nullptr", __FUNCTION__);
        return BAD_VALUE;
    }

    status = ia_exc_convert_gain_codes_to_iso(&mCmc->cmc_parsed_analog_gain_conversion,
                                              &mCmc->cmc_parsed_digital_gain,
                                              mCmc->cmc_sensitivity,
                                              results->exposures->sensor_exposure->analog_gain_code_global,
                                              results->exposures->sensor_exposure->digital_gain_global,
                                              &iso);
    return status;
}

/***** Deep Copy Functions ******/

status_t Intel3aCore::deepCopyAiqResults(AiqResults &dst,
                                         const AiqResults &src,
                                         bool onlyCopyUpdatedSAResults)
{
    status_t status = OK;
    status = deepCopyAEResults(&dst.aeResults, &src.aeResults);
    status |= deepCopyGBCEResults(&dst.gbceResults, &src.gbceResults);
    status |= deepCopyPAResults(&dst.paResults, &src.paResults);
    if (!onlyCopyUpdatedSAResults || src.saResults.lsc_update)
        status |= deepCopySAResults(&dst.saResults, &src.saResults);
    MEMCPY_S(&dst.awbResults,
             sizeof(ia_aiq_awb_results),
             &src.awbResults,
             sizeof(ia_aiq_awb_results));
    MEMCPY_S(&dst.afResults,
             sizeof(ia_aiq_af_results),
             &src.afResults,
             sizeof(ia_aiq_af_results));
    return status;
}

status_t Intel3aCore::deepCopyAEResults(ia_aiq_ae_results *dst,
                                        const ia_aiq_ae_results *src)
{
    /**
     * lets check that all the pointers are there
     * in the source and in the destination
     */
    if (CC_UNLIKELY(dst == nullptr || dst->exposures == nullptr ||
        dst->flashes == nullptr || dst->weight_grid == nullptr ||
        dst->weight_grid->weights == nullptr)) {
        LOGE("Failed to deep copy AE result- invalid destination");
        return BAD_VALUE;
    }
    if (CC_UNLIKELY(src == nullptr || src->exposures == nullptr ||
            src->flashes == nullptr || src->weight_grid == nullptr ||
            src->weight_grid->weights == nullptr)) {
        LOGE("Failed to deep copy AE result- invalid source");
        return BAD_VALUE;
    }

    dst->lux_level_estimate = src->lux_level_estimate;
    dst->flicker_reduction_mode = src->flicker_reduction_mode;
    dst->multiframe = src->multiframe;
    dst->num_flashes = src->num_flashes;
    dst->num_exposures = src->num_exposures;

    dst->exposures->converged = src->exposures->converged;
    dst->exposures->distance_from_convergence = src->exposures->distance_from_convergence;
    dst->exposures->exposure_index = src->exposures->exposure_index;
    *dst->exposures->exposure = *src->exposures->exposure;
    *dst->exposures->sensor_exposure = *src->exposures->sensor_exposure;

    // Copy weight grid
    dst->weight_grid->width = src->weight_grid->width;
    dst->weight_grid->height = src->weight_grid->height;

    unsigned int gridElements  = src->weight_grid->width *
                                 src->weight_grid->height;
    gridElements = CLIP(gridElements, MAX_AE_GRID_SIZE, 1);
    STDCOPY(dst->weight_grid->weights, src->weight_grid->weights,
            gridElements * sizeof(char));

    // Copy the flash info structure
    STDCOPY((int8_t *) dst->flashes, (int8_t *) src->flashes,
            NUM_FLASH_LEDS * sizeof(ia_aiq_flash_parameters));

    return NO_ERROR;
}

status_t Intel3aCore::deepCopyGBCEResults(ia_aiq_gbce_results *dst,
                                          const ia_aiq_gbce_results *src)
{
    /**
     * lets check that all the pointers are there
     * in the source and in the destination
     */
    if (CC_UNLIKELY(dst == nullptr || dst->r_gamma_lut == nullptr ||
       dst->g_gamma_lut == nullptr || dst->b_gamma_lut == nullptr )) {
        LOGE("Failed to deep copy GBCE result- invalid destination");
        return BAD_VALUE;
    }
    if (CC_UNLIKELY(src == nullptr || src->r_gamma_lut == nullptr ||
       src->g_gamma_lut == nullptr || src->b_gamma_lut == nullptr )) {
        LOGE("Failed to deep copy GBCE result- invalid src");
        return BAD_VALUE;
    }

    STDCOPY((int8_t *) dst->r_gamma_lut, (int8_t *) src->r_gamma_lut,
            src->gamma_lut_size * sizeof(float));

    STDCOPY((int8_t *) dst->g_gamma_lut, (int8_t *) src->g_gamma_lut,
            src->gamma_lut_size * sizeof(float));

    STDCOPY((int8_t *) dst->b_gamma_lut, (int8_t *) src->b_gamma_lut,
            src->gamma_lut_size * sizeof(float));

    dst->gamma_lut_size = src->gamma_lut_size;

    return NO_ERROR;
}

status_t Intel3aCore::deepCopyPAResults(ia_aiq_pa_results *dst,
                                        const ia_aiq_pa_results *src)
{
    /**
     * lets check that all the pointers are there
     * in the source and in the destination
     */
    if (CC_UNLIKELY(dst == nullptr ||
                    dst->color_conversion_matrix == nullptr)) {
        LOGE("Failed to deep copy PA result- invalid destination");
        return BAD_VALUE;
    }
    if (CC_UNLIKELY(src == nullptr ||
                   src->color_conversion_matrix == nullptr)) {
        LOGE("Failed to deep copy PA result- invalid source");
        return BAD_VALUE;
    }

    MEMCPY_S((void*)dst,
             sizeof(ia_aiq_pa_results),
             (void*)src,
             sizeof(ia_aiq_pa_results));

    //TODO: Copy the tables
    dst->linearization.r = nullptr;
    dst->linearization.gr = nullptr;
    dst->linearization.gb = nullptr;
    dst->linearization.b = nullptr;

    return NO_ERROR;
}

status_t Intel3aCore::deepCopySAResults(ia_aiq_sa_results *dst,
                                        const ia_aiq_sa_results *src)
{
    /**
     * lets check that all the pointers are there
     * in the source and in the destination
     */
    if (CC_UNLIKELY(dst == nullptr ||
                   dst->channel_r == nullptr ||
                   dst->channel_gr == nullptr ||
                   dst->channel_gb == nullptr ||
                   dst->channel_b == nullptr)) {
        LOGE("Failed to deep copy SA result- invalid destination");
        return BAD_VALUE;
    }
    if (CC_UNLIKELY(src == nullptr ||
                   src->channel_r == nullptr ||
                   src->channel_gr == nullptr ||
                   src->channel_gb == nullptr ||
                   src->channel_b == nullptr)) {
        LOGE("Failed to deep copy SA result- invalid source");
        return BAD_VALUE;
    }

    dst->width = src->width;
    dst->height = src->height;
    dst->lsc_update = src->lsc_update;

    if (src->lsc_update) {
        uint32_t memCopySize = src->width * src->height * sizeof(float);
        STDCOPY((int8_t *) dst->channel_r, (int8_t *) src->channel_r, memCopySize);
        STDCOPY((int8_t *) dst->channel_gr, (int8_t *) src->channel_gr, memCopySize);
        STDCOPY((int8_t *) dst->channel_gb, (int8_t *) src->channel_gb, memCopySize);
        STDCOPY((int8_t *) dst->channel_b, (int8_t *) src->channel_b, memCopySize);
    }

    return NO_ERROR;
}

status_t Intel3aCore::reFormatLensShadingMap(const LSCGrid &inputLscGrid,
                                             float *dstLscGridRGGB)
{
    LOG2("@%s, line:%d, width %d, height %d", __FUNCTION__, __LINE__,
         inputLscGrid.width, inputLscGrid.height);

    if (inputLscGrid.isBad() || dstLscGridRGGB == nullptr) {
        LOGE("Bad input values for lens shading map reformatting");
        return BAD_VALUE;
    }

    // Metadata spec request order [R, Geven, Godd, B]
    // the lensShading from ISP is 4 width * height block,
    // for ia_aiq_bayer_order_grbg, the four block is G, R, B, G
    size_t size = inputLscGrid.height * inputLscGrid.width;
    for (size_t i = 0; i < size; i++) {
        *dstLscGridRGGB++ = inputLscGrid.gridR[i];
        *dstLscGridRGGB++ = inputLscGrid.gridGr[i];
        *dstLscGridRGGB++ = inputLscGrid.gridGb[i];
        *dstLscGridRGGB++ = inputLscGrid.gridB[i];
    }

    return OK;
}

status_t Intel3aCore::storeLensShadingMap(const LSCGrid &inputLscGrid,
                                          LSCGrid &resizeLscGrid,
                                          float *dstLscGridRGGB)
{
    LOG2("@%s, line:%d", __FUNCTION__, __LINE__);
    if (inputLscGrid.isBad() || resizeLscGrid.isBad() ||
        dstLscGridRGGB == nullptr) {
        LOGE("Bad input values for lens shading map storing");
        return BAD_VALUE;
    }

    int destWidth = resizeLscGrid.width;
    int destHeight = resizeLscGrid.height;
    int width = inputLscGrid.width;
    int height = inputLscGrid.height;

    if (width != destWidth || height != destHeight) {
        // requests lensShadingMapSize must be smaller than 64*64
        // and it is a constant size.
        // Our lensShadingMapSize is dynamic based on the resolution, so need
        // to do resize for 4 channels separately

        resize2dArray(inputLscGrid.gridR,  width, height,
                      resizeLscGrid.gridR,  destWidth, destHeight);
        resize2dArray(inputLscGrid.gridGr,  width, height,
                      resizeLscGrid.gridGr,  destWidth, destHeight);
        resize2dArray(inputLscGrid.gridGb,  width, height,
                      resizeLscGrid.gridGb,  destWidth, destHeight);
        resize2dArray(inputLscGrid.gridB,  width, height,
                      resizeLscGrid.gridB,  destWidth, destHeight);

        LOG2("resize the lens shading map from [%d,%d] to [%d,%d]",
                width, height, destWidth, destHeight);
    } else {
        size_t size = destWidth * destHeight * sizeof(resizeLscGrid.gridR[0]);
        STDCOPY((int8_t *) resizeLscGrid.gridR,  (int8_t *) inputLscGrid.gridR,  size);
        STDCOPY((int8_t *) resizeLscGrid.gridGr, (int8_t *) inputLscGrid.gridGr, size);
        STDCOPY((int8_t *) resizeLscGrid.gridGb, (int8_t *) inputLscGrid.gridGb, size);
        STDCOPY((int8_t *) resizeLscGrid.gridB,  (int8_t *) inputLscGrid.gridB,  size);
    }

    return reFormatLensShadingMap(resizeLscGrid, dstLscGridRGGB);
}

/**
 *
 * Enable/Disable load/save the aiqd data from/to
 * the host file system
 *
 */
void Intel3aCore::enableAiqdDataSave(const bool enableAiqdDataSave)
{
    mEnableAiqdDataSave = enableAiqdDataSave;
}

/**
 *
 * Read the latest aiqd data from AIQ runtime and
 * save it to the host file system.
 *
 * \return true: aiqd data correctly read
 *         false: for any case with error
 */
bool Intel3aCore::saveAiqdData()
{
    LOG1("@%s", __FUNCTION__);
    ia_binary_data aiqdData;
    CLEAR(aiqdData);
    ia_err iaErr = ia_err_none;

    iaErr = ia_aiq_get_aiqd_data(mIaAiqHandle, &aiqdData);
    if (iaErr != ia_err_none
        || aiqdData.size == 0
        || aiqdData.data == nullptr) {
        LOGE("call ia_aiq_get_aiqd_data() fail, err:%d, size:%d, data:%p",
                iaErr, aiqdData.size, aiqdData.data);
        return false;
    }

    /* save aiqd data to variables which locate in the array defined in platformdata */
    PlatformData::saveAiqdData(mCameraId, aiqdData);

    return true;
}
} NAMESPACE_DECLARATION_END
