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

#ifndef PSL_RKISP1_WORKERS_OUTPUTFRAMEWORKER_H_
#define PSL_RKISP1_WORKERS_OUTPUTFRAMEWORKER_H_

#include "FrameWorker.h"
#include "tasks/ICaptureEventSource.h"
#include "tasks/JpegEncodeTask.h"
#include "NodeTypes.h"
namespace android {
namespace camera2 {

class OutputFrameWorker: public FrameWorker, public ICaptureEventSource, public IMessageHandler
{
public:
    OutputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, camera3_stream_t* stream,
                      NodeTypes nodeName, size_t pipelineDepth);
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
            PROCESS_CROP_ROTATE_SCALE = 1 << 0,
            PROCESS_SCALING = 1 << 1,
            PROCESS_JPEG_ENCODING = 1 << 2
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
        status_t scaleFrame(std::shared_ptr<CameraBuffer> input,
                            std::shared_ptr<CameraBuffer> output);
        status_t cropRotateScaleFrame(
                             std::shared_ptr<CameraBuffer> input,
                             std::shared_ptr<CameraBuffer> output,
                             int angle);
        status_t convertJpeg(std::shared_ptr<CameraBuffer> buffer,
                             std::shared_ptr<CameraBuffer> jpegBuffer,
                             Camera3Request *request);

    private:
        int mCameraId;
        int mProcessType;
        camera3_stream_t* mStream;
        /* Working buffers for post-processing*/
        std::vector<uint8_t> mRotateBuffer;
        std::vector<uint8_t> mScaleBuffer;
        std::vector<std::shared_ptr<CameraBuffer>> mPostProcessBufs;

        std::unique_ptr<JpegEncodeTask> mJpegTask;
    };

private:
    struct PostProcFrame {
        PostProcFrame() :
                    processingSettings(nullptr),
                    processBuffer(nullptr),
                    listenBuffer(nullptr),
                    stream(nullptr),
                    request(nullptr) {}
        std::shared_ptr<ProcUnitSettings> processingSettings;
        std::shared_ptr<CameraBuffer> processBuffer;
        std::shared_ptr<CameraBuffer> listenBuffer;
        camera3_stream_t* stream;
        Camera3Request *request;
    };
private:
    enum MessageId {
        MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
        MESSAGE_ID_PROCESS,
        MESSAGE_ID_MAX
    };
    struct Message {
        Message() : id(MESSAGE_ID_MAX) {}
        MessageId id;
        std::shared_ptr<PostProcFrame> frame;
    };

private:
    bool isHalUsingRequestBuffer();
    std::shared_ptr<CameraBuffer> findBuffer(Camera3Request* request,
                                             camera3_stream_t* stream);
    status_t prepareBuffer(std::shared_ptr<CameraBuffer>& buffer);
    bool checkListenerBuffer(Camera3Request* request);
    std::shared_ptr<CameraBuffer> getOutputBufferForListener();
    status_t allocListenerProcessBuffers();
    virtual void messageThreadLoop(void);
    status_t handleMessageProcess(Message & msg);
    void returnBuffers(bool returnListenerBuffers);

private:
    std::vector<std::shared_ptr<CameraBuffer>> mOutputBuffers;
    std::vector<std::shared_ptr<CameraBuffer>> mWorkingBuffers;
    std::shared_ptr<CameraBuffer> mOutputBuffer;
    std::shared_ptr<CameraBuffer> mWorkingBuffer;
    camera3_stream_t* mStream; /* OutputFrameWorker doesn't own mStream */
    bool mNeedPostProcess;
    NodeTypes mNodeName;

    SWPostProcessor mProcessor;

    // For listeners
    std::vector<camera3_stream_t*> mListeners;
    std::map<camera3_stream_t*, std::unique_ptr<SWPostProcessor>> mStreamToSWProcessMap;
    // Put to ISP if requests require listeners'buffer only
    std::shared_ptr<CameraBuffer> mOutputForListener;
    SharedItemPool<PostProcFrame> mPostProcFramePool;

    std::unique_ptr<MessageThread> mMessageThread;
    MessageQueue<Message, MessageId> mMessageQueue;
    bool mThreadRunning;

};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_OUTPUTFRAMEWORKER_H_ */
