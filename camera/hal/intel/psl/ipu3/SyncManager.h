/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#ifndef CAMERA3_HAL_SYNCMANAGER_H_
#define CAMERA3_HAL_SYNCMANAGER_H_

#include <vector>
#include <memory>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <string>
#include "ICameraIPU3HwControls.h"
#include "PlatformData.h"
#include "IPU3CameraCapInfo.h"
#include "MessageThread.h"
#include "MessageQueue.h"
#include "Camera3Request.h"
#include "PollerThread.h"
#include "CaptureUnitSettings.h"
#include "Intel3aPlus.h"
#include "CaptureUnitSettings.h"
#include "SensorHwOp.h"

namespace android {
namespace camera2 {

class MediaController;
class MediaEntity;

typedef enum {
    SUBDEV_PIXEL_ARRAY,
    SUBDEV_ISYSRECEIVER,
    SUBDEV_ISYSBACKEND
} sensorEntityType;

class ISettingsSyncListener {
public:
    virtual ~ISettingsSyncListener() {};
};

/**
 * \class SyncManager
 * This class adds the methods that are needed
 * to control the request settings and timing for
 * the flash and sensorHw
 *
 */
class SyncManager : public IMessageHandler,
                    public IPollEventListener {

public:
    SyncManager(int32_t cameraId,
                std::shared_ptr<MediaController> mediaCtl,
                ISettingsSyncListener *syncListener);
    ~SyncManager();


    status_t init(int32_t exposureDelay, int32_t gainDelay);
    status_t getSensorModeData(ia_aiq_exposure_sensor_descriptor &desc);
    status_t requestExitAndWait();
    status_t stop();
    status_t start();
    status_t isStarted(bool &isStarted);
    status_t flush();
    status_t setParameters(std::shared_ptr<CaptureUnitSettings> &settings);
    status_t setSensorFT(int width, int height);

    virtual int32_t getCurrentCameraId(void);
    /* ICaptureEventListener interface*/
    status_t notifyPollEvent(PollEventMessage *pollEventMsg);
private:
    int32_t mCameraId;
    const IPU3CameraCapInfo *mCapInfo; /* SyncManager doesn't own mCapInfo */
    std::shared_ptr<MediaController> mMediaCtl;

    ISettingsSyncListener *mSettingsSyncListener; /* SyncManager doesn't own mSettingsSyncListener */
    std::unique_ptr<PollerThread> mPollerThread;

    std::shared_ptr<V4L2Subdevice> mPixelArraySubdev;
    std::shared_ptr<V4L2Subdevice> mIsysReceiverSubdev;

    std::vector<std::shared_ptr<V4L2Subdevice>> mDevicesToPoll;

    SensorType                      mSensorType;
    std::shared_ptr<SensorHwOp>     mSensorOp;

    //Thread message id's
    enum MessageId {
        MESSAGE_ID_EXIT = 0, // messages from client
        MESSAGE_ID_INIT,
        MESSAGE_ID_GET_SENSOR_MODEDATA,
        MESSAGE_ID_START,
        MESSAGE_ID_IS_STARTED,
        MESSAGE_ID_STOP,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_SET_PARAMS,
        MESSAGE_ID_SOF,     // messages from sensor
        MESSAGE_ID_EOF,
        MESSAGE_ID_SET_SENSOR_FT,
        MESSAGE_ID_MAX      // error
    };

    //frame sync
    enum FrameSyncSource {
        FRAME_SYNC_NA,
        FRAME_SYNC_SOF = V4L2_EVENT_FRAME_SYNC,
        FRAME_SYNC_EOF // TODO: uncomment: = V4L2_EVENT_FRAME_END ; missing from Chromium kernel..
    } mFrameSyncSource;

    // message id and message data
    struct MessageSensorModeData {
        ia_aiq_exposure_sensor_descriptor *desc;
    };

    struct MessageFrameEvent {
        uint32_t exp_id;
        int32_t reqId;
        struct timeval timestamp;
    };

    struct MessageAeParams {
        ia_aiq_ae_results aeResults;
        int32_t requestId;
    };

    struct MessageIsStarted {
        bool *value;
    };

    struct MessageSensorFT {
        int width;
        int height;
    };

    struct MessageInit {
        int32_t exposureDelay;
        int32_t gainDelay;
    };

    union MessageData {
        MessageSensorModeData sensorModeData;
        MessageAeParams aeParams;
        MessageFrameEvent frameEvent;
        MessageIsStarted isStarted;
        MessageInit init;
        MessageSensorFT sFT;
    };

    struct Message {
        MessageId id;
        MessageData data;
        std::shared_ptr<CaptureUnitSettings> settings;
        Message() : id(MESSAGE_ID_EXIT), settings(nullptr) {}
    };

    MessageQueue<Message, MessageId> mMessageQueue;
    std::shared_ptr<MessageThread> mMessageThread;
    bool mThreadRunning;
    bool mStarted;

    /**
     * Settings Q control
     */
    std::vector<std::shared_ptr<CaptureUnitSettings>> mQueuedSettings;
    /**
     * Sensor delay model characterization, these are static values from
     * the XML config.
     */
    int32_t mExposureDelay;
    uint32_t mGainDelay;
    bool mDigiGainOnSensor;
    std::vector<int16_t> mDelayedAGains; /**< analog gain delay buffer */
    std::vector<int16_t> mDelayedDGains; /**< digital gain delay buffer */
    /**
     * Sensor frame rate debugging
     */
    int64_t mCurrentSettingIdentifier;

private:
    /* IMessageHandler overloads */
    virtual void messageThreadLoop(void);
    status_t handleMessageExit();
    status_t handleMessageInit(Message &msg);
    status_t handleMessageGetSensorModeData(Message &msg);
    status_t handleMessageFlush();
    status_t handleMessageStart();
    status_t handleMessageIsStarted(Message &msg);
    status_t handleMessageStop();
    status_t handleMessageSetParams(Message &msg);
    status_t handleMessageSOF(Message &msg);
    status_t handleMessageEOF(Message &msg);
    status_t handleMessageSetSensorFT(Message &msg);

    status_t initSynchronization();
    status_t deInitSynchronization();
    status_t setMediaEntity(const std::string &name, const sensorEntityType type);
    status_t createSensorObj();
    status_t applySensorParams(ia_aiq_exposure_sensor_parameters &expParams,
                               bool noDelay = false);
    status_t setSubdev(std::shared_ptr<MediaEntity> entity, sensorEntityType type);
}; //class SyncManager

} // namespace camera2
} // namespace android
#endif
