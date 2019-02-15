/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_MDPWRAPPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_MDPWRAPPER_H_

#include "MtkHeader.h"
#include <queue>
#include <utility>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class MDPWrapper {
 public:
  class OutCollection {
   public:
    MBOOL isFinish() { return isRotFinish() && isNonRotFinish(); }
    MBOOL isRotFinish() { return rotateQ.empty(); }
    MBOOL isNonRotFinish() { return nonRotateQ.empty(); }

    explicit OutCollection(const std::vector<SFPOutput>& _outList)
        : outList(_outList) {
      const size_t s = outList.size();
      for (MUINT32 i = 0; i < s; i++) {
        if (outList[i].mTransform > 0) {
          rotateQ.push(i);
        } else {
          nonRotateQ.push(i);
        }
      }
    }

    const SFPOutput& popFirstRotOut() {
      MUINT32 index = rotateQ.front();
      rotateQ.pop();
      return outList.at(index);
    }

    const SFPOutput& popFirstNonRotOut() {
      MUINT32 index = nonRotateQ.front();
      nonRotateQ.pop();
      return outList.at(index);
    }

    MVOID storeLeftOutputs(std::vector<SFPOutput>* outs) {
      outs->clear();
      outs->reserve(rotateQ.size() + nonRotateQ.size());
      while (!isRotFinish()) {
        const SFPOutput& o = popFirstRotOut();
        outs->push_back(o);
      }
      while (!isNonRotFinish()) {
        const SFPOutput& o = popFirstNonRotOut();
        outs->push_back(o);
      }
    }

   private:
    const std::vector<SFPOutput>& outList;
    std::queue<MUINT32> rotateQ;
    std::queue<MUINT32> nonRotateQ;
  };
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_MDPWRAPPER_H_
