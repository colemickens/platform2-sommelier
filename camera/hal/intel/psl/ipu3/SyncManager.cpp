/*
 * Copyright (C) 2017 Intel Corporation
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

#define LOG_TAG "SyncManager"

#include "SyncManager.h"
#include "MediaController.h"
#include "MediaEntity.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "Camera3GFXFormat.h"
#include "CaptureUnit.h"

namespace android {
namespace camera2 {


SyncManager::SyncManager(int32_t cameraId,
                         std::shared_ptr<MediaController> mediaCtl,
                         ISofListener *sofListener,
                         ISettingsSyncListener *listener):
    mCameraId(cameraId),
    mMediaCtl(mediaCtl),
    mSofListener(sofListener),
    mSettingsSyncListener(listener),
    mSensorType(SENSOR_TYPE_NONE),
    mSensorOp(nullptr),
    mFrameSyncSource(FRAME_SYNC_NA),
    mMessageQueue("Camera_SyncManager", (int)MESSAGE_ID_MAX),
    mThreadRunning(false),
    mStarted(false),
    mExposureDelay(0),
    mGainDelay(0),
    mDigiGainOnSensor(false),
    mCurrentSettingIdentifier(0),
    mMessageThread(new MessageThread(this, "SyncManager", PRIORITY_CAMERA))
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mCapInfo = getIPU3CameraCapInfo(cameraId);
    mMessageThread->run();
}

SyncManager::~SyncManager()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status;

    mMessageQueue.remove(MESSAGE_ID_SOF);
    mMessageQueue.remove(MESSAGE_ID_EOF);
    mMessageQueue.remove(MESSAGE_ID_SET_PARAMS);

    status = stop();
    if (status != NO_ERROR) {
        LOGE("Error stopping sync manager during destructor");
    }

    status = requestExitAndWait();
    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }

    if (mFrameSyncSource != FRAME_SYNC_NA) {
        // both EOF and SOF are from the ISYS receiver
        if (mIsysReceiverSubdev != nullptr)
            mIsysReceiverSubdev->unsubscribeEvent(mFrameSyncSource);

        mFrameSyncSource = FRAME_SYNC_NA;
    }

    if (mSensorOp != nullptr)
        mSensorOp = nullptr;

    mQueuedSettings.clear();
}

/**
 * based on the type of the mediacontroller entity, the correct subdev
 * will be chosen. Here, are treated 4 entities.
 * sensor side: pixel array
 * ISYS side: isys receiver
 *
 * \param[IN] entity: pointer to media entity struct
 * \param[IN] type: type of the subdev
 *
 * \return OK on success, BAD_VALUE on error
 */
status_t SyncManager::setSubdev(std::shared_ptr<MediaEntity> entity, sensorEntityType type)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    std::shared_ptr<V4L2Subdevice> subdev = nullptr;
    entity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);

    switch (type) {
    case SUBDEV_PIXEL_ARRAY:
        if (entity->getType() != SUBDEV_SENSOR) {
            LOGE("%s is not sensor subdevice", entity->getName());
            return BAD_VALUE;
        }
        mPixelArraySubdev = subdev;
        break;
    case SUBDEV_ISYSRECEIVER:
        if (entity->getType() != SUBDEV_GENERIC) {
            LOGE("%s is not Isys receiver subdevice\n", entity->getName());
        }
        mIsysReceiverSubdev = subdev;
        break;
    default:
        LOGE("Entity type (%d) not existing", type);
        return BAD_VALUE;
    }
    return OK;
}

/**
 * wrapper to retrieve media entity and set the appropriate subdev
 *
 * \param[IN] name given name of the entity
 * \param[IN] type type of the subdev
 *
 * \return OK on success, BAD_VALUE on error
 */
status_t SyncManager::setMediaEntity(const std::string &name,
                                     const sensorEntityType type)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;
    std::shared_ptr<MediaEntity> mediaEntity = nullptr;
    std::string entityName;
    const IPU3CameraCapInfo *cap = getIPU3CameraCapInfo(mCameraId);

    if (cap == nullptr) {
        LOGE("Failed to get cameraCapInfo");
        return UNKNOWN_ERROR;
    }

    //The entity port of ipu3-csi2 is dynamical,
    //get ipu3-csi2:0/1 from pixel entity sink.
    if (type == SUBDEV_ISYSRECEIVER) {
        const char* pixelType  = "pixel_array";

        entityName = cap->getMediaCtlEntityName(pixelType);
        status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
        if (mediaEntity == nullptr || status != NO_ERROR) {
             LOGE("Could not retrieve media entity %s", entityName.c_str());
             return UNKNOWN_ERROR;
        }
        std::vector<string> names;
        names.clear();
        status = mMediaCtl->getSinkNamesForEntity(mediaEntity, names);
        if (names.size()== 0 || status != NO_ERROR) {
             LOGE("Could not retrieve sink name of media entity %s",
                 entityName.c_str());
             return UNKNOWN_ERROR;
        }

        LOG1("camera %d using csi port: %s\n", mCameraId, names[0].c_str());
        entityName = names[0];
    } else {
        entityName = cap->getMediaCtlEntityName(name);
    }

    LOG1("found entityName: %s\n", entityName.c_str());
    if (entityName == "none" && name == "pixel_array") {
        LOGE("No %s in this Sensor. Should not happen", entityName.c_str());
        status = UNKNOWN_ERROR;
        return status;
    } else if (entityName == "none") {
        LOG1("No %s in this Sensor. Should not happen", entityName.c_str());
    } else {
        status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
        if (mediaEntity == nullptr || status != NO_ERROR) {
            LOGE("Could not retrieve media entity %s", entityName.c_str());
            return UNKNOWN_ERROR;
        }
        status = setSubdev(mediaEntity, type);
        if (status != NO_ERROR) {
            LOGE("Cannot set %s subdev", entityName.c_str());
            return status;
        }
    }

    return status;
}

/**
 * Enqueue init message to message queue.
 *
 * \param[IN] exposureDelay exposure delay
 * \param[IN] gainDelay gain delay
 *
 * \return status of synchronous messaging.
*/
status_t SyncManager::init(int32_t exposureDelay, int32_t gainDelay)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;
    msg.id = MESSAGE_ID_INIT;
    /**
     * The delay we want to store is the relative between exposure and gain
     * Usually exposure time delay is bigger. The model currently
     * does not support the other case. We have not seen any sensor that
     * does this.
     */
    if (exposureDelay >= gainDelay) {
        msg.data.init.gainDelay = exposureDelay - gainDelay;
    } else {
        LOGE("analog Gain delay bigger than exposure... not supported - BUG");
        msg.data.init.gainDelay = 0;
    }
    msg.data.init.exposureDelay = exposureDelay;

    return mMessageQueue.send(&msg, MESSAGE_ID_INIT);
}

/**
 * function to init the SyncManager.
 * set subdev.
 * create sensor object, pollerthread object.
 * register sensor events.
 *
 * \return status of synchronous messaging.
 */
status_t SyncManager::handleMessageInit(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;
    mExposureDelay = msg.data.init.exposureDelay;
    mGainDelay = msg.data.init.gainDelay;
    mDigiGainOnSensor = mCapInfo->digiGainOnSensor();
    mSensorType = (SensorType)mCapInfo->sensorType();

    // set pixel array
    status = setMediaEntity("pixel_array", SUBDEV_PIXEL_ARRAY);
    if (status != NO_ERROR) {
        LOGE("Cannot set pixel array");
        goto exit;
    }
    status = createSensorObj();
    if (status != NO_ERROR) {
        LOGE("Failed to create sensor object");
        goto exit;
    }

    mQueuedSettings.clear();
    mDelayedAGains.clear();
    mDelayedDGains.clear();

exit:
    mMessageQueue.reply(MESSAGE_ID_INIT, status);
    return status;
}

/**
 * method to check what type of driver the kernel
 * is using and create sensor object based on it.
 * at the moment, either SMIA or CRL.
 *
 * \return NO_ERROR if success.
 */
status_t SyncManager::createSensorObj()
{
    int fll, llp;
    status_t status = OK;

    if (mPixelArraySubdev == nullptr) {
        LOGE("Pixel array sub device not set");
        return UNKNOWN_ERROR;
    }

    mSensorOp = std::make_shared<SensorHwOp>(mPixelArraySubdev);

    return status;
}

/**
 * get sensor mode data after init.
 *
 * \param[OUT] desc sensor descriptor data
 *
 * \return status of sync message.
 */
status_t SyncManager::getSensorModeData(ia_aiq_exposure_sensor_descriptor &desc)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;

    msg.id = MESSAGE_ID_GET_SENSOR_MODEDATA;
    msg.data.sensorModeData.desc = &desc;
    return mMessageQueue.send(&msg, MESSAGE_ID_GET_SENSOR_MODEDATA);
}

/**
 * retrieve mode data (descriptor) from sensor
 *
 * \param[in] msg holding pointer to descriptor pointer
 *
 * \return OK, if sensor mode data retrieval was successful with good values
 * \return UNKNOWN_ERROR if pixel clock value was bad
 * \return Other non-OK value depending on error case
 */
status_t SyncManager::handleMessageGetSensorModeData(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;
    int integration_step = 0;
    int integration_max = 0;
    int pixel = 0;
    unsigned int vBlank = 0;
    ia_aiq_exposure_sensor_descriptor *desc;

    desc = msg.data.sensorModeData.desc;

    status = mSensorOp->getPixelRate(pixel);
    if (status != NO_ERROR) {
        LOGE("Failed to get pixel clock");
        mMessageQueue.reply(MESSAGE_ID_GET_SENSOR_MODEDATA, status);
        return status;
    } else if (pixel == 0) {
        LOGE("Bad pixel clock value: %d", pixel);
        return UNKNOWN_ERROR;
    }

    desc->pixel_clock_freq_mhz = (float)pixel / 1000000;
    unsigned int pixel_periods_per_line = 0, line_periods_per_field = 0;

    status = mSensorOp->updateMembers();
    if (status != NO_ERROR) {
        LOGE("Failed to update members");
        mMessageQueue.reply(MESSAGE_ID_GET_SENSOR_MODEDATA, status);
        return status;
    }

    status = mSensorOp->getMinimumFrameDuration(pixel_periods_per_line,
                                         line_periods_per_field);
    if (status != NO_ERROR) {
        LOGE("Failed to get frame Durations");
        mMessageQueue.reply(MESSAGE_ID_GET_SENSOR_MODEDATA, status);
        return status;
    }
    desc->pixel_periods_per_line = pixel_periods_per_line > USHRT_MAX ?
                                  USHRT_MAX: pixel_periods_per_line;
    desc->line_periods_per_field = line_periods_per_field > USHRT_MAX ?
                                  USHRT_MAX: line_periods_per_field;

    int coarse_int_time_min = -1;
    status = mSensorOp->getExposureRange(coarse_int_time_min,
                                         integration_max, integration_step);
    if (status != NO_ERROR) {
        LOGE("Failed to get Exposure Range");
        mMessageQueue.reply(MESSAGE_ID_GET_SENSOR_MODEDATA, status);
        return status;
    }

    desc->coarse_integration_time_min = CLIP(coarse_int_time_min, SHRT_MAX, 0);

    LOG2("%s: Exposure range coarse :min = %d, max = %d, step = %d",
                                    __FUNCTION__,
                                    desc->coarse_integration_time_min,
                                    integration_max, integration_step);

    desc->coarse_integration_time_max_margin = mCapInfo->getCITMaxMargin();

    //INFO: fine integration is not supported by v4l2
    desc->fine_integration_time_min = 0;
    desc->fine_integration_time_max_margin = desc->pixel_periods_per_line;

    status = mSensorOp->getVBlank(vBlank);
    desc->line_periods_vertical_blanking = vBlank > USHRT_MAX ? USHRT_MAX: vBlank;

    LOG2("%s: pixel clock = %f  ppl = %d, lpf =%d, int_min = %d, int_max_range %d",
                                    __FUNCTION__,
                                    desc->pixel_clock_freq_mhz,
                                    desc->pixel_periods_per_line,
                                    desc->line_periods_per_field,
                                    desc->coarse_integration_time_min,
                                    desc->coarse_integration_time_max_margin);

    mMessageQueue.reply(MESSAGE_ID_GET_SENSOR_MODEDATA, status);
    return status;
}

/**
 * enqueue to message queue parameter setting
 *
 * \param[in] settings Capture settings
 *
 * \return status for enqueueing message
 */
status_t SyncManager::setParameters(std::shared_ptr<CaptureUnitSettings> &settings)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    Message msg;
    msg.id = MESSAGE_ID_SET_PARAMS;
    msg.settings = settings;

    return mMessageQueue.send(&msg);
}

/**
 *
 * Queue new settings that will be consumed on next SOF
 *
 * \param[in] msg holding settings
 *
 * \return OK
 */
status_t SyncManager::handleMessageSetParams(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mQueuedSettings.push_back(msg.settings);

    return OK;
}

/**
 *
 * Set the sensor frame timing calculation width and height
 *
 * \param[in] width sensor frame timing calculation width
 * \param[in] height sensor frame timing calculation height
 *
 * \return OK
 */
status_t SyncManager::setSensorFT(int width, int height)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    Message msg;
    msg.id = MESSAGE_ID_SET_SENSOR_FT;
    msg.data.sFT.width = width;
    msg.data.sFT.height = height;

    return mMessageQueue.send(&msg, MESSAGE_ID_SET_SENSOR_FT);

}

status_t SyncManager::handleMessageSetSensorFT(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    int width = msg.data.sFT.width;
    int height = msg.data.sFT.height;

    if (mSensorOp.get() == nullptr) {
        LOGE("SensorHwOp class not initialized");
        return UNKNOWN_ERROR;
    }

    status = mSensorOp->setSensorFT(width, height);
    if (status != NO_ERROR) {
        LOGE("Failed to set sensor config");
        return UNKNOWN_ERROR;
    }

    mMessageQueue.reply(MESSAGE_ID_SET_SENSOR_FT, status);
    return status;
}

/**
 * Process work to do on SOF event detection
 *
 * \param[in] msg holding SOF event data.
 *
 *
 * \return status for work done
 */
status_t SyncManager::handleMessageSOF(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    int mode = TEST_PATTERN_MODE_OFF;

    if (!mStarted) {
        LOGD("SOF[%d] received while closing- ignoring",
                msg.data.frameEvent.exp_id);
        return OK;
    }
    // Poll again
    mPollerThread->pollRequest(0, 1000,
                              (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mDevicesToPoll);

    if (CC_UNLIKELY(mQueuedSettings.empty())) {
        LOG2("SOF[%d] Arrived and sensor does not have settings queued",
               msg.data.frameEvent.exp_id);
        // TODO: this will become an error once we fix capture unit to run at
        // sensor rate.
        // delete all gain from old previous client request.
        mDelayedAGains.clear();
        mDelayedDGains.clear();
    } else {
        mCurrentSettingIdentifier = mQueuedSettings[0]->settingsIdentifier;
        ia_aiq_exposure_sensor_parameters &expParams =
          *mQueuedSettings[0]->aiqResults.aeResults.exposures[0].sensor_exposure;

        LOG2("Applying settings @exp_id %d in Effect @ %d",
                    msg.data.frameEvent.exp_id,
                    msg.data.frameEvent.exp_id + mExposureDelay);

        status = applySensorParams(expParams);
        if (status != NO_ERROR)
            LOGE("Failed to apply sensor parameters.");

        switch (mQueuedSettings[0]->testPatternMode) {
            case ANDROID_SENSOR_TEST_PATTERN_MODE_OFF:
                mode = TEST_PATTERN_MODE_OFF;
                break;
            case ANDROID_SENSOR_TEST_PATTERN_MODE_COLOR_BARS:
                mode = TEST_PATTERN_MODE_COLOR_BARS;
                break;
            default:
                LOGW("test pattern mode[%d] not supported", mQueuedSettings[0]->testPatternMode);
                break;
        }

        status = mSensorOp->setTestPattern(mode);
        CheckError((status != NO_ERROR), status, "@%s, Fail to set test pattern mode = %d [%d]!",
                    __FUNCTION__, mQueuedSettings[0]->testPatternMode, status);

        /**
         * Mark the exposure id where this settings should be in effect.
         * this is used by the control unit to find the correct settings
         * for stats.
         * Then remove it from the Q.
         */
        // TODO: use frame idx for algo after stats rate meet out request
        mQueuedSettings[0]->inEffectFrom = mQueuedSettings[0]->aiqResults.requestId;
        mQueuedSettings.erase(mQueuedSettings.begin());
    }

    return status;
}

/**
 * Process work to do on EOF event detection
 *
 * \param[in] msg holding EOF event data.
 *
 *
 * \return status for work done
 */
status_t SyncManager::handleMessageEOF(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    UNUSED(msg);
    /**
     * We are not getting EOF yet, but once we do ...
     * TODO: In this case we should check the time before we apply the settings
     * to make sure we apply them after blanking.
     */

    return status;
}

/**
 * Apply current ae results to sensor
 *
 * \param[in] expParams sensor exposure parameters to apply
 * \param[in] noDelay apply directly sensor parameters
 * \return status of IOCTL to sensor AE settings.
 */
status_t SyncManager::applySensorParams(ia_aiq_exposure_sensor_parameters &expParams,
                                        bool noDelay)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    uint16_t aGain, dGain;
    // Frame duration
    status |= mSensorOp->setFrameDuration(expParams.line_length_pixels,
                                          expParams.frame_length_lines);
    // gain
    mDelayedAGains.push_back(expParams.analog_gain_code_global);
    mDelayedDGains.push_back(expParams.digital_gain_global);
    aGain = mDelayedAGains[0];
    dGain = mDelayedDGains[0];
    if (mDelayedAGains.size() > mGainDelay) {
        mDelayedAGains.erase(mDelayedAGains.begin());
        mDelayedDGains.erase(mDelayedDGains.begin());
    }

    if (mDigiGainOnSensor == false)
        dGain = 0;

    status |= mSensorOp->setGains(noDelay ? expParams.analog_gain_code_global : aGain,
                                  noDelay ? expParams.digital_gain_global : dGain);

    // set exposure at last in order to trigger sensor driver apply all exp settings
    status |= mSensorOp->setExposure(expParams.coarse_integration_time,
                                    expParams.fine_integration_time);

    return status;
}

/**
 * enqueue request to start
 *
 * \return value of success for enqueueing message
 */
status_t SyncManager::start()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;
    msg.id = MESSAGE_ID_START;
    return mMessageQueue.send(&msg, MESSAGE_ID_START);
}

/**
 *
 * Request start of polling to sensor sync events
 *
 * \return OK
 */
status_t SyncManager::handleMessageStart()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;
    if (mStarted) {
        LOGW("SyncManager already started");
        mMessageQueue.reply(MESSAGE_ID_START, OK);
        return OK;
    }

    status = initSynchronization();
    if (CC_UNLIKELY(status != NO_ERROR)) {
       LOGE("Failed to initialize CSI synchronization");
       mMessageQueue.reply(MESSAGE_ID_START, UNKNOWN_ERROR);
       return UNKNOWN_ERROR;
    }

    if (!mQueuedSettings.empty()) {
        ia_aiq_exposure_sensor_parameters &expParams =
                  *mQueuedSettings[0]->aiqResults.aeResults.exposures[0].sensor_exposure;

        LOG1("Applying FIRST settings");
        status = applySensorParams(expParams);
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Failed to apply sensor parameters.");
        }

        /**
         * Mark the exposure id where this settings should be in effect
         */
        mQueuedSettings[0]->inEffectFrom = 0;
        mQueuedSettings.erase(mQueuedSettings.begin());
    }
    // Start to poll
    mPollerThread->pollRequest(0, 1000,
                              (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mDevicesToPoll);
    mStarted = true;
    mMessageQueue.reply(MESSAGE_ID_START, OK);
    return OK;
}

/**
 * enqueue requeue request for isStarted
 *
 * \return value of success for enqueueing message
 */
status_t SyncManager::isStarted(bool &started)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;
    msg.id = MESSAGE_ID_IS_STARTED;
    msg.data.isStarted.value = &started;
    return mMessageQueue.send(&msg, MESSAGE_ID_IS_STARTED);
}

/**
 * tells if the event is taken into account or not.
 *
 * \return OK
 */
status_t SyncManager::handleMessageIsStarted(Message &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    *msg.data.isStarted.value = mStarted;

    mMessageQueue.reply(MESSAGE_ID_IS_STARTED, OK);
    return OK;
}
/**
 * enqueue request to stop
 *
 * \return value of success for enqueueing message
 */
status_t SyncManager::stop()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;
    msg.id = MESSAGE_ID_STOP;
    return mMessageQueue.send(&msg,MESSAGE_ID_STOP);
}

/**
 * empty queues and request stop.
 *
 * De initializes synchronization mechanism.
 *
 * \return OK
 */
status_t SyncManager::handleMessageStop()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    if (mStarted) {
        status = mPollerThread->flush(true);
        if (status != NO_ERROR)
            LOGE("could not flush Sensor pollerThread");
        mStarted = false;
    }

    status = deInitSynchronization();
    mMessageQueue.reply(MESSAGE_ID_STOP, status);
    return OK;
}

/**
 * enqueue request to flush the queue of polling requests
 * synchronous call
 *
 * \return value of success for enqueueing message
 */
status_t SyncManager::flush()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

/**
 * empty queues and request of sensor settings
 *
 * \return flush status from Pollerthread.
 */
status_t SyncManager::handleMessageFlush()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    if (mPollerThread)
        status = mPollerThread->flush(true);

    mQueuedSettings.clear();

    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
}

status_t SyncManager::requestExitAndWait(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}

status_t SyncManager::handleMessageExit(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mThreadRunning = false;
    mMessageQueue.reply(MESSAGE_ID_EXIT, NO_ERROR);
    return NO_ERROR;
}

void SyncManager::messageThreadLoop(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mThreadRunning = true;

    while (mThreadRunning) {
        status_t status = NO_ERROR;
        Message msg;
        mMessageQueue.receive(&msg);

        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_INIT:
            status = handleMessageInit(msg);
            break;
        case MESSAGE_ID_GET_SENSOR_MODEDATA:
            status = handleMessageGetSensorModeData(msg);
            break;
        case MESSAGE_ID_START:
            status = handleMessageStart();
            break;
        case MESSAGE_ID_IS_STARTED:
            status = handleMessageIsStarted(msg);
            break;
        case MESSAGE_ID_STOP:
            status = handleMessageStop();
            break;
        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;
        case MESSAGE_ID_SET_PARAMS:
            status = handleMessageSetParams(msg);
            break;
        case MESSAGE_ID_SOF:
            status = handleMessageSOF(msg);
            break;
        case MESSAGE_ID_EOF:
            status = handleMessageEOF(msg);
            break;
        case MESSAGE_ID_SET_SENSOR_FT:
            status = handleMessageSetSensorFT(msg);
            break;
        default:
            LOGE("ERROR @%d: Unknown message %d", status, (int)msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE(" error %d in handling message: %d", status, (int)msg.id);
    }
}

int SyncManager::getCurrentCameraId(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    return mCameraId;
}

/**
 * gets notification everytime an event is triggered
 * in the PollerThread SyncManager is subscribed.
 *
 * \param[in] pollEventMsg containing information on poll data
 *
 * \return status on parsing poll data validity
 */
status_t SyncManager::notifyPollEvent(PollEventMessage *pollEventMsg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    struct v4l2_event event;
    Message msg;
    status_t status = NO_ERROR;

    CLEAR(event);
    if (pollEventMsg == nullptr || pollEventMsg->data.activeDevices == nullptr)
        return BAD_VALUE;

    if (pollEventMsg->id == POLL_EVENT_ID_ERROR) {
        LOGE("Polling Failed for frame id: %d, ret was %d",
              pollEventMsg->data.reqId,
              pollEventMsg->data.pollStatus);

        // Poll again
        mPollerThread->pollRequest(0, 1000,
                           (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mDevicesToPoll);
    } else if (pollEventMsg->data.activeDevices->empty()) {
        LOG1("%s Polling from Flush: succeeded", __FUNCTION__);
        // TODO flush actions.
    } else {
        //cannot be anything else than event if ending up here.
        do {
            status = mIsysReceiverSubdev->dequeueEvent(&event);
            if (status < 0) {
                LOGE("dequeueing event failed");
                break;
            }

            msg.data.frameEvent.timestamp.tv_sec = event.timestamp.tv_sec;
            msg.data.frameEvent.timestamp.tv_usec = (event.timestamp.tv_nsec / 1000);
            msg.data.frameEvent.exp_id = event.u.frame_sync.frame_sequence;
            msg.data.frameEvent.reqId = pollEventMsg->data.reqId;
            if (mFrameSyncSource == FRAME_SYNC_SOF) {
                msg.id = MESSAGE_ID_SOF;
            } else if (mFrameSyncSource == FRAME_SYNC_EOF) {
                msg.id = MESSAGE_ID_EOF;
            } else {
                msg.id = MESSAGE_ID_MAX;
                LOGE("Message ID = MESSAGE_ID_MAX should never end up here");
            }
            mMessageQueue.send(&msg);
            LOG2("%s: EVENT, MessageId: %d, activedev: %d, reqId: %d, seq: %u, frame sequence: %u",
                 __FUNCTION__, pollEventMsg->id, pollEventMsg->data.activeDevices->size(),
                 pollEventMsg->data.reqId, event.sequence, event.u.frame_sync.frame_sequence);

            // notify SOF event
            mSofListener->notifySofEvent(event.u.frame_sync.frame_sequence);
        } while (event.pending > 0);
    }
    return OK;
}

/**
 * Open the sync sub-device  (CSI receiver).
 * Subscribe to SOF events.
 * Create and initialize the poller thread to poll SOF events from it.
 *
 * \return OK
 * \return UNKNOWN_ERROR If we could not find the CSI receiver sub-device.
 * \return NO_DATA If we could not allocate the PollerThread
 */
status_t SyncManager::initSynchronization()
{
    status_t status = OK;
    mDevicesToPoll.clear();
    /*
     * find the sub-device that represent the csi receiver, open it and keep a
     * reference in mIsysReceiverSubdev
     */
    status = setMediaEntity("csi_receiver", SUBDEV_ISYSRECEIVER);
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGE("Cannot find the isys csi-receiver");
        return UNKNOWN_ERROR;
    }

    if (CC_UNLIKELY(mIsysReceiverSubdev == nullptr)) {
        LOGE("ISYS Receiver Sub device to poll is nullptr");
        return UNKNOWN_ERROR;
    }

    /*
     *  SOF is checked first and preferred to EOF, because it provides a better
     *  timing on how to apply the parameters and doesn't include any
     *  calculation.
     */
    status = mIsysReceiverSubdev->subscribeEvent(FRAME_SYNC_SOF);
    if (status != NO_ERROR) {
        LOG1("SOF event not supported on ISYS receiver node, trying EOF");
        status = mIsysReceiverSubdev->subscribeEvent(FRAME_SYNC_EOF);
        if (status != NO_ERROR) {
            LOGE("EOF event not existing on ISYS receiver node, FAIL");
            return status;
        }
        mFrameSyncSource = FRAME_SYNC_EOF;
        LOG1("%s: Using EOF event", __FUNCTION__);
    } else {
        mFrameSyncSource = FRAME_SYNC_SOF;
        LOG1("%s: Using SOF event", __FUNCTION__);
    }

    mDevicesToPoll.push_back(mIsysReceiverSubdev);

    mPollerThread.reset(new PollerThread("SensorPollerThread"));

    status = mPollerThread->init((std::vector<std::shared_ptr<V4L2DeviceBase>>&)mDevicesToPoll,
                                 this, POLLPRI | POLLIN | POLLERR, false);
    if (status != NO_ERROR)
        LOGE("Failed to init PollerThread in sync manager");

    return status;
}

/**
 *
 * Un-subscribe from the events
 * Delete the poller thread.
 * Clear list of devices to poll.
 *
 * NOTE: It assumes that the poller thread is stopped.
 * \return OK
 */
status_t SyncManager::deInitSynchronization()
{
    if (mIsysReceiverSubdev) {
        if (mFrameSyncSource != FRAME_SYNC_NA)
            mIsysReceiverSubdev->unsubscribeEvent(mFrameSyncSource);
        mIsysReceiverSubdev->close();
        mIsysReceiverSubdev.reset();
        mFrameSyncSource = FRAME_SYNC_NA;
    }
    if (mPollerThread)
        mPollerThread->requestExitAndWait();
    mPollerThread.reset();
    mDevicesToPoll.clear();

    return OK;
}
}   // namespace camera2
}   // namespace android
