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

#define LOG_TAG "ControlUnit"

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "ControlUnit.h"
#include "IPU3CameraHw.h"
#include "CaptureUnit.h"
#include "ImguUnit.h"
#include "CameraStream.h"
#include "IPU3CapturedStatistics.h"
#include "IPU3CameraCapInfo.h"
#include "CameraMetadataHelper.h"
#include "PlatformData.h"
#include "ProcUnitSettings.h"
#include "LensHw.h"
#include "SettingsProcessor.h"
#include "Metadata.h"
#include "AAARunner.h"

namespace android {
namespace camera2 {

ControlUnit::ControlUnit(ImguUnit *thePU,
                         CaptureUnit *theCU,
                         int cameraId,
                         IStreamConfigProvider &aStreamCfgProv) :
        mRequestStatePool("CtrlReqState"),
        mCaptureUnitSettingsPool("CapUSettings"),
        mProcUnitSettingsPool("ProcUSettings"),
        mLatestStatistics(nullptr),
        mLatestRequestId(-1),
        mImguUnit(thePU),
        mCaptureUnit(theCU),
        m3aWrapper(nullptr),
        mCameraId(cameraId),
        mCameraThread("CtrlUThread"),
        mBaseIso(100),
        mStreamCfgProv(aStreamCfgProv),
        mSettingsProcessor(nullptr),
        mMetadata(nullptr),
        m3ARunner(nullptr),
        mLensController(nullptr)
{
}

status_t
ControlUnit::init()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    status_t status = OK;
    const char *sensorName = nullptr;
    ia_binary_data nvmData = {nullptr, 0};
    LensHw *lensController;

    if (!mCameraThread.Start()) {
        LOGE("Camera thread failed to start");
        return UNKNOWN_ERROR;
    }

    const IPU3CameraCapInfo *cap = getIPU3CameraCapInfo(mCameraId);
    if (!cap) {
        LOGE("Not enough information for getting NVM data");
    } else {
        sensorName = cap->getSensorName();
    }

    if (!cap || cap->sensorType() == SENSOR_TYPE_RAW) {
        m3aWrapper = new Intel3aPlus(mCameraId);
    } else {
        LOGE("SoC camera 3A control missing");
        return UNKNOWN_ERROR;
    }

    m3aWrapper->enableAiqdDataSave(true);
    if (cap)
        nvmData = cap->mNvmData;
    if (m3aWrapper->initAIQ(MAX_STATISTICS_WIDTH,
                            MAX_STATISTICS_HEIGHT,
                            nvmData,
                            sensorName) != NO_ERROR) {
        LOGE("Error initializing 3A control");
        return UNKNOWN_ERROR;
    }

    mSettingsProcessor = new SettingsProcessor(mCameraId, m3aWrapper, mStreamCfgProv);
    mSettingsProcessor->init();

    mMetadata = new Metadata(mCameraId, m3aWrapper);
    status = mMetadata->init();

    mCaptureUnit->setSettingsProcessor(mSettingsProcessor);

    /*
     * init the pools of Request State structs and CaptureUnit settings
     * and Processing Unit Settings
     */
    mRequestStatePool.init(MAX_REQUEST_IN_PROCESS_NUM, RequestCtrlState::reset);
    mCaptureUnitSettingsPool.init(SETTINGS_POOL_SIZE + 2);
    mProcUnitSettingsPool.init(SETTINGS_POOL_SIZE, ProcUnitSettings::reset);

    /*
     * Retrieve the Lens Controller interface from Capture Unit
     */
    lensController = mCaptureUnit->getLensControlInterface();

    mSettingsHistory.clear();

    /* Set ISO map support */
    bool supportIsoMap = false;
    if (cap)
        supportIsoMap = cap->getSupportIsoMap();
    m3aWrapper->setSupportIsoMap(supportIsoMap);

    m3ARunner = new AAARunner(mCameraId, m3aWrapper, mSettingsProcessor, lensController);

    /* Set digi gain support */
    bool supportDigiGain = false;
    if (cap)
        supportDigiGain = cap->digiGainOnSensor();
    status = m3ARunner->init(supportDigiGain);

    status = allocateLscResults();
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to allocate LSC results");
        return NO_MEMORY;
    }

    return status;
}


/**
 * allocateLscResults
 *
 * Allocates the size of the LSC tables used as part of the AIQ results
 * that the 3A algorithms produce.
 * This allocation is done dynamically since it depends on the sensor.
 *
 * Since cmc is read here, base_iso is stored in this function also.
 *
 * The memory will be freed by the AiqResult destructor.
 *
 */
status_t ControlUnit::allocateLscResults()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    status_t status = OK;
    std::shared_ptr<CaptureUnitSettings> capSettings = nullptr;
    ia_binary_data cpfData;
    ia_cmc_t *cmcData = nullptr;

    PlatformData::getCpfAndCmc(cpfData, &cmcData, nullptr, mCameraId);
    if (!cmcData) {
        LOGE("No CMC data available for sensor. fix the CPF file!");
        return UNKNOWN_ERROR;
    }

    if (cmcData && cmcData->cmc_sensitivity)
        mBaseIso = cmcData->cmc_sensitivity->base_iso;
    else
        LOGW("Cannot get base iso from cmc");

    if (!cmcData || !cmcData->cmc_parsed_lens_shading.cmc_lens_shading) {
        LOGW("Lens shading grid not set in cmc");
        return BAD_VALUE;
    }

    int tableH = cmcData->cmc_parsed_lens_shading.cmc_lens_shading->grid_height;
    int tableW = cmcData->cmc_parsed_lens_shading.cmc_lens_shading->grid_width;
    int tableSize = tableW * tableH;
    if (tableSize == 0) {
        LOGE("Invalid LSC table tize: fix the CPF file!");
        return BAD_VALUE;
    }

    int poolSize = mCaptureUnitSettingsPool.availableItems();
    for (int i = 0; i < poolSize; i++) {
        mCaptureUnitSettingsPool.acquireItem(capSettings);
        status = capSettings->aiqResults.allocateLsc(tableSize);
        if (CC_UNLIKELY(status != OK)) {
            break; // error reported outside
        }
        capSettings->aiqResults.init();
        m3ARunner->initLsc(capSettings->aiqResults, tableSize);
    }

    if (status == OK)
        status = m3ARunner->allocateLscTable(tableSize);

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
        me->captureBufs.rawBuffer.reset();
        me->captureBufs.rawNonScaledBuffer.reset();
        me->captureBufs.lastRawNonScaledBuffer.reset();
        DELETE_ARRAY_AND_NULLIFY(me->rGammaLut);
        DELETE_ARRAY_AND_NULLIFY(me->gGammaLut);
        DELETE_ARRAY_AND_NULLIFY(me->bGammaLut);
    } else {
        LOGE("Trying to reset a null CtrlState structure !! - BUG ");
    }
}

void RequestCtrlState::init(Camera3Request *req)
{
    request = req;
    aiqInputParams.init();
    if (CC_LIKELY(captureSettings)) {
        captureSettings->aiqResults.init();
        captureSettings->aiqResults.requestId = req->getId();
        captureSettings->afRegion.init(0);
        captureSettings->aeRegion.init(0);
        captureSettings->makernote.data = nullptr;
        captureSettings->makernote.size = 0;
    } else {
        LOGE(" Failed to init Ctrl State struct: no capture settings!! - BUG");
        return;
    }
    if (CC_LIKELY(processingSettings.get() != nullptr)) {
        processingSettings->captureSettings = captureSettings;
        processingSettings->request = req;
    } else {
        LOGE(" Failed to init Ctrl State: no processing settings!! - BUG");
        return;
    }
    afState = ALGORITHM_NOT_CONFIG;
    aeState = ALGORITHM_NOT_CONFIG;
    awbState = ALGORITHM_NOT_CONFIG;
    ctrlUnitResult = request->getPartialResultBuffer(CONTROL_UNIT_PARTIAL_RESULT);
    statsArrived = false;
    framesArrived = 0;
    shutterDone = false;
    blackLevelOff = false;
    intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
    aaaControls.ae.aeMode =  ANDROID_CONTROL_AE_MODE_ON;
    aaaControls.af.afMode =  ANDROID_CONTROL_AF_MODE_AUTO;
    aaaControls.af.afTrigger =  ANDROID_CONTROL_AF_TRIGGER_IDLE;
    aaaControls.awb.awbMode =  ANDROID_CONTROL_AWB_MODE_AUTO;
    aaaControls.controlMode =  ANDROID_CONTROL_MODE_AUTO;

    tonemapContrastCurve = false;
    rGammaLut = nullptr;
    gGammaLut = nullptr;
    bGammaLut = nullptr;
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
    int64_t id = captureSettings->aiqResults.requestId;
    ctrlUnitResult->update(ANDROID_SYNC_FRAME_NUMBER,
                          &id, 1);

    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count == 1) {
        intent = entry.data.u8[0];
    }

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
}

ControlUnit::~ControlUnit()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);

    mLatestStatistics = nullptr;
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
        mPendingRequests.clear();
        mWaitingForCapture.clear();
        mSettingsHistory.clear();
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
ControlUnit::processRequest(Camera3Request* request)
{
    status_t status = NO_ERROR;
    std::shared_ptr<RequestCtrlState> state;
    LOG2("@%s: id %d", __FUNCTION__, request->getId());

    status = acquireRequestStateStruct(state);
    if (CC_UNLIKELY(status != OK) || CC_UNLIKELY(state.get() == nullptr)) {
        return status;  // error log already done in the helper method
    }

    state->init(request);

    base::Callback<status_t()> closure =
            base::Bind(&ControlUnit::handleNewRequest, base::Unretained(this),
                       base::Passed(std::move(state)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return OK;
}

status_t ControlUnit::handleNewRequest(std::shared_ptr<RequestCtrlState> state)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);

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

    mPendingRequests.push_back(reqState);
    reqState = mPendingRequests[0];

    /**
     * PHASE2: Process for capture or Q or reprocessing
     * Use dummy stats if no stats is received.
     *
     * Use the latest valid stats for still capture,
     * it comes from video pipe (during still preview)
     */
    if (mLatestRequestId >= AWB_CONVERGENCE_WAIT_COUNT ||
        mLatestRequestId == -1 ||
        reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB) > 0 ||
        (mLatestStatistics != nullptr &&
         mLatestRequestId == mLatestStatistics->id)) {
      mPendingRequests.erase(mPendingRequests.begin());

      status = processRequestForCapture(reqState, mLatestStatistics);
      if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to process req %d for capture [%d]",
             reqState->request->getId(), status);
        // TODO: handle error !
      }
    }

    return status;
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
ControlUnit::processRequestForCapture(std::shared_ptr<RequestCtrlState> &reqState,
                                      std::shared_ptr<IPU3CapturedStatistics> &stats)
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

    if (stats != nullptr) {
        prepareStats(*reqState, *stats.get());
        LOG2("%s, stats frame sequence %u", __FUNCTION__, stats->frameSequence);
    }

    bool forceUpdated = (mLatestRequestId < 0 ? true : false);
    status = m3ARunner->run2A(*reqState, forceUpdated);
    if (status != NO_ERROR) {
       LOGE("Error in running run2AandCapture for request %d", reqId);
       return status;
    }

    mMetadata->writeLSCMetadata(reqState);

    m3ARunner->runAf(*reqState);

    // Latest results are saved for the next frame calculation if we do not
    // find the correct results.
    // TODO: Remove this once we fix the request flow so we can use the results
    // from the request at prepareStats
    m3aWrapper->deepCopyAiqResults(m3ARunner->getLatestResults(),
                                   reqState->captureSettings->aiqResults,
                                   true);
    m3ARunner->updateInputParams(reqState->aiqInputParams);

    status = mCaptureUnit->doCapture(reqState->request,
                                     reqState->captureSettings);
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGE("Failed to issue capture request for id %d", reqId);
    }
    /**
     * Move the request to the vector mWaitingForCapture
     */
    mWaitingForCapture.insert(std::make_pair(reqId, reqState));
    mLatestRequestId = reqId;

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
        // Keep the size of the history fixed
        if (mSettingsHistory.size() >= MAX_SETTINGS_HISTORY_SIZE) {
            LOGP("%s delete one hold for %p in mCaptureUnitSettingsPool",
                 __FUNCTION__, mSettingsHistory.begin()->get());
            mSettingsHistory.erase(mSettingsHistory.begin());
        }

        mSettingsHistory.push_back(reqState->captureSettings);
        LOGP("%s add one hold for %p in mCaptureUnitSettingsPool",
             __FUNCTION__, reqState->captureSettings.get());
    }

    int jpegBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);
    int implDefinedBufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    int yuv888BufCount = reqState->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_YCbCr_420_888);
    LOG2("@%s jpegs:%d impl defined:%d yuv888:%d inputbufs:%d req id %d",
         __FUNCTION__,
         jpegBufCount,
         implDefinedBufCount,
         yuv888BufCount,
         (reqState->request->hasInputBuf() ? 1 : 0),
         reqState->request->getId());
    if (jpegBufCount > 0) {
        // NOTE: Makernote should be get after isp_bxt_run()
        // NOTE: makernote.data deleted in JpegEncodeTask::handleMakernote()
        const unsigned mknSize = MAKERNOTE_SECTION1_SIZE + MAKERNOTE_SECTION2_SIZE;
        MakernoteData mkn = {nullptr, mknSize};
        mkn.data = new char[mknSize];
        m3aWrapper->getMakerNote(ia_mkn_trg_section_2, mkn);

        reqState->captureSettings->makernote = mkn;

    } else {
        // No JPEG buffers in request. Reset MKN info, just in case.
        reqState->captureSettings->makernote.data = nullptr;
        reqState->captureSettings->makernote.size = 0;
    }

    return status;
}

status_t ControlUnit::handleNewImage(MessageNewImage msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    int reqId = msg.requestId;

    std::map<int, std::shared_ptr<RequestCtrlState>>::iterator it =
                                    mWaitingForCapture.find(reqId);
    CheckError(it == mWaitingForCapture.end(), UNKNOWN_ERROR,
               "@%s, Unexpected new image received %d", __FUNCTION__, reqId);

    std::shared_ptr<RequestCtrlState> reqState = it->second;
    CheckError(reqState == nullptr, UNKNOWN_ERROR,
               "@%s, State for request Id = %d is nullptr", __FUNCTION__, reqId);

    /*
     * Send the buffer. See complete processing to understand
     * how we do the hold up.
     */
    reqState->captureBufs.rawBuffer = nullptr;
    reqState->captureBufs.rawNonScaledBuffer = nullptr;
    reqState->captureBufs.lastRawNonScaledBuffer = nullptr;

    reqState->framesArrived++;

    CheckError(msg.type != CAPTURE_EVENT_RAW_BAYER, UNKNOWN_ERROR,
               "@%s, Unknown capture buffer type in request %d", __FUNCTION__, reqId);

    reqState->captureBufs.rawNonScaledBuffer = msg.rawBuffer;
    reqState->captureBufs.lastRawNonScaledBuffer = msg.lastRawBuffer;

    status_t status = completeProcessing(reqState);
    CheckError(status != OK, status, "@%s, Cannot complete the buffer processing", __FUNCTION__);

    return OK;
}

status_t ControlUnit::handleNewStat(MessageStats msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    status_t status = NO_ERROR;
    std::shared_ptr<IPU3CapturedStatistics> stats = msg.stats;
    int statsId = 0;
    if (stats.get() != nullptr) {
        statsId = stats->id;

        // Still pipe has no stats output and data is invalid
        // so here only valid data are saved.
        if (stats->rgbsGridArray[0] && stats->rgbsGridArray[0]->grid_width)
            mLatestStatistics = stats;
    }

    if (mPendingRequests.empty()) {
        return status;
    }

    bool cio2Starving = false;
    if (statsId == mLatestRequestId && mLatestRequestId >= AWB_CONVERGENCE_WAIT_COUNT) {
        // The 0 ~ AWB_CONVERGENCE_WAIT_COUNT request will run with new statistics to speed
        // AWB converging process. Otherwise, queue all pending requests to CIO2.
        cio2Starving = true;
    }

    // Process request
    do {
        std::shared_ptr<RequestCtrlState> reqState = mPendingRequests[0];
        mPendingRequests.erase(mPendingRequests.begin());

        if (CC_UNLIKELY(!reqState || reqState->request == nullptr)) {
            LOGE("reqState is nullptr, find BUG!");
            return UNKNOWN_ERROR;
        }

        LOG2("@%s: process reqState %d, with stat id of req %d", __FUNCTION__,
                reqState->request->getId(), statsId);

        status = processRequestForCapture(reqState, mLatestStatistics);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Failed to process request %d for capture ", reqState->request->getId());
            // TODO: handle error !
        }
    } while (cio2Starving && !mPendingRequests.empty());

    return status;
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
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    int reqId = reqState->captureSettings->aiqResults.requestId;
    bool updateMeta = false;

    LOG2("complete processing req %d frames arrived %d",
             reqId, reqState->framesArrived);

    // We do this only once per request when the first buffer arrives
    if (reqState->framesArrived == 1) {
        mMetadata->writeAwbMetadata(*reqState);
        mMetadata->writeSensorMetadata(*reqState);
        mMetadata->writePAMetadata(*reqState);
        mMetadata->writeJpegMetadata(*reqState);
        mMetadata->writeMiscMetadata(*reqState);
        mMetadata->writeLensMetadata(*reqState);
        mMetadata->fillTonemapCurve(*reqState);

        int64_t rollingShutterSkew = mCaptureUnit->getRollingShutterSkew();
        //# ANDROID_METADATA_Dynamic android.sensor.rollingShutterSkew done
        reqState->ctrlUnitResult->update(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
                                         &rollingShutterSkew, 1);

        uint8_t pipelineDepth = mCaptureUnit->getPipelineDepth();
        //# ANDROID_METADATA_Dynamic android.request.pipelineDepth done
        reqState->ctrlUnitResult->update(ANDROID_REQUEST_PIPELINE_DEPTH,
                                         &pipelineDepth, 1);
    }

    updateMeta = true;

    mWaitingForCapture.erase(reqId);

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

        mImguUnit->completeRequest(reqState->processingSettings,
                                         reqState->captureBufs,
                                         updateMeta);
    } else {
        LOGE("request or captureSetting is nullptr - Fix the bug!");
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ControlUnit::handleNewShutter(MessageShutter msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    std::shared_ptr<RequestCtrlState> reqState = nullptr;
    int reqId = msg.requestId;

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
    //use request setting timestamp when reprocess
    if (reqState->request->hasInputBuf()){
        camera_metadata_ro_entry entry;
        entry = metaData->find(ANDROID_SENSOR_TIMESTAMP);
        if (entry.count == 1) {
            ts = entry.data.i64[0];
        }
    }
    //# ANDROID_METADATA_Dynamic android.sensor.timestamp done
    reqState->ctrlUnitResult->update(ANDROID_SENSOR_TIMESTAMP, &ts, 1);
    reqState->request->mCallback->shutterDone(reqState->request, ts);
    reqState->shutterDone = true;
    reqState->captureSettings->timestamp = ts;

    return NO_ERROR;
}

status_t ControlUnit::handleNewSensorDescriptor(MessageSensorMode msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    mMetadata->FillSensorDescriptor(msg);
    return mSettingsProcessor->handleNewSensorDescriptor(msg);
}

status_t
ControlUnit::flush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    status_t status = NO_ERROR;
    base::Callback<status_t()> closure =
            base::Bind(&ControlUnit::handleFlush, base::Unretained(this));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t ControlUnit::handleFlush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);

    mWaitingForCapture.clear();
    mPendingRequests.clear();
    mSettingsHistory.clear();

    return NO_ERROR;
}

bool
ControlUnit::notifyCaptureEvent(CaptureMessage *captureMsg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);

    if (captureMsg == nullptr) {
        return false;
    }

    if (captureMsg->id == CAPTURE_MESSAGE_ID_ERROR) {
        // handle capture error
        return true;
    }

    switch (captureMsg->data.event.type) {
        case CAPTURE_EVENT_RAW_BAYER:
        {
            MessageNewImage msg;
            msg.type = CAPTURE_EVENT_RAW_BAYER;
            msg.requestId = captureMsg->data.event.reqId;
            msg.rawBuffer = captureMsg->data.event.pixelBuffer;
            msg.lastRawBuffer = captureMsg->data.event.lastPixelBuffer;
            base::Callback<status_t()> closure =
                    base::Bind(&ControlUnit::handleNewImage,
                               base::Unretained(this),
                               base::Passed(std::move(msg)));
            mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
            break;
        }
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
            if (CC_UNLIKELY(captureMsg->data.event.stats.get() == nullptr)) {
                LOGE("captureMsg->stats == nullptr");
                return false;
            } else {
                MessageStats msg;
                msg.stats = captureMsg->data.event.stats;
                base::Callback<status_t()> closure =
                        base::Bind(&ControlUnit::handleNewStat,
                                   base::Unretained(this),
                                   base::Passed(std::move(msg)));
                mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
            }
            break;
        case CAPTURE_EVENT_SHUTTER:
        {
            MessageShutter msg;
            msg.requestId = captureMsg->data.event.reqId;
            msg.tv_sec = captureMsg->data.event.timestamp.tv_sec;
            msg.tv_usec = captureMsg->data.event.timestamp.tv_usec;
            base::Callback<status_t()> closure =
                    base::Bind(&ControlUnit::handleNewShutter,
                               base::Unretained(this),
                               base::Passed(std::move(msg)));
            mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
            break;
        }
        case CAPTURE_EVENT_NEW_SOF:
        {
            std::lock_guard<std::mutex> l(mSofDataLock);
            if (mSofDataMap.size() >= MAX_SETTINGS_HISTORY_SIZE) {
                mSofDataMap.erase(mSofDataMap.begin());
            }
            mSofDataMap[captureMsg->data.event.sequence] =
                      captureMsg->data.event.timestamp.tv_sec * 1000000
                      + captureMsg->data.event.timestamp.tv_usec;
            LOG2("sof event sequence = %u", captureMsg->data.event.sequence);
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
 * Prepares the ia_aiq_statistics_input_params struct before running 3A and then
 * it calls Intel3Aplus::setStatistics() to pass them to the 3A algorithms.
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
                               IPU3CapturedStatistics &s)
{
    status_t status = NO_ERROR;
    std::shared_ptr<CaptureUnitSettings> settingsInEffect = nullptr;
    LOG2(" %s: statistics from request %d used to process request %d",
        __FUNCTION__, s.id,
        reqState.request->getId());

    // Prepare the input parameters for the statistics
    ia_aiq_statistics_input_params* params = &s.aiqStatsInputParams;
    params->camera_orientation = ia_aiq_camera_orientation_unknown;

    params->external_histograms = nullptr;
    params->num_external_histograms = 0;

    settingsInEffect = findSettingsInEffect(params->frame_id);
    if (settingsInEffect.get()) {
        params->frame_ae_parameters = &settingsInEffect->aiqResults.aeResults;
        params->frame_af_parameters = &settingsInEffect->aiqResults.afResults;
        params->awb_results = &settingsInEffect->aiqResults.awbResults;
        params->frame_sa_parameters = &settingsInEffect->aiqResults.saResults;
        params->frame_pa_parameters = &settingsInEffect->aiqResults.paResults;
    } else {
        LOG1("preparing statistics from exp %lld that we do not track", params->frame_id);

        // default to latest results
        AiqResults& latestResults = m3ARunner->getLatestResults();
        params->frame_ae_parameters = &latestResults.aeResults;
        params->frame_af_parameters = &latestResults.afResults;
        params->awb_results = &latestResults.awbResults;
        params->frame_sa_parameters = &latestResults.saResults;
        params->frame_pa_parameters = &latestResults.paResults;
    }

    unsigned long long sofTimestamp = 0;
    {
    std::lock_guard<std::mutex> l(mSofDataLock);
    if (mSofDataMap.find(params->frame_id) != mSofDataMap.end()) {
        sofTimestamp = mSofDataMap.at(params->frame_id);
    } else {
        LOG2("frame timestamp %llu us, rolling shutter time %ld us", params->frame_timestamp,
              mCaptureUnit->getRollingShutterSkew() / 1000);
        sofTimestamp = params->frame_timestamp - mCaptureUnit->getRollingShutterSkew() / 1000;
    }
    }
    params->frame_timestamp = sofTimestamp
              - params->frame_ae_parameters->exposures[0].exposure->exposure_time_us;
    LOG2("frame expo start timestamp %lld, sequence %llu", params->frame_timestamp,
              params->frame_id);
    /**
     * Pass stats to all 3A algorithms
     * Since at the moment we do not have separate events for AF and AA stats
     * there is no need to pass the stats per algorithm.
     * AF usually runs first, but not always. For that reason we pass the stats
     * to the AIQ algorithms here.
     */
    status = m3aWrapper->setStatistics(params);
    if (CC_UNLIKELY(status != OK)) {
        LOGW("Failed to set statistics for 3A iteration");
    }

    // algo's are ready to run
    reqState.afState = ALGORITHM_READY;
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
    std::deque<std::shared_ptr<CaptureUnitSettings>>::iterator it;
    for (it = mSettingsHistory.begin(); it != mSettingsHistory.end(); ++it) {
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
             mSettingsHistory[0]->inEffectFrom);
        settingsInEffect = mSettingsHistory[0];
    }

    return settingsInEffect;
}

} // namespace camera2
} // namespace android

