/*
 * Copyright (C) 2017-2019 Intel Corporation.
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

#ifndef PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_
#define PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_

#include <cros-camera/camera_thread.h>
#include "FaceEngine.h"
#include "FrameWorker.h"
#include "tasks/ICaptureEventSource.h"
#include "SWPostProcessor.h"

namespace cros {
namespace intel {

class OutputFrameWorker: public FrameWorker, public ICaptureEventSource
{
public:
    OutputFrameWorker(std::shared_ptr<cros::V4L2VideoNode> node, int cameraId, camera3_stream_t* stream,
                      IPU3NodeNames nodeName, size_t pipelineDepth, FaceEngine* faceEngine);
    virtual ~OutputFrameWorker();

    void addListener(camera3_stream_t* stream);
    void clearListeners();
    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    struct ProcessingData {
        std::shared_ptr<CameraBuffer> mOutputBuffer;
        std::shared_ptr<CameraBuffer> mWorkingBuffer;
        std::shared_ptr<DeviceMessage> mMsg;
    };

private:
    status_t handlePostRun();
    bool isHalUsingRequestBuffer();
    std::shared_ptr<CameraBuffer> findBuffer(Camera3Request* request,
                                             camera3_stream_t* stream);
    status_t prepareBuffer(std::shared_ptr<CameraBuffer>& buffer);
    bool checkListenerBuffer(Camera3Request* request);
    void dump(std::shared_ptr<CameraBuffer> buf, const CameraStream* stream);
    status_t processData(ProcessingData processingData);
    bool isAsyncProcessingNeeded(std::shared_ptr<CameraBuffer> outBuf);

private:
    camera3_stream_t* mStream; /* OutputFrameWorker doesn't own mStream */
    bool mNeedPostProcess;
    IPU3NodeNames mNodeName;

    SWPostProcessor mProcessor;

    int mSensorOrientation;
    FaceEngine* mFaceEngine;
    unsigned int mFaceEngineRunInterval; // run 1 frame every mFaceEngineRunInterval frames.
    unsigned int mFrameCnt; // from 0 to (mFaceEngineRunInterval - 1).
    std::unique_ptr<CameraOrientationDetector> mCamOriDetector;

    // For listeners
    std::vector<camera3_stream_t*> mListeners;
    std::vector<std::unique_ptr<SWPostProcessor> > mListenerProcessors;

    std::vector<std::shared_ptr<CameraBuffer>> mInternalBuffers;

    /**
     * Thread control members
     */
    cros::CameraThread mCameraThread;

    std::queue<ProcessingData> mProcessingDataQueue;
    std::mutex mProcessingDataLock;
    ProcessingData mProcessingData;
    bool mDoAsyncProcess;
};

} /* namespace intel */
} /* namespace cros */

#endif /* PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_ */
