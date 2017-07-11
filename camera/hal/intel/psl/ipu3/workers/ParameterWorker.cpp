/*
 * Copyright (C) 2017 Intel Corporation.
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

namespace android {
namespace camera2 {

ParameterWorker::ParameterWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId) :
        FrameWorker(node, cameraId, "ParameterWorker"),
        mSkyCamAIC(nullptr),
        mCmcData(nullptr),
        mAicConfig(nullptr)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    CLEAR(mIspPipes);
    CLEAR(mStillRuntimeParams);
    CLEAR(mPreviewRuntimeParams);
    CLEAR(mStillParams);
    mStillParamsReady = false;
}

ParameterWorker::~ParameterWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    RuntimeParamsHelper::deleteAiStructs(mStillRuntimeParams);
    RuntimeParamsHelper::deleteAiStructs(mPreviewRuntimeParams);
    for (int i = 0; i < NUM_ISP_PIPES; i++) {
        delete mIspPipes[i];
        mIspPipes[i] = nullptr;
    }
}

int ParameterWorker::allocLscTable(IPU3AICRuntimeParams & runtime)
{
    ia_aiq_sa_results *sa_results = new ia_aiq_sa_results;
    int  LscSize = 0;
    memset(sa_results, 0, sizeof(ia_aiq_sa_results));

    if(mCmcData != nullptr){
         LscSize = mCmcData->cmc_parsed_lens_shading.cmc_lens_shading->grid_height *
                      mCmcData->cmc_parsed_lens_shading.cmc_lens_shading->grid_width ;
        LOG2("%s alloc lsc for runtime size %d",__FUNCTION__, LscSize);
        sa_results->lsc_grid[0][0] =  new unsigned short[LscSize];
        sa_results->lsc_grid[0][1] =  new unsigned short[LscSize];
        sa_results->lsc_grid[1][0] =  new unsigned short[LscSize];
        sa_results->lsc_grid[1][1] =  new unsigned short[LscSize];
        sa_results->width = mCmcData->cmc_parsed_lens_shading.cmc_lens_shading->grid_width;
        sa_results->height = mCmcData->cmc_parsed_lens_shading.cmc_lens_shading->grid_height;
    }
    runtime.sa_results = sa_results;

    return LscSize;
}

status_t ParameterWorker::setStillParam(ipu3_uapi_params *param)
{
    std::lock_guard<std::mutex> l(mStillParamsMutex);

    if (param == nullptr) {
      mStillParamsReady = false;
      CLEAR(mStillParams);
      return NO_ERROR;
    }

    MEMCPY_S((void *)&mStillParams, sizeof(ipu3_uapi_params),
        (void *)param, sizeof(ipu3_uapi_params));
    mStillParamsReady = true;

    return NO_ERROR;
}

status_t ParameterWorker::configure(std::shared_ptr<GraphConfig> &config)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t ret = OK;

    mStillParamsReady = false;

    if (PlatformData::getCpfAndCmc(mCpfData, mCmcData, mCameraId) != OK) {
        LOGE("%s : Could not get cpf and cmc data",__FUNCTION__);
        return NO_INIT;
    }

    ret = RuntimeParamsHelper::allocateAiStructs(mStillRuntimeParams);
    if (ret != OK) {
        LOGE("Cannot allocate AIC struct");
        RuntimeParamsHelper::deleteAiStructs(mStillRuntimeParams);
        return ret;
    }

    ret = allocLscTable(mStillRuntimeParams);
    if (ret < 0) {
        LOGE("%s : Cannot allocate Sa struct",__FUNCTION__);
        RuntimeParamsHelper::deleteAiStructs(mStillRuntimeParams);
        return ret;
    }


    ret = RuntimeParamsHelper::allocateAiStructs(mPreviewRuntimeParams);
    if (ret != OK) {
        LOGE("Cannot allocate AIC struct");
        RuntimeParamsHelper::deleteAiStructs(mStillRuntimeParams);
        RuntimeParamsHelper::deleteAiStructs(mPreviewRuntimeParams);
        return ret;
    }


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

    bool foundPreview = config->doesNodeExist("imgu:preview");
    if (foundPreview) {
        ret = getPipeConfig(mPreviewPipeConfig, config, GC_PREVIEW);
        if (ret != OK) {
            LOGE("Failed to get pipe config preview pipe");
            return ret;
        }
        fillAicInputParams(sensorParams, mPreviewPipeConfig, mPreviewRuntimeParams);
    }

    bool foundStill = config->doesNodeExist("imgu:video");
    if (foundStill) {
        ret = getPipeConfig(mStillPipeConfig, config, GC_VIDEO);
        if (ret != OK) {
            LOGE("Failed to get pipe config for video pipe");
            return ret;
        }
        fillAicInputParams(sensorParams, mStillPipeConfig, mStillRuntimeParams);

        foundPreview = false;
    }

    for (int i = 0; i < NUM_ISP_PIPES; i++) {
        mIspPipes[i] = new IPU3ISPPipe;
    }

    if (foundStill) {
        if (mSkyCamAIC == nullptr) {
            mSkyCamAIC = SkyCamProxy::createProxy(mCameraId, mIspPipes, NUM_ISP_PIPES, mCmcData, &mCpfData, &mStillRuntimeParams, 0, 0);
            if (mSkyCamAIC == nullptr) {
                LOGE("Not able to create SkyCam AIC");
                return NO_MEMORY;
            }
        }
    }

    if (foundPreview) {
        if (mSkyCamAIC == nullptr) {
            mSkyCamAIC = SkyCamProxy::createProxy(mCameraId, mIspPipes, NUM_ISP_PIPES, mCmcData, &mCpfData, &mPreviewRuntimeParams, 0, 0);
            if (mSkyCamAIC == nullptr) {
                LOGE("Not able to create SkyCam AIC");
                return NO_MEMORY;
            }
        }
    }

    FrameInfo frame;
    int page_size = getpagesize();
    frame.width = sizeof(ipu3_uapi_params) + page_size - (sizeof(ipu3_uapi_params) % page_size);
    frame.height = 1;
    frame.stride = frame.width;
    frame.format = V4L2_META_FMT_IPU3_PARAMS;
    ret = setWorkerDeviceFormat(V4L2_BUF_TYPE_VIDEO_OUTPUT, frame);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers();
    if (ret != OK)
        return ret;

    ret = allocateWorkerBuffers();
    if (ret != OK)
        return ret;

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
    LOGD("mRuntimeParams");
    if (mStillRuntimeParams.awb_results)
        LOGD("  mRuntimeParams.awb_results: %f", mStillRuntimeParams.awb_results->accurate_b_per_g);
    if (mStillRuntimeParams.frame_resolution_parameters)
        LOGD("  mRuntimeParams.frame_resolution_parameters->BDS_horizontal_padding %d", mStillRuntimeParams.frame_resolution_parameters->BDS_horizontal_padding);
    if (mStillRuntimeParams.exposure_results)
        LOGD("  mRuntimeParams.exposure_results->analog_gain: %f", mStillRuntimeParams.exposure_results->analog_gain);
}

status_t ParameterWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    std::lock_guard<std::mutex> l(mStillParamsMutex);

    if (mStillParamsReady) {
        LOG2("%s copy the still param, mBuffers size %d, user ptr: %p, still ptr: %p, %d!",
          __FUNCTION__, mBuffers.size(), mBuffers[0].m.userptr, &mStillParams, sizeof(ipu3_uapi_params));
        MEMCPY_S((void *)mBuffers[0].m.userptr, sizeof(ipu3_uapi_params),
            (void *)&mStillParams, sizeof(ipu3_uapi_params));
    } else {
        mMsg = msg;

        updateAicInputParams(mMsg, mStillRuntimeParams);
        if (mSkyCamAIC)
            mSkyCamAIC->Run(mStillRuntimeParams);
        mAicConfig = mSkyCamAIC->GetAicConfig();
        if (mAicConfig == nullptr) {
            LOGE("Could not get AIC config");
            return UNKNOWN_ERROR;
        }

        ipu3_uapi_params *ipu3Params = (ipu3_uapi_params*)mBuffers[0].m.userptr;
        IPU3AicToFwEncoder::encodeParameters(mAicConfig, ipu3Params);

        ICaptureEventListener::CaptureMessage outMsg;
        outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
        outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_PARAM;
        std::shared_ptr<ipu3_uapi_params> lParams = std::make_shared<ipu3_uapi_params>();
        LOG2("%s return the parameter %p to control unit!", __FUNCTION__, lParams.get());
        MEMCPY_S(lParams.get(), sizeof(ipu3_uapi_params), (void*)ipu3Params, sizeof(ipu3_uapi_params));
        outMsg.data.event.param = lParams;
        outMsg.data.event.reqId = msg->pMsg.processingSettings->request->getId();

        notifyListeners(&outMsg);
    }


    status_t status = mNode->putFrame(&mBuffers[0]);
    if (status != OK) {
        LOGE("putFrame failed");
        return UNKNOWN_ERROR;
    }

    return OK;
}

status_t ParameterWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    v4l2_buffer_info outBuf;
    CLEAR(outBuf);

    status_t status = mNode->grabFrame(&outBuf);
    if (status != OK) {
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

    pAicResolutionConfigParams->horizontal_IF_crop = ((pipeCfg.csi_be_width / 2 - pipeCfg.input_feeder_out_width / 2) / 2);
    pAicResolutionConfigParams->vertical_IF_crop = ((pipeCfg.csi_be_height / 2 - pipeCfg.input_feeder_out_height / 2) / 2);
    pAicResolutionConfigParams->BDSin_img_width = pipeCfg.input_feeder_out_width / 2;
    pAicResolutionConfigParams->BDSin_img_height = pipeCfg.input_feeder_out_height / 2;
    pAicResolutionConfigParams->BDSout_img_width = pipeCfg.bds_out_width / 2;
    pAicResolutionConfigParams->BDSout_img_height = pipeCfg.bds_out_height / 2;
    pAicResolutionConfigParams->BDS_horizontal_padding = (uint16_t)(mGridInfo.bds_padding_width / 2);

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

} /* namespace camera2 */
} /* namespace android */
