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

#undef LOG_TAG
#define LOG_TAG "mtkcam-PipelineModelSession"

#include <impl/AppRequestParser.h>
#include <memory>
#include <mtkcam/pipeline/model/session/PipelineModelSessionBase.h>
#include "MyUtils.h"
#include <string>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::pipeline::model::PipelineModelSessionBase;

/******************************************************************************
 *
 ******************************************************************************/
PipelineModelSessionBase::PipelineModelSessionBase(
    std::string const&& sessionName, CtorParams const& rCtorParams)
    : mSessionName(sessionName),
      mStaticInfo(rCtorParams.staticInfo),
      mDebugInfo(rCtorParams.debugInfo),
      mPipelineModelCallback(rCtorParams.pPipelineModelCallback),
      mPipelineSettingPolicy(rCtorParams.pPipelineSettingPolicy) {}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionBase::submitRequest(
    std::vector<std::shared_ptr<UserRequestParams>> const& requests,
    uint32_t* numRequestProcessed) -> int {
  *numRequestProcessed = 0;

  std::vector<std::shared_ptr<ParsedAppRequest>> reqs;
  reqs.reserve(requests.size());

  // Convert: UserRequestParams -> ParsedAppRequest
  for (size_t i = 0; i < requests.size(); i++) {
    auto r = std::make_shared<ParsedAppRequest>();
    RETURN_ERROR_IF_NULLPTR(r, -ENODEV,
                            "i:%zu Fail to make_shared<ParsedAppRequest>", i);

    RETURN_ERROR_IF_NOT_OK(parseAppRequest(r.get(), requests[i].get()),
                           "parseAppRequest fail on requestNo:%u - %zu/%zu "
                           "requests parsed sucessfully",
                           requests[i]->requestNo, i, requests.size());

    // Dump the metadata request if it's not repeating
    if (CC_UNLIKELY(!r->pParsedAppMetaControl->repeating)) {
      MY_LOGD("requestNo:%d %s", r->requestNo,
              toString(*r->pParsedAppMetaControl).c_str());
    }

    reqs.emplace_back(r);
  }

  // Submit ParsedAppRequest one by one
  for (size_t i = 0; i < reqs.size(); i++, (*numRequestProcessed)++) {
    RETURN_ERROR_IF_NOT_OK(submitOneRequest(reqs[i]),
                           "submitOneRequest fail on requestNo:%u - %u/%zu "
                           "requests submitted sucessfully",
                           reqs[i]->requestNo, *numRequestProcessed,
                           reqs.size());
  }

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionBase::beginFlush() -> int {
  auto pPipelineContext = getCurrentPipelineContext();
  RETURN_ERROR_IF_NULLPTR(pPipelineContext, OK, "No current pipeline context");
  RETURN_ERROR_IF_NOT_OK(pPipelineContext->flush(), "PipelineContext::flush()");
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionBase::endFlush() -> void {}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionBase::dumpState(
    const std::vector<std::string>& options __unused) -> void {
  auto pPipelineContext = getCurrentPipelineContext();
  if (pPipelineContext != nullptr) {
    pPipelineContext->dumpState(options);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionBase::determineTimestampSOF(
    StreamId_T const streamId,
    std::vector<std::shared_ptr<IMetaStreamBuffer>> const& vMetaStreamBuffer)
    -> int64_t {
  int64_t timestampSOF = 0;
  for (size_t i = 0; i < vMetaStreamBuffer.size(); i++) {
    auto const& pStreamBuffer = vMetaStreamBuffer[i];
    if (streamId == pStreamBuffer->getStreamInfo()->getStreamId()) {
      auto pMetadata = pStreamBuffer->tryReadLock(LOG_TAG);
      IMetadata::getEntry(pMetadata, MTK_P1NODE_FRAME_START_TIMESTAMP,
                          &timestampSOF);
      pStreamBuffer->unlock(LOG_TAG, pMetadata);
      break;
    }
  }
  return timestampSOF;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionBase::updateFrameTimestamp(
    MUINT32 const requestNo,
    MINTPTR const userId,
    Result const& result,
    int64_t timestampStartOfFrame) -> void {
  if (result.bFrameEnd) {
    return;
  }

  std::shared_ptr<IPipelineModelCallback> pCallback;
  pCallback = mPipelineModelCallback.lock();
  if (CC_UNLIKELY(!pCallback.get())) {
    MY_LOGE("Have not set callback to session");
    return;
  }

  {
    UserOnFrameUpdated params;
    params.requestNo = requestNo;
    params.userId = userId;
    params.nOutMetaLeft = result.nAppOutMetaLeft;
    params.timestampStartOfFrame = timestampStartOfFrame;
    for (size_t i = 0; i < result.vAppOutMeta.size(); ++i) {
      params.vOutMeta.push_back(result.vAppOutMeta[i]);
    }

    pCallback->onFrameUpdated(params);
  }
}
