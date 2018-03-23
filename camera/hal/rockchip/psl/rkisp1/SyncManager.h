/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#ifndef CAMERA3_HAL_SYNCMANAGER_H_
#define CAMERA3_HAL_SYNCMANAGER_H_

#include <vector>
#include <memory>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <string>
#include "ICameraRKISP1HwControls.h"
#include "PlatformData.h"
#include "RKISP1CameraCapInfo.h"
#include "Camera3Request.h"
#include "PollerThread.h"
#include "CaptureUnitSettings.h"
#include "Rk3aPlus.h"
#include "CaptureUnitSettings.h"
#include "SensorHwOp.h"

#include <cros-camera/camera_thread.h>

namespace android {
namespace camera2 {

class MediaController;
class MediaEntity;

typedef enum {
    SUBDEV_PIXEL_ARRAY,
    SUBDEV_ISYSRECEIVER,
    SUBDEV_ISYSBACKEND
} sensorEntityType;

enum {
    TEST_PATTERN_MODE_OFF = 0,
    TEST_PATTERN_MODE_COLOR_BARS = 1,
    TEST_PATTERN_MODE_DEFAULT = 2,
};

class ISofListener {
public:
    virtual ~ISofListener() {}
    virtual bool notifySofEvent(uint32_t sequence) = 0;
};

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
class SyncManager : public IPollEventListener {

public:
    SyncManager(int32_t cameraId,
                std::shared_ptr<MediaController> mediaCtl,
                ISofListener *sofListener,
                ISettingsSyncListener *syncListener);
    ~SyncManager();


    status_t init(int32_t exposureDelay, int32_t gainDelay);
    status_t getSensorModeData(rk_aiq_exposure_sensor_descriptor &desc);
    status_t stop();
    status_t start();
    status_t isStarted(bool &isStarted);
    status_t flush();
    status_t setParameters(std::shared_ptr<CaptureUnitSettings> settings);
    status_t setSensorFT(int width, int height);

    virtual int32_t getCurrentCameraId(void);
    /* ICaptureEventListener interface*/
    status_t notifyPollEvent(PollEventMessage *pollEventMsg);
private:
    int32_t mCameraId;
    const RKISP1CameraCapInfo *mCapInfo; /* SyncManager doesn't own mCapInfo */
    std::shared_ptr<MediaController> mMediaCtl;

    ISofListener *mSofListener; /* SyncManager doesn't own mSofListener */
    std::unique_ptr<PollerThread> mPollerThread;

    std::shared_ptr<V4L2Subdevice> mPixelArraySubdev;
    std::shared_ptr<V4L2Subdevice> mIsysReceiverSubdev;

    std::vector<std::shared_ptr<V4L2Subdevice>> mDevicesToPoll;

    SensorType                      mSensorType;
    std::shared_ptr<SensorHwOp>     mSensorOp;

    //frame sync
    enum FrameSyncSource {
        FRAME_SYNC_NA,
        FRAME_SYNC_SOF = V4L2_EVENT_FRAME_SYNC,
        FRAME_SYNC_EOF // TODO: uncomment: = V4L2_EVENT_FRAME_END ; missing from Chromium kernel..
    } mFrameSyncSource;

    // message id and message data
    struct MessageSensorModeData {
        rk_aiq_exposure_sensor_descriptor *desc;
    };

    struct MessageFrameEvent {
        uint32_t exp_id;
        int32_t reqId;
        struct timeval timestamp;
    };

    struct MessageFrameEvent        mLatestFrameEventMsg;

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

    cros::CameraThread mCameraThread;
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
    rk_aiq_exposure_sensor_parameters mLatestExpParams;
    uint32_t mLatestInEffectFrom;
private:
    /* IMessageHandler overloads */
    status_t handleInit(MessageInit msg);
    status_t handleGetSensorModeData(MessageSensorModeData msg);
    status_t handleFlush();
    status_t handleStart();
    status_t handleIsStarted(MessageIsStarted msg);
    status_t handleStop();
    status_t applyParameters(std::shared_ptr<CaptureUnitSettings> &settings);
    status_t handleSetParams(std::shared_ptr<CaptureUnitSettings> settings);
    status_t handleSOF(MessageFrameEvent msg);
    status_t handleEOF();
    status_t handleSetSensorFT(MessageSensorFT msg);

    status_t initSynchronization();
    status_t deInitSynchronization();
    status_t setMediaEntity(const std::string &name, const sensorEntityType type);
    status_t createSensorObj();
    status_t applySensorParams(rk_aiq_exposure_sensor_parameters &expParams,
                               bool noDelay = false);
    status_t setSubdev(std::shared_ptr<MediaEntity> entity, sensorEntityType type);
}; //class SyncManager

} // namespace camera2
} // namespace android
#endif
