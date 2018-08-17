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

#define LOG_TAG "ParameterWorker"

#include <ia_cmc_types.h>
#include <ia_types.h>
#include <cpffData.h>
#include <Pipe.h>
#include <KBL_AIC.h>
#include <linux/intel-ipu3.h>

#include "ParameterWorker.h"
#include "PlatformData.h"
#include "SkyCamProxy.h"
#include "RuntimeParamsHelper.h"
#include "IPU3AicToFwEncoder.h"
#include "NodeTypes.h"

namespace android {
namespace camera2 {

const unsigned int PARA_WORK_BUFFERS = 1;

ParameterWorker::ParameterWorker(std::shared_ptr<cros::V4L2VideoNode> node,
                                 int cameraId,
                                 GraphConfig::PipeType pipeType) :
        FrameWorker(node, cameraId, PARA_WORK_BUFFERS, "ParameterWorker"),
        mPipeType(pipeType),
        mSkyCamAIC(nullptr),
        mCmcData(nullptr),
        mAicConfig(nullptr)
{
    LOG1("%s, mPipeType %d", __FUNCTION__, mPipeType);
    CLEAR(mIspPipes);
    CLEAR(mRuntimeParams);
    CLEAR(mCpfData);
    CLEAR(mGridInfo);
}

ParameterWorker::~ParameterWorker()
{
    LOG1("%s, mPipeType %d", __FUNCTION__, mPipeType);
    RuntimeParamsHelper::deleteAiStructs(mRuntimeParams);
    for (int i = 0; i < NUM_ISP_PIPES; i++) {
        delete mIspPipes[i];
        mIspPipes[i] = nullptr;
    }
}

status_t ParameterWorker::configure(std::shared_ptr<GraphConfig> &config)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t ret = OK;
    uintptr_t cmcHandle = reinterpret_cast<uintptr_t>(nullptr);

    if (PlatformData::getCpfAndCmc(mCpfData, &mCmcData, &cmcHandle, mCameraId) != OK) {
        LOGE("%s : Could not get cpf and cmc data",__FUNCTION__);
        return NO_INIT;
    }

    RuntimeParamsHelper::allocateAiStructs(mRuntimeParams);

    GraphConfig::NodesPtrVector sinks;
    std::string name = "csi_be:output";
    ret = config->graphGetDimensionsByName(name, mCsiBe.width, mCsiBe.height);
    if (ret != OK) {
        LOGE("Cannot find <%s> node", name.c_str());
        return ret;
    }

    ret = setGridInfo(mCsiBe.width);
    if (ret != OK) {
        return ret;
    }

    ia_aiq_frame_params sensorParams;
    config->getSensorFrameParams(sensorParams);

    PipeConfig pipeConfig;

    if (config->doesNodeExist("imgu:video")) {
        ret = getPipeConfig(pipeConfig, config, GC_VIDEO);
        if (ret != OK) {
            LOGE("Failed to get pipe config for video pipe");
            return ret;
        }
        overrideCPFFMode(&pipeConfig, config);
        fillAicInputParams(sensorParams, pipeConfig, mRuntimeParams);
    } else if (config->doesNodeExist("imgu:preview")) {
        ret = getPipeConfig(pipeConfig, config, GC_PREVIEW);
        if (ret != OK) {
            LOGE("Failed to get pipe config preview pipe");
            return ret;
        }
        overrideCPFFMode(&pipeConfig, config);
        fillAicInputParams(sensorParams, pipeConfig, mRuntimeParams);
    } else {
        LOGE("PipeType %d config is wrong", mPipeType);
        return BAD_VALUE;
    }

    for (int i = 0; i < NUM_ISP_PIPES; i++) {
        mIspPipes[i] = new IPU3ISPPipe;
    }

    ia_cmc_t* cmc = reinterpret_cast<ia_cmc_t*>(cmcHandle);

    AicMode aicMode = mPipeType == GraphConfig::PIPE_STILL ? AIC_MODE_STILL : AIC_MODE_VIDEO;
    if (mSkyCamAIC == nullptr) {
        mSkyCamAIC = SkyCamProxy::createProxy(mCameraId, aicMode, mIspPipes, NUM_ISP_PIPES, cmc, &mCpfData, &mRuntimeParams, 0, 0);
        CheckError((mSkyCamAIC == nullptr), NO_MEMORY, "Not able to create SkyCam AIC");
    }

    FrameInfo frame;
    int page_size = getpagesize();
    frame.width = sizeof(ipu3_uapi_params) + page_size - (sizeof(ipu3_uapi_params) % page_size);
    frame.height = 1;
    frame.stride = frame.width;
    frame.format = V4L2_META_FMT_IPU3_PARAMS;
    ret = setWorkerDeviceFormat(frame);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers(getDefaultMemoryType(IMGU_NODE_PARAM));
    if (ret != OK)
        return ret;

    ret = allocateWorkerBuffers(GRALLOC_USAGE_SW_WRITE_OFTEN |
                                GRALLOC_USAGE_HW_CAMERA_READ,
                                HAL_PIXEL_FORMAT_BLOB);
    if (ret != OK)
        return ret;

    mIndex = 0;

    return OK;
}

status_t ParameterWorker::setGridInfo(uint32_t csiBeWidth)
{
    if (csiBeWidth == 0) {
        LOGE("CSI BE width cannot be 0 - BUG");
        return BAD_VALUE;
    }
    mGridInfo.bds_padding_width = ALIGN128(csiBeWidth);

    return OK;
}

void ParameterWorker::dump()
{
    LOGD("dump mRuntimeParams");
    if (mRuntimeParams.awb_results)
        LOGD("  mRuntimeParams.awb_results: %f", mRuntimeParams.awb_results->accurate_b_per_g);
    if (mRuntimeParams.frame_resolution_parameters)
        LOGD("  mRuntimeParams.frame_resolution_parameters->BDS_horizontal_padding %d", mRuntimeParams.frame_resolution_parameters->BDS_horizontal_padding);
    if (mRuntimeParams.exposure_results)
        LOGD("  mRuntimeParams.exposure_results->analog_gain: %f", mRuntimeParams.exposure_results->analog_gain);
}

status_t ParameterWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    std::lock_guard<std::mutex> l(mParamsMutex);

    mMsg = msg;

    // Don't queue ISP parameter buffer if test pattern mode is used.
    if (mMsg->pMsg.processingSettings->captureSettings->testPatternMode
            != ANDROID_SENSOR_TEST_PATTERN_MODE_OFF) {
        return OK;
    }

    updateAicInputParams(mMsg, mRuntimeParams);
    if (mSkyCamAIC)
        mSkyCamAIC->Run(mRuntimeParams);
    mAicConfig = mSkyCamAIC->GetAicConfig();
    if (mAicConfig == nullptr) {
        LOGE("Could not get AIC config");
        return UNKNOWN_ERROR;
    }

    ipu3_uapi_params *ipu3Params = (ipu3_uapi_params*)mBufferAddr[mIndex];
    IPU3AicToFwEncoder::encodeParameters(mAicConfig, ipu3Params);

    status_t status = mNode->PutFrame(&mBuffers[mIndex]);
    if (status != OK) {
        LOGE("putFrame failed");
        return UNKNOWN_ERROR;
    }

    mIndex = (mIndex + 1) % mPipelineDepth;

    return OK;
}

status_t ParameterWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    // Don't dequeue ISP parameter buffer if test pattern mode is used.
    if (mMsg->pMsg.processingSettings->captureSettings->testPatternMode
            != ANDROID_SENSOR_TEST_PATTERN_MODE_OFF) {
        return OK;
    }

    cros::V4L2Buffer outBuf;

    status_t status = mNode->GrabFrame(&outBuf);
    if (status < 0) {
        LOGE("grabFrame failed");
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t ParameterWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mMsg = nullptr;
    return OK;
}

void ParameterWorker::updateAicInputParams(std::shared_ptr<DeviceMessage> msg, IPU3AICRuntimeParams &runtimeParams) const
{
    runtimeParams.manual_brightness = msg->pMsg.processingSettings->captureSettings->ispSettings.manualSettings.manualBrightness;
    runtimeParams.manual_contrast = msg->pMsg.processingSettings->captureSettings->ispSettings.manualSettings.manualContrast;
    runtimeParams.manual_hue = msg->pMsg.processingSettings->captureSettings->ispSettings.manualSettings.manualHue;
    runtimeParams.manual_saturation = msg->pMsg.processingSettings->captureSettings->ispSettings.manualSettings.manualSaturation;
    runtimeParams.manual_sharpness = msg->pMsg.processingSettings->captureSettings->ispSettings.manualSettings.manualSharpness;
    camera2::RuntimeParamsHelper::copyPaResults(runtimeParams, msg->pMsg.processingSettings->captureSettings->aiqResults.paResults);
    camera2::RuntimeParamsHelper::copySaResults(runtimeParams, msg->pMsg.processingSettings->captureSettings->aiqResults.saResults);
    camera2::RuntimeParamsHelper::copyWeightGrid(runtimeParams, msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.weight_grid);
    runtimeParams.isp_vamem_type = 0; //???

    ia_aiq_exposure_parameters *pExposure_parameters;
    pExposure_parameters = (ia_aiq_exposure_parameters*) runtimeParams.exposure_results;
    pExposure_parameters->exposure_time_us = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->exposure_time_us;
    pExposure_parameters->analog_gain = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->analog_gain;
    pExposure_parameters->aperture_fn = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->aperture_fn;
    pExposure_parameters->digital_gain = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->digital_gain;
    pExposure_parameters->iso = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->iso;
    pExposure_parameters->nd_filter_enabled = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->nd_filter_enabled;
    pExposure_parameters->total_target_exposure = msg->pMsg.processingSettings->captureSettings->aiqResults.aeResults.exposures->exposure->total_target_exposure;

    ia_aiq_awb_results *pAwb_results;
    pAwb_results = (ia_aiq_awb_results*) runtimeParams.awb_results;
    pAwb_results->accurate_b_per_g = msg->pMsg.processingSettings->captureSettings->aiqResults.awbResults.accurate_b_per_g;
    pAwb_results->accurate_r_per_g = msg->pMsg.processingSettings->captureSettings->aiqResults.awbResults.accurate_r_per_g;
    pAwb_results->cct_estimate = msg->pMsg.processingSettings->captureSettings->aiqResults.awbResults.cct_estimate;
    pAwb_results->distance_from_convergence = msg->pMsg.processingSettings->captureSettings->aiqResults.awbResults.distance_from_convergence;
    pAwb_results->final_b_per_g = msg->pMsg.processingSettings->captureSettings->aiqResults.awbResults.final_b_per_g;
    pAwb_results->final_r_per_g = msg->pMsg.processingSettings->captureSettings->aiqResults.awbResults.final_r_per_g;

    ia_aiq_gbce_results *pGbce_results;
    pGbce_results = (ia_aiq_gbce_results*) runtimeParams.gbce_results;
    pGbce_results->b_gamma_lut = msg->pMsg.processingSettings->captureSettings->aiqResults.gbceResults.b_gamma_lut;
    pGbce_results->g_gamma_lut = msg->pMsg.processingSettings->captureSettings->aiqResults.gbceResults.g_gamma_lut;
    pGbce_results->gamma_lut_size = msg->pMsg.processingSettings->captureSettings->aiqResults.gbceResults.gamma_lut_size;
    pGbce_results->r_gamma_lut = msg->pMsg.processingSettings->captureSettings->aiqResults.gbceResults.r_gamma_lut;
    pGbce_results->tone_map_lut = msg->pMsg.processingSettings->captureSettings->aiqResults.gbceResults.tone_map_lut;
    pGbce_results->tone_map_lut_size = msg->pMsg.processingSettings->captureSettings->aiqResults.gbceResults.tone_map_lut_size;
}

void ParameterWorker::fillAicInputParams(ia_aiq_frame_params &sensorFrameParams, PipeConfig &pipeCfg, IPU3AICRuntimeParams &runtimeParams) const
{
    aic_resolution_config_parameters_t *pAicResolutionConfigParams;
    ia_aiq_output_frame_parameters_t *pOutput_frame_params;
    aic_input_frame_parameters_t *pInput_frame_params;

    //Fill AIC input frame params
    pInput_frame_params = (aic_input_frame_parameters_t*)runtimeParams.input_frame_params;
    pInput_frame_params->sensor_frame_params = sensorFrameParams;
    pInput_frame_params->fix_flip_x = 0;
    pInput_frame_params->fix_flip_y = 0;
//    pInput_frame_params->sensor_frame_params.horizontal_scaling_numerator /= pModeData->binning_factor_x;
//    pInput_frame_params->sensor_frame_params.vertical_scaling_numerator /= pModeData->binning_factor_y;


    //Fill AIC output frame params
    pOutput_frame_params = (ia_aiq_output_frame_parameters_t*)runtimeParams.output_frame_params;

    pOutput_frame_params->height = runtimeParams.input_frame_params->sensor_frame_params.cropped_image_height;
    pOutput_frame_params->width = runtimeParams.input_frame_params->sensor_frame_params.cropped_image_width;

    pAicResolutionConfigParams = (aic_resolution_config_parameters_t*)runtimeParams.frame_resolution_parameters;
    // Temporary assigning values to m_AicResolutionConfigParams until KS property will give the information.
    // IF crop is the offset between the sensor output and the IF cropping.
    // Currently assuming that the ISP crops in the middle.
    // Need to consider bayer order

    pAicResolutionConfigParams->horizontal_IF_crop = ((pipeCfg.csi_be_width - pipeCfg.input_feeder_out_width) / 2);
    pAicResolutionConfigParams->vertical_IF_crop = ((pipeCfg.csi_be_height - pipeCfg.input_feeder_out_height) / 2);
    pAicResolutionConfigParams->BDSin_img_width = pipeCfg.input_feeder_out_width;
    pAicResolutionConfigParams->BDSin_img_height = pipeCfg.input_feeder_out_height;
    pAicResolutionConfigParams->BDSout_img_width = pipeCfg.bds_out_width;
    pAicResolutionConfigParams->BDSout_img_height = pipeCfg.bds_out_height;
    pAicResolutionConfigParams->BDS_horizontal_padding =
                          (uint16_t)(ALIGN128(pipeCfg.bds_out_width) - pipeCfg.bds_out_width);

    runtimeParams.frame_resolution_parameters = pAicResolutionConfigParams;

    LOGD("AIC res CFG params: IF Crop %dx%d, BDS In %dx%d, BDS Out %dx%d, BDS Padding %d",
            pAicResolutionConfigParams->horizontal_IF_crop,
            pAicResolutionConfigParams->vertical_IF_crop,
            pAicResolutionConfigParams->BDSin_img_width,
            pAicResolutionConfigParams->BDSin_img_height,
            pAicResolutionConfigParams->BDSout_img_width,
            pAicResolutionConfigParams->BDSout_img_height,
            pAicResolutionConfigParams->BDS_horizontal_padding);

    LOGD("Sensor/cio2 Output %dx%d, effective input %dx%d",
            pipeCfg.csi_be_width, pipeCfg.csi_be_height, pipeCfg.input_feeder_out_width, pipeCfg.input_feeder_out_height);

    runtimeParams.mode_index = pipeCfg.cpff_mode_hint;
}

status_t ParameterWorker::getPipeConfig(PipeConfig &pipeCfg, std::shared_ptr<GraphConfig> &config, const string &pin) const
{
    status_t ret = OK;

    string baseNode = string("imgu:");

    string node = baseNode + pin;

    int temp = 0;
    ret |= config->getValue(node, GCSS_KEY_CPFF_MODE_HINT, temp);
    pipeCfg.cpff_mode_hint = temp;

    ret |= config->getValue(node, GCSS_KEY_IMGU_PIPE_OUTPUT_ID, temp);
    pipeCfg.valid = temp;

    node = baseNode + pin + ":if";
    ret |= config->graphGetDimensionsByName(node, pipeCfg.input_feeder_out_width,
            pipeCfg.input_feeder_out_height);

    node = baseNode + pin + ":bds";
    ret |= config->graphGetDimensionsByName(node, pipeCfg.bds_out_width,
            pipeCfg.bds_out_height);

    node = baseNode + pin + ":gdc";
    ret |= config->graphGetDimensionsByName(node, pipeCfg.gdc_out_width,
            pipeCfg.gdc_out_height);

    node = baseNode + pin + ":yuv";
    ret |= config->graphGetDimensionsByName(node, pipeCfg.main_out_width,
            pipeCfg.main_out_height);

    node = baseNode + pin + ":filter";
    ret |= config->graphGetDimensionsByName(node, pipeCfg.filter_width,
            pipeCfg.filter_height);

    node = baseNode + pin + ":env";
    ret |= config->graphGetDimensionsByName(node, pipeCfg.envelope_width,
            pipeCfg.envelope_height);

    if (ret != OK) {
        LOGE("Cannot GraphConfig data.");
        return UNKNOWN_ERROR;
    }

    pipeCfg.view_finder_out_width = 0;
    pipeCfg.view_finder_out_height = 0;
    pipeCfg.csi_be_height = mCsiBe.height;
    pipeCfg.csi_be_width = mCsiBe.width;

    return ret;
}

void ParameterWorker::overrideCPFFMode(PipeConfig *pipeCfg, std::shared_ptr<GraphConfig> &config)
{
    if (pipeCfg == nullptr)
        return;

    /* Due to suppport 360 degree orientation, so width is less than
     * height in portrait mode, need to use max length between width
     * and height to do comparison.
     */
    int maxLength = MAX(pipeCfg->main_out_width, pipeCfg->main_out_height);
    if (maxLength > RESOLUTION_1080P_WIDTH) {
        pipeCfg->cpff_mode_hint = CPFF_MAIN;
    } else if (maxLength > RESOLUTION_720P_WIDTH) {
        pipeCfg->cpff_mode_hint = CPFF_FHD;
    } else if (maxLength > RESOLUTION_VGA_WIDTH) {
        pipeCfg->cpff_mode_hint = CPFF_HD;
    } else {
        pipeCfg->cpff_mode_hint = CPFF_VGA;
    }
    LOG2("%s final cpff mode %d", __FUNCTION__, pipeCfg->cpff_mode_hint);
}

} /* namespace camera2 */
} /* namespace android */
