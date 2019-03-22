/*
 * Copyright (C) 2019 Intel Corporation.
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

#ifndef SW_POSTPROCESSOR_H_
#define SW_POSTPROCESSOR_H_

#include "cameraOrientationDetector/CameraOrientationDetector.h"
#include "tasks/JpegEncodeTask.h"

namespace cros {
namespace intel {

class SWPostProcessor {
public:
    enum PostProcessType {
        // Processe frame in order
        PROCESS_NONE = 0,
        PROCESS_ROTATE = 1 << 0,
        PROCESS_SCALING = 1 << 1,
        PROCESS_CROP = 1 << 2,
        PROCESS_JPEG_ENCODING = 1 << 3
    };

public:
    SWPostProcessor(int cameraId);
    ~SWPostProcessor();

    status_t configure(camera3_stream_t* outStream,
                       int inputW, int inputH, int inputFmt = V4L2_PIX_FMT_NV12);
    bool needPostProcess() const {return (mProcessType != PROCESS_NONE);}

    // Crops the source frame |srcBuf| to make it has same aspect ratio as reference frame |refBuf|
    // and outputs the result as |dstBuf|.
    status_t cropFrameToSameAspectRatio(std::shared_ptr<CameraBuffer>& srcBuf,
                                        std::shared_ptr<CameraBuffer>& refBuf,
                                        std::shared_ptr<CameraBuffer>* dstBuf);
    status_t scaleFrame(std::shared_ptr<CameraBuffer>& srcBuf,
                        std::shared_ptr<CameraBuffer>& dstBuf);
    status_t processFrame(std::shared_ptr<CameraBuffer>& input,
                          std::shared_ptr<CameraBuffer>& output,
                          std::shared_ptr<ProcUnitSettings>& settings,
                          Camera3Request* request,
                          bool needReprocess);

private:
    status_t lockBuffer(const std::shared_ptr<CameraBuffer>& buffer) const;
    std::shared_ptr<CameraBuffer> requestBuffer(int cameraId, int width, int height);
    void releaseBuffers();
    int getRotationDegrees(camera3_stream_t* stream) const;
    status_t convertJpeg(std::shared_ptr<CameraBuffer> input,
                         std::shared_ptr<CameraBuffer> output,
                         Camera3Request* request);

private:
    int mCameraId;
    int mProcessType;
    camera3_stream_t* mStream;
    /* Working buffers for post-processing*/
    std::vector<uint8_t> mRotateBuffer;
    std::vector<std::shared_ptr<CameraBuffer>> mPostProcessBufs;

    std::unique_ptr<JpegEncodeTask> mJpegTask;
};

} /* namespace intel */
} /* namespace cros */

#endif /* SW_POSTPROCESSOR_H_ */
