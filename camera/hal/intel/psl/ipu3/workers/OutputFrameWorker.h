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

#ifndef PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_
#define PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_

#include "FrameWorker.h"
#include "tasks/ICaptureEventSource.h"
#include "tasks/JpegEncodeTask.h"

#include <cros-camera/camera_thread.h>

namespace android {
namespace camera2 {

class OutputFrameWorker: public FrameWorker, public ICaptureEventSource
{
public:
    OutputFrameWorker(std::shared_ptr<cros::V4L2VideoNode> node, int cameraId, camera3_stream_t* stream,
                      IPU3NodeNames nodeName, size_t pipelineDepth);
    virtual ~OutputFrameWorker();

    void addListener(camera3_stream_t* stream);
    void clearListeners();
    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    class SWPostProcessor {
    public:
        enum PostProcessType {
            // Processe frame in order
            PROCESS_NONE = 0,
            PROCESS_ROTATE = 1<<0,
            PROCESS_SCALING = 1<<1,
            PROCESS_JPEG_ENCODING = 1<<2
        };

    public:
        SWPostProcessor(int cameraId);
        ~SWPostProcessor();

        status_t configure(camera3_stream_t* outStream, int inputW, int inputH,
                           int inputFmt = V4L2_PIX_FMT_NV12);
        bool needPostProcess() const {
            return (mProcessType != PROCESS_NONE);
        };
        status_t processFrame(std::shared_ptr<CameraBuffer>& input,
                              std::shared_ptr<CameraBuffer>& output,
                              std::shared_ptr<ProcUnitSettings>& settings,
                              Camera3Request* request);

    private:
        int getRotationDegrees(camera3_stream_t* stream) const;
        status_t convertJpeg(std::shared_ptr<CameraBuffer> buffer,
                             std::shared_ptr<CameraBuffer> jpegBuffer,
                             Camera3Request *request);

    private:
        int mCameraId;
        int mProcessType;
        camera3_stream_t* mStream;
        /* Working buffers for post-processing*/
        std::vector<uint8_t> mRotateBuffer;
        std::vector<std::shared_ptr<CameraBuffer>> mPostProcessBufs;

        std::unique_ptr<JpegEncodeTask> mJpegTask;
    };

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

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_ */
