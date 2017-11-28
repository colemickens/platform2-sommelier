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

#ifndef CAMERA3_HAL_CONTROLUNIT_H_
#define CAMERA3_HAL_CONTROLUNIT_H_
#include <memory>
#include <vector>
#include <linux/rkisp1-config.h>
#include "MessageQueue.h"
#include "MessageThread.h"
#include "ImguUnit.h"
#include "SharedItemPool.h"
#include "CaptureUnitSettings.h"
#include "RequestCtrlState.h"
#include "SyncManager.h"
#include "Rk3aRunner.h"

namespace android {
namespace camera2 {

class SettingsProcessor;
class Metadata;

// Forward definitions to avoid extra includes
class IStreamConfigProvider;
struct ProcUnitSettings;
class Rk3aRunner;

/**
 * \class ControlUnit
 *
 * ControlUnit class control the request flow between Capture Unit and
 * Processing Unit. It uses the Rockchip3Aplus to process 3A settings for
 * each request and to run the 3A algorithms.
 *
 */
class ControlUnit : public IMessageHandler, public ICaptureEventListener
{
public:
    explicit ControlUnit(ImguUnit *thePU,
                         int CameraId,
                         IStreamConfigProvider &aStreamCfgProv,
                         std::shared_ptr<MediaController> mc);
    virtual ~ControlUnit();

    status_t init();
    status_t configStreamsDone(bool configChanged);

    status_t processRequest(Camera3Request* request,
                            std::shared_ptr<GraphConfig> graphConfig);

    /* ICaptureEventListener interface*/
    bool notifyCaptureEvent(CaptureMessage *captureMsg);

    status_t flush(void);

public:  /* private types */
    // thread message id's
    enum MessageId {
        MESSAGE_ID_EXIT = 0,
        MESSAGE_ID_NEW_REQUEST,
        MESSAGE_ID_NEW_2A_STAT,
        MESSAGE_ID_NEW_SENSOR_METADATA,
        MESSAGE_ID_NEW_SENSOR_DESCRIPTOR,
        MESSAGE_ID_NEW_SOF,
        MESSAGE_ID_NEW_SHUTTER,
        MESSAGE_ID_NEW_REQUEST_DONE,
        MESSAGE_NEW_CV_RESULT,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_MAX
    };

    struct MessageGeneric {
        bool enable;
    };

    struct MessageRequest {
        unsigned int frame_number;
    };

    struct MessageShutter {
        int requestId;
        int64_t tv_sec;
        int64_t tv_usec;
    };

    struct MessageSensorMode {
        rk_aiq_exposure_sensor_descriptor exposureDesc;
        rk_aiq_frame_params frameParams;
    };

    // union of all message data
    union MessageData {
        MessageGeneric generic;
        MessageRequest request;
        MessageShutter shutter;
        MessageSensorMode sensor;
    };

    // message id and message data
    struct Message {
        MessageId id;
        unsigned int requestId; /**< For raw buffers from CaptureUnit as
                                     they don't have request */
        MessageData data;
        Camera3Request* request;
        std::shared_ptr<RequestCtrlState> state;
        std::shared_ptr<rk_aiq_statistics_input_params> stats;
        CaptureEventType type;
        Message(): id(MESSAGE_ID_EXIT),
            requestId(0),
            request(nullptr),
            state(nullptr),
            type(CAPTURE_EVENT_MAX)
        { CLEAR(data); }
    };

private:
    typedef struct {
        int reqId;
        CaptureUnitSettings *captureSettings;
    } RequestSettings;

private:  /* Methods */
    // prevent copy constructor and assignment operator
    ControlUnit(const ControlUnit& other);
    ControlUnit& operator=(const ControlUnit& other);

    status_t initTonemaps();
    status_t requestExitAndWait();

    /* IMessageHandler overloads */
    virtual void messageThreadLoop();

    status_t handleMessageExit();
    status_t handleNewRequest(Message &msg);
    status_t handleNewStat(Message &msg);
    status_t handleNewRequestDone(Message &msg);
    status_t handleNewSensorDescriptor(Message &msg);
    status_t handleNewSof(Message &msg);
    status_t handleNewShutter(Message &msg);
    status_t handleMessageFlush(void);

    status_t processRequestForCapture(std::shared_ptr<RequestCtrlState> &reqState);

    void     prepareStats(RequestCtrlState &reqState,
                          rk_aiq_statistics_input_params &s);
    status_t completeProcessing(std::shared_ptr<RequestCtrlState> &reqState);
    status_t allocateLscResults();
    status_t acquireRequestStateStruct(std::shared_ptr<RequestCtrlState>& state);
    std::shared_ptr<CaptureUnitSettings> findSettingsInEffect(uint64_t expId);
    status_t getIspRect(rk_aiq_exposure_sensor_descriptor &desc);
    status_t getSensorModeData(rk_aiq_exposure_sensor_descriptor &desc);
    status_t initStaticMetadata();
    status_t applyAeParams(std::shared_ptr<CaptureUnitSettings> &aiqCaptureSettings);

private:  /* Members */
    SharedItemPool<RequestCtrlState> mRequestStatePool;
    SharedItemPool<CaptureUnitSettings> mCaptureUnitSettingsPool;
    SharedItemPool<ProcUnitSettings> mProcUnitSettingsPool;

    std::map<int, std::shared_ptr<RequestCtrlState>> mWaitingForCapture;
    AiqResults mLatestAiqResults;
    int64_t mLatestRequestId;

    ImguUnit       *mImguUnit; /* ControlUnit doesn't own ImguUnit */
    Rk3aPlus    *m3aWrapper;
    int             mCameraId;

    std::shared_ptr<MediaController>             mMediaCtl;

    /**
     * Thread control members
     */
    bool mThreadRunning;
    MessageQueue<Message, MessageId> mMessageQueue;
    std::unique_ptr<MessageThread> mMessageThread;

    /**
     * Settings history
     */
    static const int16_t MAX_SETTINGS_HISTORY_SIZE = 10;
    std::vector<std::shared_ptr<CaptureUnitSettings>>    mSettingsHistory;

    /*
     * Provider of details of the stream configuration
     */
    IStreamConfigProvider &mStreamCfgProv;
    SettingsProcessor *mSettingsProcessor;
    Metadata *mMetadata;

    Rk3aRunner *m3ARunner;
    int mSensorSettingsDelay;
    int mGainDelay;
    bool mLensSupported;
    std::shared_ptr<LensHw> mLensController;
    std::shared_ptr<SyncManager>    mSyncManager;

    rk_aiq_exposure_sensor_descriptor mSensorDescriptor;

    uint32_t mSofSequence;
    int64_t mShutterDoneReqId;
    static const int16_t AWB_CONVERGENCE_WAIT_COUNT = 2;
};  // class ControlUnit

}  // namespace camera2
}  // namespace android

#endif  // CAMERA3_HAL_CONTROLUNIT_H_
