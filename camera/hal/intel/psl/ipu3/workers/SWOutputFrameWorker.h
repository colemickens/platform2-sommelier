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

#ifndef PSL_IPU3_WORKERS_SWOUTPUTFRAMEWORKER_H_
#define PSL_IPU3_WORKERS_SWOUTPUTFRAMEWORKER_H_

#include "IDeviceWorker.h"
#include "tasks/ICaptureEventSource.h"
#include "tasks/JpegEncodeTask.h"

namespace android {
namespace camera2 {

class SWOutputFrameWorker: public IDeviceWorker, public ICaptureEventListener
{
public:
    SWOutputFrameWorker(int cameraId, camera3_stream_t* stream);
    virtual ~SWOutputFrameWorker();

    virtual bool notifyCaptureEvent(CaptureMessage *msg);

    status_t configure(std::shared_ptr<GraphConfig> &config);
    virtual status_t startWorker() { return OK; };
    virtual status_t stopWorker() { return OK; };
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    std::unique_ptr<JpegEncodeTask> mJpegTask;
    status_t convertJpeg(std::shared_ptr<CameraBuffer> buffer,
                         std::shared_ptr<CameraBuffer> jpegBuffer,
                         Camera3Request *request);

    std::shared_ptr<CameraBuffer> mOutputBuffer;
    std::shared_ptr<CameraBuffer> mInputBuffer;
    camera3_stream_t* mStream;
    bool mHasNewInput;
    bool mAllDone;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_WORKERS_SWOUTPUTFRAMEWORKER_H_ */
