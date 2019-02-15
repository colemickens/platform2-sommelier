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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_DispatchProcessor.h"

#define P2_DISPATCH_THREAD_NAME "p2_dispatch"

#include <memory>
#include <vector>

namespace P2 {

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG DispatchProcessor
#define P2_TRACE TRACE_DISPATCH_PROCESSOR
#include "P2_LogHeader.h"

DispatchProcessor::DispatchProcessor() : Processor(P2_DISPATCH_THREAD_NAME) {
  MY_LOG_FUNC_ENTER();
  mForceProcessor =
      property_get_int32(KEY_P2_PROC_POLICY, VAL_P2_PROC_DFT_POLICY);
  mBasicProcessor = std::make_shared<BasicProcessor>();
  mStreamingProcessor = std::make_shared<StreamingProcessor>();
  MY_LOG_FUNC_EXIT();
}

DispatchProcessor::~DispatchProcessor() {
  MY_LOG_S_FUNC_ENTER(mLog);
  this->uninit();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL DispatchProcessor::onInit(const P2InitParam& param) {
  ILog log = param.mP2Info.mLog;
  MY_LOG_S_FUNC_ENTER(log);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:init()");

  MBOOL ret = MTRUE;

  mP2Info = param.mP2Info;
  mLog = mP2Info.mLog;

  mEnableBasic = (mForceProcessor == P2_POLICY_FORCE_BASIC) ||
                 USE_BASIC_PROCESSOR ||
                 (mP2Info.getConfigInfo().mP2Type == P2_HS_VIDEO);

  mEnableStreaming = (mP2Info.getConfigInfo().mP2Type != P2_HS_VIDEO) &&
                     (mForceProcessor == P2_POLICY_DYNAMIC ||
                      mForceProcessor == P2_POLICY_FORCE_STREAMING);

  MY_LOGI("Enable Basic/Streaming (%d/%d)", mEnableBasic, mEnableStreaming);

  mBasicProcessor->setEnable(mEnableBasic);
  mStreamingProcessor->setEnable(mEnableStreaming);

  ret = ret && mBasicProcessor->init(param) && mStreamingProcessor->init(param);

  mDumpPlugin = std::make_shared<P2DumpPlugin>();
  mScanlinePlugin = std::make_shared<P2ScanlinePlugin>();
  mDrawIDPlugin = std::make_shared<P2DrawIDPlugin>();

  MY_LOG_S_FUNC_EXIT(log);
  return ret;
}

MVOID DispatchProcessor::onUninit() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:uninit()");

  mBasicProcessor->uninit();
  mStreamingProcessor->uninit();

  mDumpPlugin = nullptr;
  mScanlinePlugin = nullptr;
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID DispatchProcessor::onThreadStart() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:threadStart()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID DispatchProcessor::onThreadStop() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:threadStop()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL DispatchProcessor::onConfig(const P2ConfigParam& param) {
  MY_LOG_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:config()");
  mP2Info = param.mP2Info;
  ret = mBasicProcessor->config(param) && mStreamingProcessor->config(param);
  MY_LOG_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL DispatchProcessor::onEnque(
    const std::shared_ptr<P2FrameRequest>& request) {
  ILog reqLog = spToILog(request);
  TRACE_S_FUNC_ENTER(reqLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:enque()");
  if (request != nullptr) {
    P2DumpType dumpType = P2_DUMP_NONE;
    dumpType = mDumpPlugin->needDumpFrame(request->getFrameID());

    if (dumpType != P2_DUMP_NONE) {
      request->registerImgPlugin(mDumpPlugin);
    }
    if (mScanlinePlugin->isEnabled()) {
      request->registerImgPlugin(mScanlinePlugin, MTRUE);
    }
    if (mDrawIDPlugin->isEnabled()) {
      request->registerImgPlugin(mDrawIDPlugin, MTRUE);
    }

    std::vector<std::shared_ptr<P2Request>> p2Requests =
        request->extractP2Requests();
    if (mEnableStreaming) {
      for (const auto& it : p2Requests) {
        it->mDumpType = dumpType;
      }
      mStreamingProcessor->enque(p2Requests);
    } else {
      for (const auto& it : p2Requests) {
        if (it == nullptr) {
          MY_S_LOGW(reqLog, "Invalid P2Request = nullptr");
          continue;
        }

        if (reqLog.getLogLevel() >= 2) {
          it->dump();
        }
        it->mDumpType = dumpType;

        if (!it->isValidMeta(IN_APP) ||
            !(it->isValidMeta(IN_P1_HAL) || it->isValidMeta(IN_P1_HAL_2))) {
          MY_S_LOGW(reqLog, "Meta check failed: inApp(%d) inHal(%d) inHal2(%d)",
                    it->isValidMeta(IN_APP), it->isValidMeta(IN_P1_HAL),
                    it->isValidMeta(IN_P1_HAL_2));
          continue;
        }

        if (needBasicProcess(it)) {
          mBasicProcessor->enque(it);
        } else {
          it->releaseResource(P2Request::RES_ALL);
        }
      }
    }
  }

  TRACE_S_FUNC_EXIT(reqLog);
  return MTRUE;
}

MVOID DispatchProcessor::onNotifyFlush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:notifyFlush()");
  mBasicProcessor->notifyFlush();
  mStreamingProcessor->notifyFlush();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID DispatchProcessor::onWaitFlush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Dispatch:waitFlush()");
  mBasicProcessor->waitFlush();
  mStreamingProcessor->waitFlush();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL DispatchProcessor::needBasicProcess(
    const std::shared_ptr<P2Request>& request) {
  ILog log = spToILog(request);
  MBOOL ret = MFALSE;
  TRACE_S_FUNC_ENTER(log);
  ret = (request != nullptr && mEnableBasic && request->hasInput() &&
         request->hasOutput());
  TRACE_S_FUNC(log, "in=%d out=%d ret=%d", request->hasInput(),
               request->hasOutput(), ret);
  TRACE_S_FUNC_EXIT(log, "ret=%d", ret);
  return ret;
}

}  // namespace P2
