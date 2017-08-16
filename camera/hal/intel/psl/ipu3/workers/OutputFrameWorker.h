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

#ifndef PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_
#define PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_

#include "FrameWorker.h"
#include "tasks/ICaptureEventSource.h"
#include "tasks/JpegEncodeTask.h"

namespace android {
namespace camera2 {

class OutputFrameWorker: public FrameWorker, public ICaptureEventSource
{
public:
    OutputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, camera3_stream_t* stream,
                      IPU3NodeNames nodeName, size_t pipelineDepth);
    virtual ~OutputFrameWorker();

    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    enum PostProcessType {
        PROCESS_NONE = 0,      // pipe outputs directly, don't need any SW process
        PROCESS_JPEG_ENCODING = 1<<0,
        PROCESS_ROTATE = 1<<1,
        PROCESS_DOWNSCALING= 1<<2
    };

private:
    std::unique_ptr<JpegEncodeTask> mJpegTask;
    int needRotation();
    status_t scaleFrame(std::shared_ptr<CameraBuffer> input,
                        std::shared_ptr<CameraBuffer> output);
    status_t rotateFrame(int outFormat, int angle);
    status_t convertJpeg(std::shared_ptr<CameraBuffer> buffer,
                         std::shared_ptr<CameraBuffer> jpegBuffer,
                         Camera3Request *request);
    /* remove after isp is working end here */


private:
    std::shared_ptr<CameraBuffer> mOutputBuffer;
    std::shared_ptr<CameraBuffer> mWorkingBuffer;
    camera3_stream_t* mStream; /* OutputFrameWorker doesn't own mStream */
    bool mAllDone;
    int mPostProcessType;
    IPU3NodeNames mNodeName;

    /* OutputFrameWorker has the ownership of this rotate working buffer */
    std::vector<uint8_t> mRotateBuffer;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_WORKERS_OUTPUTFRAMEWORKER_H_ */
