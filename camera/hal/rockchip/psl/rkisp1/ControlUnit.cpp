/*
 * Copyright (C) 2017 Intel Corporation.
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

#define LOG_TAG "ControlUnit"

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "ControlUnit.h"
#include "RKISP1CameraHw.h"
#include "ImguUnit.h"
#include "CameraStream.h"
#include "RKISP1CameraCapInfo.h"
#include "CameraMetadataHelper.h"
#include "PlatformData.h"
#include "ProcUnitSettings.h"
#include "LensHw.h"
#include "SettingsProcessor.h"
#include "Metadata.h"
#include "Rk3aRunner.h"

static const int SETTINGS_POOL_SIZE = MAX_REQUEST_IN_PROCESS_NUM * 2;

namespace android {
namespace camera2 {

ControlUnit::ControlUnit(ImguUnit *thePU,
                         int cameraId,
                         IStreamConfigProvider &aStreamCfgProv,
                         std::shared_ptr<MediaController> mc) :
        mRequestStatePool("CtrlReqState"),
        mCaptureUnitSettingsPool("CapUSettings"),
        mProcUnitSettingsPool("ProcUSettings"),
        mLatestRequestId(-1),
        mImguUnit(thePU),
        m3aWrapper(nullptr),
        mCameraId(cameraId),
        mMediaCtl(mc),
        mCameraThread("CtrlUThread"),
        mStreamCfgProv(aStreamCfgProv),
        mSettingsProcessor(nullptr),
        mMetadata(nullptr),
        m3ARunner(nullptr),
        mSensorSettingsDelay(0),
        mGainDelay(0),
        mLensSupported(false),
        mLensController(nullptr),
        mSofSequence(0),
        mShutterDoneReqId(-1)
{
}

/**
 * initStaticMetadata
 *
 * Create CameraMetadata object to retrieve the static tags used in this class
 * we cache them as members so that we do not need to query CameraMetadata class
 * everytime we need them. This is more efficient since find() is not cheap
 */
status_t ControlUnit::initStaticMetadata()
{
    //Initialize the CameraMetadata object with the static metadata tags
    camera_metadata_t* plainStaticMeta;
    plainStaticMeta = (camera_metadata_t*)PlatformData::getStaticMetadata(mCameraId);
    if (plainStaticMeta == nullptr) {
        LOGE("Failed to get camera %d StaticMetadata", mCameraId);
        return UNKNOWN_ERROR;
    }

    CameraMetadata staticMeta(plainStaticMeta);
    camera_metadata_entry entry;
    entry = staticMeta.find(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE);
    if (entry.count == 1) {
        LOG1("camera %d minimum focus distance:%f", mCameraId, entry.data.f[0]);
        mLensSupported = (entry.data.f[0] > 0) ? true : false;
        LOG1("Lens movement %s for camera id %d",
             mLensSupported ? "supported" : "NOT supported", mCameraId);
    }
    staticMeta.release();

    const RKISP1CameraCapInfo *cap = getRKISP1CameraCapInfo(mCameraId);
    if (cap == nullptr) {
        LOGE("Failed to get cameraCapInfo");
        return UNKNOWN_ERROR;
    }
    mSensorSettingsDelay = MAX(cap->mExposureLag, cap->mGainLag);
    mGainDelay = cap->mGainLag;

    return NO_ERROR;
}

status_t
ControlUnit::init()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;
    const char *sensorName = nullptr;

    //Cache the static metadata values we are going to need in the capture unit
    if (initStaticMetadata() != NO_ERROR) {
        LOGE("Cannot initialize static metadata");
        return NO_INIT;
    }

    ISofListener *sofListener = NULL;
    ISettingsSyncListener *syncListener = NULL;
    mSyncManager = std::make_shared<SyncManager>(mCameraId, mMediaCtl, sofListener, syncListener);

    status = mSyncManager->init(mSensorSettingsDelay, mGainDelay);
    if (status != NO_ERROR) {
        LOGE("Cannot initialize SyncManager (status= 0x%X)", status);
        return status;
    }

    if (!mLensSupported) {
        mLensController = nullptr;
    } else {
        mLensController = std::make_shared<LensHw>(mCameraId, mMediaCtl);
        if (mLensController == nullptr) {
            LOGE("@%s: Error creating LensHw", __FUNCTION__);
            return NO_INIT;
        }

        status = mLensController->init();
        if (status != NO_ERROR) {
            LOGE("%s:Cannot initialize LensHw (status= 0x%X)", __FUNCTION__, status);
            return status;
        }
    }

    if (!mCameraThread.Start()) {
        LOGE("Camera thread failed to start");
        return UNKNOWN_ERROR;
    }

    const RKISP1CameraCapInfo *cap = getRKISP1CameraCapInfo(mCameraId);
    if (!cap) {
        LOGE("Not enough information for getting NVM data");
    } else {
        sensorName = cap->getSensorName();
    }

    if (!cap || cap->sensorType() == SENSOR_TYPE_RAW) {
        m3aWrapper = new Rk3aPlus(mCameraId);
    } else {
        LOGE("SoC camera 3A control missing");
        return UNKNOWN_ERROR;
    }

    if (m3aWrapper->initAIQ(sensorName) != NO_ERROR) {
        LOGE("Error initializing 3A control");
        return UNKNOWN_ERROR;
    }

    mSettingsProcessor = new SettingsProcessor(mCameraId, m3aWrapper, mStreamCfgProv);
    mSettingsProcessor->init();

    mMetadata = new Metadata(mCameraId, m3aWrapper);
    status = mMetadata->init();
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Error Initializing metadata");
        return UNKNOWN_ERROR;
    }

    /*
     * init the pools of Request State structs and CaptureUnit settings
     * and Processing Unit Settings
     */
    mRequestStatePool.init(MAX_REQUEST_IN_PROCESS_NUM, RequestCtrlState::reset);
    mCaptureUnitSettingsPool.init(SETTINGS_POOL_SIZE + 2);
    mProcUnitSettingsPool.init(SETTINGS_POOL_SIZE, ProcUnitSettings::reset);

    mSettingsHistory.clear();

    m3ARunner = new Rk3aRunner(mCameraId, m3aWrapper, mSettingsProcessor, mLensController.get());

    /* Set digi gain support */
    bool supportDigiGain = false;
    if (cap)
        supportDigiGain = cap->digiGainOnSensor();

    status = m3ARunner->init();
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Error Initializing 3A Runner");
        return UNKNOWN_ERROR;
    }

    return status;
}

/**
 * reset
 *
 * This method is called by the SharedPoolItem when the item is recycled
 * At this stage we can cleanup before recycling the struct.
 * In this case we reset the TracingSP of the capture unit settings and buffers
 * to remove this reference. Other references may be still alive.
 */
void RequestCtrlState::reset(RequestCtrlState* me)
{
    if (CC_LIKELY(me != nullptr)) {
        me->captureSettings.reset();
        me->processingSettings.reset();
        me->graphConfig.reset();
    } else {
        LOGE("Trying to reset a null CtrlState structure !! - BUG ");
    }
}

void RequestCtrlState::initAAAControls()
{
    const CameraMetadata* settings = request->getSettings();
    if (CC_UNLIKELY(settings == nullptr)) {
        LOGE("no settings in request - BUG");
        return;
    }

    camera_metadata_ro_entry entry;
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        aaaControls.controlMode = entry.data.u8[0];
    }
    entry = settings->find(ANDROID_CONTROL_AE_ANTIBANDING_MODE);
    if (entry.count == 1) {
        aaaControls.ae.aeAntibanding = entry.data.u8[0];
    }
    entry = settings->find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    if (entry.count == 1) {
        aaaControls.ae.evCompensation = entry.data.i32[0];
    }
    entry = settings->find(ANDROID_CONTROL_AE_LOCK);
    if (entry.count == 1) {
        aaaControls.ae.aeLock = entry.data.u8[0];
    }
    /* entry = settings->find(ANDROID_CONTROL_AE_REGIONS); */
    /* if (entry.count == 1) { */
    /*     aaaControls.ae.aeMode = entry.data.u8[0]; */
    /* } */
    entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    if (entry.count > 1) {
        aaaControls.ae.aeTargetFpsRange[0] = entry.data.i32[0];
        aaaControls.ae.aeTargetFpsRange[1] = entry.data.i32[1];
    }
    entry = settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
    if (entry.count == 1) {
        aaaControls.ae.aePreCaptureTrigger = entry.data.u8[0];
    }
    entry = settings->find(ANDROID_CONTROL_AWB_LOCK);
    if (entry.count == 1) {
        aaaControls.awb.awbLock = entry.data.u8[0];
    }
    entry = settings->find(ANDROID_CONTROL_AWB_MODE);
    if (entry.count == 1) {
        aaaControls.awb.awbMode = entry.data.u8[0];
    }
    entry = settings->find(ANDROID_COLOR_CORRECTION_MODE);
    if (entry.count == 1) {
        aaaControls.awb.colorCorrectionMode = entry.data.u8[0];
    }
    entry = settings->find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
    if (entry.count == 1) {
        aaaControls.awb.colorCorrectionAberrationMode = entry.data.u8[0];
    }
    /* entry = settings->find(ANDROID_CONTROL_AWB_REGIONS); */
    /* if (entry.count == 1) { */
    /*     aaaControls.awb.awbMode = entry.data.u8[0]; */
    /* } */
}

void RequestCtrlState::init(Camera3Request *req,
                                         std::shared_ptr<GraphConfig> aGraphConfig)
{
    request = req;
    graphConfig = aGraphConfig;
    aiqInputParams.init();
    if (CC_LIKELY(captureSettings)) {
        /* captureSettings->aiqResults.init(); */
        CLEAR(captureSettings->aiqResults);
        captureSettings->aeRegion.init(0);
        captureSettings->makernote.data = nullptr;
        captureSettings->makernote.size = 0;
    } else {
        LOGE(" Failed to init Ctrl State struct: no capture settings!! - BUG");
        return;
    }
    if (CC_LIKELY(processingSettings.get() != nullptr)) {
        processingSettings->captureSettings = captureSettings;
        processingSettings->graphConfig = aGraphConfig;
        processingSettings->request = req;
    } else {
        LOGE(" Failed to init Ctrl State: no processing settings!! - BUG");
        return;
    }
    aeState = ALGORITHM_NOT_CONFIG;
    awbState = ALGORITHM_NOT_CONFIG;
    ctrlUnitResult = request->getPartialResultBuffer(CONTROL_UNIT_PARTIAL_RESULT);
    statsArrived = false;
    framesArrived = 0;
    shutterDone = false;
    blackLevelOff = false;
    intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
    aaaControls.ae.aeMode =  ANDROID_CONTROL_AE_MODE_ON;
    aaaControls.awb.awbMode =  ANDROID_CONTROL_AWB_MODE_AUTO;
    aaaControls.controlMode =  ANDROID_CONTROL_MODE_AUTO;
    initAAAControls();

    tonemapContrastCurve = false;
    rGammaLutSize = 0;
    gGammaLutSize = 0;
    bGammaLutSize = 0;

    if (CC_UNLIKELY(ctrlUnitResult == nullptr)) {
        LOGE("no partial result buffer - BUG");
        return;
    }

    /**
     * Apparently we need to have this tags in the results
     */
    const CameraMetadata* settings = request->getSettings();

    if (CC_UNLIKELY(settings == nullptr)) {
        LOGE("no settings in request - BUG");
        return;
    }

    camera_metadata_ro_entry entry;
    entry = settings->find(ANDROID_REQUEST_ID);
    if (entry.count == 1) {
        ctrlUnitResult->update(ANDROID_REQUEST_ID, entry.data.i32,
                entry.count);
    }
    int64_t id = request->getId();
    ctrlUnitResult->update(ANDROID_SYNC_FRAME_NUMBER,
                          &id, 1);

    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count == 1) {
        intent = entry.data.u8[0];
    }
    LOG2("%s:%d: request id(%lld), capture_intent(%d)", __func__, __LINE__, id, intent);
    ctrlUnitResult->update(ANDROID_CONTROL_CAPTURE_INTENT, entry.data.u8,
                                                           entry.count);
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        aaaControls.controlMode = entry.data.u8[0];
        ctrlUnitResult->update(ANDROID_CONTROL_MODE, entry.data.u8, entry.count);
    }

    entry = settings->find(ANDROID_CONTROL_AE_MODE);
    if (entry.count == 1) {
        aaaControls.ae.aeMode = entry.data.u8[0];
        ctrlUnitResult->update(ANDROID_CONTROL_AE_MODE, entry.data.u8, entry.count);
    }

    /**
     * We don't have AF, so just update metadata now
     */
    entry = settings->find(ANDROID_CONTROL_AF_MODE);
    if (entry.count > 0)
        ctrlUnitResult->update(entry);

    uint8_t afTrigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    ctrlUnitResult->update(ANDROID_CONTROL_AF_TRIGGER, &afTrigger, 1);

    uint8_t afState = ANDROID_CONTROL_AF_STATE_INACTIVE;
    ctrlUnitResult->update(ANDROID_CONTROL_AF_STATE, &afState, 1);
}

ControlUnit::~ControlUnit()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    mSettingsHistory.clear();

    mCameraThread.Stop();

    if (mSettingsProcessor) {
        delete mSettingsProcessor;
        mSettingsProcessor = nullptr;
    }

    if (m3aWrapper) {
        m3aWrapper->deinit();
        delete m3aWrapper;
        m3aWrapper = nullptr;
    }

    if(mSyncManager) {
        mSyncManager->stop();
        mSyncManager = nullptr;
    }

    delete mMetadata;
    mMetadata = nullptr;

    delete m3ARunner;
    m3ARunner = nullptr;
}

status_t
ControlUnit::configStreamsDone(bool configChanged)
{
    LOG1("@%s: config changed: %d", __FUNCTION__, configChanged);

    if (configChanged) {
        mLatestRequestId = -1;
        mWaitingForCapture.clear();
        mSettingsHistory.clear();
        /*stop here? zyc*/
        mSyncManager->stop();

        /* get sensor mode here ? zyc */
        ICaptureEventListener::CaptureMessage outMsg;
        outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
        outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_NEW_SENSOR_DESCRIPTOR;

        status_t status = getSensorModeData(outMsg.data.event.exposureDesc);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Failed to retrieve sensor mode data - BUG");
            return status;
        }

        //outMsg.data.event.frameParams = isysConfigResult.sensorFrameParams;
        notifyCaptureEvent(&outMsg);
    }

    return NO_ERROR;
}

/**
 * acquireRequestStateStruct
 *
 * acquire a free request control state structure.
 * Since this structure contains also a capture settings item that are also
 * stored in a pool we need to acquire one of those as well.
 *
 */
status_t
ControlUnit::acquireRequestStateStruct(std::shared_ptr<RequestCtrlState>& state)
{
    status_t status = NO_ERROR;
    status = mRequestStatePool.acquireItem(state);
    if (status != NO_ERROR) {
        LOGE("Failed to acquire free request state struct - BUG");
        /*
         * This should not happen since AAL is holding clients to send more
         * requests than we can take
         */
        return UNKNOWN_ERROR;
    }

    status = mCaptureUnitSettingsPool.acquireItem(state->captureSettings);
    if (status != NO_ERROR) {
        LOGE("Failed to acquire free CapU settings  struct - BUG");
        /*
         * This should not happen since AAL is holding clients to send more
         * requests than we can take
         */
        return UNKNOWN_ERROR;
    }

    // set a unique ID for the settings
    state->captureSettings->settingsIdentifier = systemTime();

    status = mProcUnitSettingsPool.acquireItem(state->processingSettings);
    if (status != NO_ERROR) {
        LOGE("Failed to acquire free ProcU settings  struct - BUG");
        /*
         * This should not happen since AAL is holding clients to send more
         * requests than we can take
         */
        return UNKNOWN_ERROR;
    }
    return OK;
}

/**
 * processRequest
 *
 * Acquire the control structure to keep the state of the request in the control
 * unit and send the message to be handled in the internal message thread.
 */
status_t
ControlUnit::processRequest(Camera3Request* request,
                            std::shared_ptr<GraphConfig> graphConfig)
{
    status_t status = NO_ERROR;
    std::shared_ptr<RequestCtrlState> state;
    LOG2("@%s: id %d", __FUNCTION__, request->getId());

    status = acquireRequestStateStruct(state);
    if (CC_UNLIKELY(status != OK) || CC_UNLIKELY(state.get() == nullptr)) {
        return status;  // error log already done in the helper method
    }

    state->init(request, graphConfig);

    base::Callback<status_t()> closure =
            base::Bind(&ControlUnit::handleNewRequest, base::Unretained(this),
                       base::Passed(std::move(state)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return OK;
}

status_t ControlUnit::handleNewRequest(std::shared_ptr<RequestCtrlState> state)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    status_t status = NO_ERROR;
    std::shared_ptr<RequestCtrlState> reqState = state;

    /**
     * PHASE 1: Process the settings
     * In this phase we analyze the request's metadata settings and convert them
     * into either:
     *  - input parameters for 3A algorithms
     *  - parameters used for SoC sensors
     *  - Capture Unit settings
     *  - Processing Unit settings
     */
    const CameraMetadata *reqSettings = reqState->request->getSettings();

    if (reqSettings == nullptr) {
        LOGE("no settings in request - BUG");
        return UNKNOWN_ERROR;
    }

    status = mSettingsProcessor->processRequestSettings(*reqSettings, *reqState);
    if (status != NO_ERROR) {
        LOGE("Could not process all settings, reporting request as invalid");
    }

    status = processRequestForCapture(reqState);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to process req %d for capture [%d]",
        reqState->request->getId(), status);
        // TODO: handle error !
    }

    return status;
}

void dumpAEC(rk_aiq_ae_results aec_result)
{
    LOG2("AecResultDump:exposure(%d,%f,%f,%d), sensor_exposure(%d,%d,%d,%d,%d,%d), aec_config_result:enabled(%d) win(%d,%d,%d,%d) mode(%d)",
        aec_result.exposure.exposure_time_us,
        aec_result.exposure.analog_gain,
        aec_result.exposure.digital_gain,
        aec_result.exposure.iso,

        aec_result.sensor_exposure.fine_integration_time,
        aec_result.sensor_exposure.coarse_integration_time,
        aec_result.sensor_exposure.analog_gain_code_global,
        aec_result.sensor_exposure.digital_gain_global,
        aec_result.sensor_exposure.line_length_pixels,
        aec_result.sensor_exposure.frame_length_lines,

        aec_result.aec_config_result.enabled,
        aec_result.aec_config_result.win.h_offset,
        aec_result.aec_config_result.win.v_offset,
        aec_result.aec_config_result.win.width,
        aec_result.aec_config_result.win.height,
        aec_result.aec_config_result.mode);
}
void dumpAWB(rk_aiq_awb_results awb_result)
{
    LOG2("AwbResultDump:enabled(%d), awb_meas_mode(%d), awb_meas_cfg(%d,%d,%d,%d,%d,%d), awb_win(%d,%d,%d,%d),gain(%d,%d,%d,%d),gain enable(%d)",
        awb_result.awb_meas_cfg.enabled,
        awb_result.awb_meas_cfg.awb_meas_mode,

        awb_result.awb_meas_cfg.awb_meas_cfg.max_y,
        awb_result.awb_meas_cfg.awb_meas_cfg.ref_cr_max_r,
        awb_result.awb_meas_cfg.awb_meas_cfg.min_y_max_g,
        awb_result.awb_meas_cfg.awb_meas_cfg.ref_cb_max_b,
        awb_result.awb_meas_cfg.awb_meas_cfg.max_c_sum,
        awb_result.awb_meas_cfg.awb_meas_cfg.min_c,

        awb_result.awb_meas_cfg.awb_win.h_offset,
        awb_result.awb_meas_cfg.awb_win.v_offset,
        awb_result.awb_meas_cfg.awb_win.width,
        awb_result.awb_meas_cfg.awb_win.height,

        awb_result.awb_gain_cfg.awb_gains.red_gain,
        awb_result.awb_gain_cfg.awb_gains.green_r_gain,
        awb_result.awb_gain_cfg.awb_gains.green_b_gain,
        awb_result.awb_gain_cfg.awb_gains.blue_gain,
        awb_result.awb_gain_cfg.enabled
        );
}

void dumpMISC(rk_aiq_misc_isp_results aiqresults)
{
}

void dump3A(struct AiqResults aiqresults)
{
    dumpAEC(aiqresults.aeResults);
    dumpAWB(aiqresults.awbResults);
}


/**
 * processRequestForCapture
 *
 * Run 3A algorithms and send the results to capture unit for capture
 *
 * This is the second phase in the request processing flow.
 *
 * The request settings have been processed in the first phase
 *
 * If this step is successful the request will be moved to the
 * mWaitingForCapture waiting for the pixel buffers.
 */
status_t
ControlUnit::processRequestForCapture(std::shared_ptr<RequestCtrlState> &reqState)
{
    status_t status = NO_ERROR;
    if (CC_UNLIKELY(reqState.get() == nullptr)) {
        LOGE("Invalid parameters passed- request not captured - BUG");
        return BAD_VALUE;
    }

    if (CC_UNLIKELY(reqState->captureSettings.get() == nullptr)) {
        LOGE("capture Settings not given - BUG");
        return BAD_VALUE;
    }

    /* Write the dump flag into capture settings, so that the PAL dump can be
     * done all the way down at PgParamAdaptor. For the time being, only dump
     * during jpeg captures.
     */
    reqState->processingSettings->dump =
            LogHelper::isDumpTypeEnable(CAMERA_DUMP_RAW) &&
            reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB) > 0;
    // dump the PAL run from ISA also
    reqState->captureSettings->dump = reqState->processingSettings->dump;

    int reqId = reqState->request->getId();

    /**
     * Move the request to the vector mWaitingForCapture
     */
    mWaitingForCapture.insert(std::make_pair(reqId, reqState));
    if (mLatestRequestId < 0)
    {
        //handle the first request
        MessageStats msg;
        msg.stats = NULL;
        handleNewStat(msg);
    }

    mLatestRequestId = reqId;

    int jpegBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);
    int implDefinedBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    int yuv888BufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_YCbCr_420_888);
    LOG2("@%s jpegs:%d impl defined:%d yuv888:%d inputbufs:%d req id %d",
         __FUNCTION__,
         jpegBufCount,
         implDefinedBufCount,
         yuv888BufCount,
         reqState->request->getNumberInputBufs(),
         reqState->request->getId());
    if (jpegBufCount > 0) {
        // NOTE: Makernote should be get after isp_bxt_run()
        // NOTE: makernote.data deleted in JpegEncodeTask::handleMakernote()
        /* TODO */
        /* const unsigned mknSize = MAKERNOTE_SECTION1_SIZE + MAKERNOTE_SECTION2_SIZE; */
        /* MakernoteData mkn = {nullptr, mknSize}; */
        /* mkn.data = new char[mknSize]; */
        /* m3aWrapper->getMakerNote(ia_mkn_trg_section_2, mkn); */

        /* reqState->captureSettings->makernote = mkn; */

    } else {
        // No JPEG buffers in request. Reset MKN info, just in case.
        reqState->captureSettings->makernote.data = nullptr;
        reqState->captureSettings->makernote.size = 0;
    }

    bool started = false;
    mSyncManager->isStarted(started);
    if (!started) {
        LOG1("@%s: Starting SyncManager", __FUNCTION__);
        mSyncManager->start();
    }

    /*TODO, needn't this anymore ? zyc*/
    reqState->framesArrived++;
    status = completeProcessing(reqState);
    if (status != OK)
        LOGE("Cannot complete the buffer processing - fix the bug!");

    return status;
}

status_t
ControlUnit::applyAeParams(std::shared_ptr<CaptureUnitSettings> &aiqCaptureSettings)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    return mSyncManager->setParameters(aiqCaptureSettings);
}

status_t ControlUnit::fillMetadata(std::shared_ptr<RequestCtrlState> &reqState)
{
    mMetadata->writeMiscMetadata(*reqState);
    mMetadata->writeJpegMetadata(*reqState);

    mMetadata->writeAwbMetadata(*reqState);
    mMetadata->writeSensorMetadata(*reqState);
    mMetadata->writeLensMetadata(*reqState);
    mMetadata->writeLSCMetadata(reqState);
    mMetadata->fillTonemapCurve(*reqState);

    rk_aiq_exposure_sensor_descriptor desc;
    status_t status = getSensorModeData(desc);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to retrieve sensor mode data - BUG");
        return status;
    }
    int64_t rollingShutterSkew = ((int64_t)(desc.sensor_output_height - 1) *
                                 desc.pixel_periods_per_line /
                                 (desc.pixel_clock_freq_mhz * 1000000ULL)) *
                                 1000000000ULL;
    //# ANDROID_METADATA_Dynamic android.sensor.rollingShutterSkew done
    reqState->ctrlUnitResult->update(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
                                     &rollingShutterSkew, 1);

    uint8_t pipelineDepth;
    mSettingsProcessor->getStaticMetadataCache().getPipelineDepth(pipelineDepth);
    //# ANDROID_METADATA_Dynamic android.request.pipelineDepth done
    reqState->ctrlUnitResult->update(ANDROID_REQUEST_PIPELINE_DEPTH,
                                     &pipelineDepth, 1);
    // return 0.0f for the fixed-focus
     if (!mLensSupported) {
         float focusDistance = 0.0f;
         reqState->ctrlUnitResult->update(ANDROID_LENS_FOCUS_DISTANCE,
                                          &focusDistance, 1);
     }

    return OK;
}

status_t
ControlUnit::handleNewStat(MessageStats msg)
{
    status_t status = NO_ERROR;
    std::shared_ptr<RequestCtrlState> reqState = nullptr;
    AiqResults *latestResults = m3ARunner->getLatestResults();
    std::map<int, std::shared_ptr<RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.begin();
    if (it == mWaitingForCapture.end()) {
        LOGW("have no request, drop the stats");
        return OK;
    }

    reqState = it->second;
    if (CC_UNLIKELY(reqState.get() == nullptr || reqState->captureSettings.get() == nullptr)) {
        LOGE("No valid state or settings, Fix the bug!");
        return UNKNOWN_ERROR;
    }

    /**
     * Cache the generated metadata to mLatestAiqMetadata, since the
     * corresponding aiq results would not take effect in this request.
     */
    mLatestAiqMetadata.clear();
    CameraMetadata *ctrlUnitResult = reqState->ctrlUnitResult;
    reqState->ctrlUnitResult = &mLatestAiqMetadata;

    std::shared_ptr<rk_aiq_statistics_input_params> stats = msg.stats;
    unsigned long long statsId = UINT64_MAX;
    if (stats != nullptr) {
        statsId = stats->frame_id;
        prepareStats(*reqState, *stats.get());
    } else {
        //only allow the first request stats is NULL
        if (mLatestRequestId >= 0)
            LOGE("stats is NULL, but request id is valid, BUG!");
        status = m3aWrapper->setStatistics(NULL, &mSensorDescriptor);
        if (CC_UNLIKELY(status != OK)) {
            LOGW("Failed to set statistics for 3A iteration");
        }
    }
    /*TODO: zyc*/
    bool forceUpdated = (mLatestRequestId < 0 ? true : false);
    status = m3ARunner->run2A(*reqState, forceUpdated);
    if (status != NO_ERROR) {
       LOGE("Error in running run2AandCapture for frame id %llu", statsId);
       reqState->ctrlUnitResult = ctrlUnitResult;
       return status;
    }

    /*
     * Store the settings in the settings history if we expect stats to be in
     * use. This is only in case the control mode is different than
     * ANDROID_CONTROL_MODE_OFF_KEEP_STATE
     * WA - HAL runs out of capture settings in ANDROID_CONTROL_MODE_OFF,
     * so history is not udated for it. TODO fix later so that 3A runs in
     * background without actually applying the settings.
     */
    if (reqState->aaaControls.controlMode != ANDROID_CONTROL_MODE_OFF_KEEP_STATE &&
        reqState->aaaControls.controlMode != ANDROID_CONTROL_MODE_OFF) {
        if (mSettingsHistory.size() > 0) {
            std::vector<std::shared_ptr<CaptureUnitSettings>>::iterator it, last;
            last = mSettingsHistory.begin();
            for (it = mSettingsHistory.begin() + 1; it != mSettingsHistory.end(); ++it) {
                if ((*it)->inEffectFrom != UINT32_MAX &&
                    (*it)->inEffectFrom == (*last)->inEffectFrom) {
                    mSettingsHistory.erase(it);
                    it--;
                }
                last = it;
            }
        }
        mSettingsHistory.push_back(reqState->captureSettings);
        reqState->captureSettings->inEffectFrom = UINT32_MAX;
    }

    status = applyAeParams(reqState->captureSettings);
    if (status != NO_ERROR) {
        LOGE("Failed to apply AE settings for frame id %llu", statsId);
    }

    reqState->captureSettings->aiqResults.frame_id = statsId;
    *latestResults = reqState->captureSettings->aiqResults;
    /* dump3A(latestResults); */

    reqState->ctrlUnitResult = ctrlUnitResult;
    return status;
}

status_t
ControlUnit::handleRequestDone(MessageRequestDone msg)
{
    std::shared_ptr<RequestCtrlState> reqState = nullptr;
    int reqId = msg.requestId;

    std::map<int, std::shared_ptr<RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.find(reqId);
    if (it == mWaitingForCapture.end()) {
        LOGE("Unexpected request done event received for request %d - Fix the bug", reqId);
        return UNKNOWN_ERROR;
    }

    reqState = it->second;
    if (CC_UNLIKELY(reqState.get() == nullptr || reqState->captureSettings.get() == nullptr)) {
        LOGE("No valid state or settings for request Id = %d"
             "- Fix the bug!", reqId);
        return UNKNOWN_ERROR;
    }

    /*
     * Remove the request from Q once we have received all pixel data buffers
     * we expect from ISA. Query the graph config for that.
     *
     * Request which are processed from input buffers do not wait for pixel data
     */
    if (reqState->request->getNumberInputBufs() == 0)
        mWaitingForCapture.erase(reqId);

    return OK;
}

/**
 * completeProcessing
 *
 * Forward the pixel buffer to the Processing Unit to complete the processing.
 * If all the buffers from Capture Unit have arrived then:
 * - it updates the metadata
 * - it removes the request from the vector mWaitingForCapture.
 *
 * The metadata update is now transferred to the ProcessingUnit.
 * This is done only on arrival of the last pixel data buffer. ControlUnit still
 * keeps the state, so it is responsible for triggering the update.
 */
status_t
ControlUnit::completeProcessing(std::shared_ptr<RequestCtrlState> &reqState)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    int reqId = reqState->request->getId();

    if (CC_LIKELY((reqState->request != nullptr) &&
                  (reqState->captureSettings.get() != nullptr))) {
        LOG2("%s: completing buffer %d for request %d",
                __FUNCTION__,
                reqState->framesArrived,
                reqId);

        /* TODO: cleanup
         * This struct copy from state is only needed for JPEG creation.
         * Ideally we should directly write inside members of processingSettings
         * whatever settings are needed for Processing Unit.
         * This should be moved to any of the processXXXSettings.
         */
        reqState->processingSettings->android3Actrl = reqState->aaaControls;

        // Apply cached aiqResults and metadata
        reqState->captureSettings->aiqResults = *m3ARunner->getLatestResults();
        reqState->ctrlUnitResult->append(mLatestAiqMetadata);

        fillMetadata(reqState);

        mImguUnit->completeRequest(reqState->processingSettings, true);
    } else {
        LOGE("request or captureSetting is nullptr - Fix the bug!");
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ControlUnit::handleNewShutter(MessageShutter msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    std::shared_ptr<RequestCtrlState> reqState = nullptr;
    int reqId = msg.requestId;

    //check whether this reqId has been shutter done
    if (reqId <= mShutterDoneReqId)
        return OK;

    std::map<int, std::shared_ptr<RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.find(reqId);
    if (it == mWaitingForCapture.end()) {
        LOGE("Unexpected shutter event received for request %d - Fix the bug", reqId);
        return UNKNOWN_ERROR;
    }

    reqState = it->second;
    if (CC_UNLIKELY(reqState.get() == nullptr || reqState->captureSettings.get() == nullptr)) {
        LOGE("No valid state or settings for request Id = %d"
             "- Fix the bug!", reqId);
        return UNKNOWN_ERROR;
    }

    /* flash state - hack, should know from frame whether it fired */
    const CameraMetadata* metaData = reqState->request->getSettings();
    if (metaData == nullptr) {
        LOGE("Metadata should not be nullptr. Fix the bug!");
        return UNKNOWN_ERROR;
    }

    uint8_t flashState = ANDROID_FLASH_STATE_UNAVAILABLE;

    //# ANDROID_METADATA_Dynamic android.flash.state done
    reqState->ctrlUnitResult->update(ANDROID_FLASH_STATE, &flashState, 1);

    int64_t ts = msg.tv_sec * 1000000000; // seconds to nanoseconds
    ts += msg.tv_usec * 1000; // microseconds to nanoseconds

    //# ANDROID_METADATA_Dynamic android.sensor.timestamp done
    reqState->ctrlUnitResult->update(ANDROID_SENSOR_TIMESTAMP, &ts, 1);
    reqState->request->mCallback->shutterDone(reqState->request, ts);
    reqState->shutterDone = true;
    reqState->captureSettings->timestamp = ts;
    mShutterDoneReqId = reqId;

    reqState->ctrlUnitResult->update(ANDROID_SYNC_FRAME_NUMBER, &msg.sequence,
                                     1);

    return NO_ERROR;
}

status_t ControlUnit::handleNewSensorDescriptor(MessageSensorMode msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    mMetadata->FillSensorDescriptor(msg);
    mSensorDescriptor = msg.exposureDesc;

    return mSettingsProcessor->handleNewSensorDescriptor(msg);
}

status_t
ControlUnit::flush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    base::Callback<status_t()> closure =
            base::Bind(&ControlUnit::handleFlush, base::Unretained(this));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t ControlUnit::handleFlush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mSyncManager->flush();
    mWaitingForCapture.clear();
    mSettingsHistory.clear();

    return NO_ERROR;
}

status_t
ControlUnit::getIspRect(rk_aiq_exposure_sensor_descriptor &desc)
{
//copy the define in the GraphConfig.cpp
#define MEDIACTL_PAD_IN_NUM 0
#define MEDIACTL_PAD_OUTPUT_NUM 2
    status_t status = NO_ERROR;
    const MediaCtlConfig *mediaCtlConfig = mStreamCfgProv.getMediaCtlConfig(IStreamConfigProvider::IMGU_COMMON);
    for (unsigned int i = 0; i < mediaCtlConfig->mSelectionParams.size(); i++) {
        MediaCtlSelectionParams param = mediaCtlConfig->mSelectionParams[i];
        if (strstr(param.entityName.c_str(), "rkisp1-isp-subdev")) {
            switch (param.pad) {
            case MEDIACTL_PAD_IN_NUM:
                desc.isp_input_width = param.width;
                desc.isp_input_height = param.height;
                break;
            case MEDIACTL_PAD_OUTPUT_NUM:
                desc.isp_output_width = param.width;
                desc.isp_output_height = param.height;
                break;
            default :
                LOG2("%s:%d: wrong entity pad(%d)", __func__, __LINE__, param.pad);
                status = BAD_VALUE;
                break;
            }
        }
    }
    return status;
}

/**
 * getSensorModeData
 *
 * Retrieves the exposure sensor descriptor that the 3A algorithms need to run
 * This information is relayed to control unit.
 * The other piece of information related to sensor mode (frame params) is
 * provided by the Input System as part of the configuration results.
 *
 * \param [out] desc: Sensor descriptor to fill in
 * \return OK if everything went fine.
 * \return BAD_VALUE if any of the sensor driver queries failed.
 */
status_t
ControlUnit::getSensorModeData(rk_aiq_exposure_sensor_descriptor &desc)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    status = getIspRect(desc);
    status |= mSyncManager->getSensorModeData(desc);
    return status;
}

bool
ControlUnit::notifyCaptureEvent(CaptureMessage *captureMsg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (captureMsg == nullptr) {
        return false;
    }

    if (captureMsg->id == CAPTURE_MESSAGE_ID_ERROR) {
        // handle capture error
        return true;
    }

    switch (captureMsg->data.event.type) {
        case CAPTURE_EVENT_NEW_SENSOR_DESCRIPTOR:
        {
            MessageSensorMode msg;
            msg.exposureDesc = captureMsg->data.event.exposureDesc;
            msg.frameParams = captureMsg->data.event.frameParams;
            base::Callback<status_t()> closure =
                    base::Bind(&ControlUnit::handleNewSensorDescriptor,
                               base::Unretained(this),
                               base::Passed(std::move(msg)));
            mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
            break;
        }
        case CAPTURE_EVENT_2A_STATISTICS:
        {
            MessageStats msg;
            msg.stats = captureMsg->data.event.stats;
            base::Callback<status_t()> closure =
                    base::Bind(&ControlUnit::handleNewStat,
                               base::Unretained(this),
                               base::Passed(std::move(msg)));
            mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
            break;
        }
        case CAPTURE_EVENT_SHUTTER:
        {
            status_t status = OK;
            MessageShutter msg;
            msg.requestId = captureMsg->data.event.reqId;
            msg.tv_sec = captureMsg->data.event.timestamp.tv_sec;
            msg.tv_usec = captureMsg->data.event.timestamp.tv_usec;
            msg.sequence = captureMsg->data.event.sequence;
            base::Callback<status_t()> closure =
                    base::Bind(&ControlUnit::handleNewShutter,
                               base::Unretained(this),
                               base::Passed(std::move(msg)));
            mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
            break;
        }
        case CAPTURE_EVENT_NEW_SOF:
            mSofSequence = captureMsg->data.event.sequence;
            LOG2("sof event sequence = %u", mSofSequence);
            break;
        case CAPTURE_REQUEST_DONE:
        {
            MessageRequestDone msg;
            msg.requestId = captureMsg->data.event.reqId;
            base::Callback<status_t()> closure =
                    base::Bind(&ControlUnit::handleRequestDone,
                               base::Unretained(this),
                               base::Passed(std::move(msg)));
            mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
            break;
        }
        default:
            LOGW("Unsupported Capture event ");
            break;
    }

    return true;
}

/**
 * prepareStats
 *
 * Prepares the rk_aiq_statistics_input_params struct before running 3A and then
 * it calls Rockchip3Aplus::setStatistics() to pass them to the 3A algorithms.
 *
 * The main preparation consist in finding the capture unit settings that were
 * in effect when the statistics where captured.
 *
 * The AIQ results in effect when the statistics were gathered are available in
 * the control unit
 * TODO: Current flow is not handling these results well. We may have completed
 * the request and the results are lost. We need to keep the capture settings
 * alive longer than the request state struct. This was done in Sofia.
 *
 * Alternatively once we have EmDa we can also use that.
 *
 * \param reqState [IN/OUT]: request to process
 * \param s [OUT]: structure holding the captured stats
 */
void ControlUnit::prepareStats(RequestCtrlState &reqState,
                               rk_aiq_statistics_input_params &s)
{
    status_t status = NO_ERROR;
    std::shared_ptr<CaptureUnitSettings> settingsInEffect = nullptr;
    LOG2(" %s: statistics from request %lld used to process request %d",
        __FUNCTION__, s.frame_id,
        reqState.request->getId());

    // Prepare the input parameters for the statistics
    rk_aiq_statistics_input_params* params = &s;

    settingsInEffect = findSettingsInEffect(params->frame_id);
    if (settingsInEffect.get()) {
        params->ae_results = &settingsInEffect->aiqResults.aeResults;
        params->awb_results = &settingsInEffect->aiqResults.awbResults;
        params->misc_results = &settingsInEffect->aiqResults.miscIspResults;
    } else {
        LOG1("preparing statistics from exp %lld that we do not track", params->frame_id);

        // default to latest results
        AiqResults *latestResults = m3ARunner->getLatestResults();
        params->ae_results = &latestResults->aeResults;
        params->awb_results = &latestResults->awbResults;
        params->misc_results = &latestResults->miscIspResults;
    }

    status = m3aWrapper->setStatistics(params, &mSensorDescriptor);
    if (CC_UNLIKELY(status != OK)) {
        LOGW("Failed to set statistics for 3A iteration");
    }
    // algo's are ready to run
    reqState.aeState = ALGORITHM_READY;
    reqState.awbState = ALGORITHM_READY;
}

/**
 *
 * find the capture unit settings that were in effect for the frame with
 * exposure id (expId) was captured.
 *
 * Iterates through the vector settings history to find the settings marked as
 * in effect in an exposure id that is the same or bigger.
 *
 * It keeps the size of the settings history buffer limited.
 */
std::shared_ptr<CaptureUnitSettings> ControlUnit::findSettingsInEffect(uint64_t expId)
{
    std::shared_ptr<CaptureUnitSettings> settingsInEffect = nullptr;
    std::vector<std::shared_ptr<CaptureUnitSettings>>::iterator it;
    for (it = mSettingsHistory.begin(); it != mSettingsHistory.end(); ++it) {
        LOG2("%s:%d:mSettingsHistory.size(%d) ineffectFrom(%d), expId(%lld), exposure(%d), aec gain(%d)", __func__, __LINE__,mSettingsHistory.size(), (*it)->inEffectFrom, expId,
             (*it)->aiqResults.aeResults.sensor_exposure.coarse_integration_time,
             (*it)->aiqResults.aeResults.sensor_exposure.analog_gain_code_global);
        if ((*it)->inEffectFrom == expId) {
            // we found the exact settings
            settingsInEffect = *it;
            break;
        }
        if ((*it)->inEffectFrom > expId && it != mSettingsHistory.begin()) {
            // Pick the previous settings which have had effect already.
            // "it" is not the history begin so "it--" is safe.
            it--;
            settingsInEffect = *it;
            break;
        }
    }

    if (it == mSettingsHistory.end() && !mSettingsHistory.empty()) {
        LOG2("Could not find settings for expID %" PRIu64 " providing for %d", expId,
             mSettingsHistory.back()->inEffectFrom);
        settingsInEffect = mSettingsHistory.back();
    }

    // Keep the size of the history fixed
    if (mSettingsHistory.size() == MAX_SETTINGS_HISTORY_SIZE)
        mSettingsHistory.erase(mSettingsHistory.begin());
    return settingsInEffect;
}

} // namespace camera2
} // namespace android

