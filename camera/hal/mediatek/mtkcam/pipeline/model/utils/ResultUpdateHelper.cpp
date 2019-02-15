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

#define LOG_TAG "mtkcam-ResultUpdateHelper"

#include <impl/ResultUpdateHelper.h>

#include <memory>
#include <mtkcam/pipeline/hwnode/NodeId.h>

#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

auto ResultUpdateHelper(std::weak_ptr<IPipelineModelCallback> const& rpCallback,
                        uint32_t requestNo,
                        std::shared_ptr<IMetaStreamBuffer> resultMeta,
                        bool isLastPartial) -> void {
  CAM_TRACE_NAME(__FUNCTION__);

  MINTPTR const userId = eNODEID_UNKNOWN;

  std::shared_ptr<IPipelineModelCallback> pCallback;
  pCallback = rpCallback.lock();
  if (CC_UNLIKELY(!pCallback.get())) {
    MY_LOGE("Have not set callback to session");
    return;
  }

  UserOnFrameUpdated params;
  params.requestNo = requestNo;
  params.userId = userId;
  // we don't know the actual number. device check this value >=0 to identify
  // last partial result.
  params.nOutMetaLeft = (isLastPartial) ? 0 : 1;
  params.vOutMeta.push_back(resultMeta);

  pCallback->onFrameUpdated(params);
}
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
