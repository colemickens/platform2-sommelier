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

#ifndef PSL_IPU3_WORKERS_INPUTFRAMEWORKER_H_
#define PSL_IPU3_WORKERS_INPUTFRAMEWORKER_H_

#include "FrameWorker.h"

namespace android {
namespace camera2 {

class InputFrameWorker : public FrameWorker
{
public:
    InputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId);
    virtual ~InputFrameWorker();

    status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_WORKERS_INPUTFRAMEWORKER_H_ */
