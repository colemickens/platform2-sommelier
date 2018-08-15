/*
 * Copyright (C) 2018 Intel Corporation.
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

#ifndef INTEL_FACE_ENGINE_H_
#define INTEL_FACE_ENGINE_H_

#include <vector>
#include <ia_types.h>

#include "Intel3aCommon.h"
#include "IPCFaceEngine.h"

namespace android {
namespace camera2 {
class IntelFaceEngine {
public:
    IntelFaceEngine();
    virtual ~IntelFaceEngine();

    bool init(unsigned int max_face_num, int maxWidth, int maxHeight, face_detection_mode fd_mode);
    void uninit();
    bool prepareRun(const pvl_image& frame);
    bool run(FaceEngineResult* results);

private:
    IPCFaceEngine mIpc;
    Intel3aCommon mCommon;

    bool mInitialized;

    ShmMemInfo mMemInit;
    ShmMemInfo mMemRun;

    std::vector<ShmMem> mMems;

    DISALLOW_COPY_AND_ASSIGN(IntelFaceEngine);
};
} /* namespace camera2 */
} /* namespace android */
#endif // INTEL_FACE_ENGINE_H_
