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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_BasicProcessor.h"
#include "P2_Util.h"
#include <src/pass2/NormalStream.h>

#include <memory>
#include <vector>

#define LOG_TAG "P2_BasicProcessor"

#define P2_BASIC_THREAD_NAME "p2_basic"
#define FORCE_BURST 0

namespace P2 {

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG BasicProcessor
#define P2_TRACE TRACE_BASIC_PROCESSOR
#include "P2_LogHeader.h"

BasicProcessor::BasicProcessor() : Processor(P2_BASIC_THREAD_NAME) {
  MY_LOG_FUNC_ENTER();
  MY_LOG_FUNC_EXIT();
}

BasicProcessor::~BasicProcessor() {
  MY_LOG_S_FUNC_ENTER(mLog);
  this->uninit();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL BasicProcessor::onInit(const P2InitParam& param) {
  ILog log = param.mP2Info.mLog;
  MY_LOG_S_FUNC_ENTER(log);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:init()");

  MBOOL ret = MFALSE;

  mP2Info = param.mP2Info;
  mLog = mP2Info.mLog;
  ret = initNormalStream() && init3A();
  if (!ret) {
    MY_S_LOGE(mLog, "P2_Basic:init fail");
    uninitNormalStream();
    uninit3A();
  }

  MY_LOG_S_FUNC_EXIT(log);
  return ret;
}

MVOID BasicProcessor::onUninit() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:uninit()");
  uninitNormalStream();
  uninit3A();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID BasicProcessor::onThreadStart() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:threadStart()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID BasicProcessor::onThreadStop() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:threadStop()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL BasicProcessor::onConfig(const P2ConfigParam& param) {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:config()");
  mP2Info = param.mP2Info;
  MY_LOG_S_FUNC_EXIT(mLog);
  return MTRUE;
}

MBOOL BasicProcessor::onEnque(const std::shared_ptr<P2Request>& request) {
  ILog log = spToILog(request);
  TRACE_S_FUNC_ENTER(log);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:enque()");
  MBOOL ret = MFALSE;

  processVenc(request);

  if (request != nullptr && request->hasInput() && request->hasOutput()) {
    std::shared_ptr<P2Payload> payload = std::make_shared<P2Payload>(request);
    MUINT32 portFlag = mEnableVencStream ? P2Util::USE_VENC : 0;
    payload->mIO = P2Util::extractSimpleIO(request, portFlag);
    payload->mIO.setUseLMV(MFALSE);

    if (payload->mIO.hasInput() && payload->mIO.hasOutput()) {
      P2MetaSet metaSet = request->getMetaSet();
      /*add for tuning flow*/
      int size = mTuningBuffers.size();
      payload->mIO.mTuningBuffer = mTuningBuffers[size - 1];
      payload->mTuning =
          P2Util::xmakeTuning(request->mP2Pack, payload->mIO, mHal3A, &metaSet);
      request->updateMetaSet(metaSet);
      payload->mQParams =
          P2Util::xmakeQParams(request->mP2Pack, payload->mIO, payload->mTuning,
                               payload->mP2Obj.toPtrTable());
      mTuningBuffers.pop_back();

      payload->mRequest->releaseResource(P2Request::RES_IN_IMG);
      ret = processBurst(payload) || processP2(payload);
    } else {
      checkBurst();
    }
  }

  TRACE_S_FUNC_EXIT(log);
  return ret;
}

MVOID BasicProcessor::onNotifyFlush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:notifyFlush()");
  if (mBurstQueue.size()) {
    updateResult(mBurstQueue, MFALSE);
    mBurstQueue.clear();
  }
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID BasicProcessor::onWaitFlush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:waitFlush()");
  waitP2CBDone();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL BasicProcessor::initNormalStream() {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;

  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "P2_Basic:NormalStream create");
  mNormalStream = std::make_shared<NSCam::v4l2::NormalStream>(
      mP2Info.getConfigInfo().mMainSensorID);
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  if (mNormalStream == nullptr) {
    MY_S_LOGE(mLog, "OOM: cannot create NormalStream");
  } else {
    P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "P2_Basic:NormalStream init");
    ret = mNormalStream->init(getName());
    P2_CAM_TRACE_END(TRACE_ADVANCED);
    if (!ret) {
      MY_S_LOGE(mLog, "NormalStream init failed");
      return MFALSE;
    }
    ret = mNormalStream->requestBuffers(
        NSImageio::NSIspio::EPortIndex_TUNING,
        NSCam::IImageBufferAllocator::ImgParam(0, 0), &mTuningBuffers);
    if (!ret) {
      MY_S_LOGE(mLog, "NormalStream requestBuffers failed");
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MVOID BasicProcessor::uninitNormalStream() {
  TRACE_S_FUNC_ENTER(mLog);
  if (mNormalStream) {
    configVencStream(MFALSE);
    for (auto& it : mTuningBuffers) {
      it->unlockBuf("V4L2");
    }
    mNormalStream->uninit(getName());
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MBOOL BasicProcessor::init3A() {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MTRUE;
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "P2_Basic:3A create");
  MAKE_Hal3A(
      mHal3A, [](IHal3A* p) { p->destroyInstance(LOG_TAG); },
      mP2Info.getConfigInfo().mMainSensorID, LOG_TAG);

  P2_CAM_TRACE_END(TRACE_ADVANCED);
  if (mHal3A == nullptr) {
    MY_S_LOGE(mLog, "OOM: cannot create Hal3A");
    ret = MFALSE;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MVOID BasicProcessor::uninit3A() {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID BasicProcessor::onP2CB(const QParams& qparams,
                             std::shared_ptr<P2Payload> const& payload) {
  TRACE_S_FUNC_ENTER(payload->mRequest->mLog);
  P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:onP2CB()");
  std::shared_ptr<IImageBuffer> tuningBuffer = payload->mIO.mTuningBuffer;
  mTuningBuffers.push_back(tuningBuffer);
  payload->mIO.updateResult(qparams.mDequeSuccess);
}

MBOOL BasicProcessor::processVenc(const std::shared_ptr<P2Request>& request) {
  TRACE_S_FUNC_ENTER(request->mLog);
  MBOOL ret = MTRUE;
  if (request != nullptr) {
    MINT32 fps = 0;
    MSize size;
    std::shared_ptr<P2Meta> meta = request->getMeta(IN_P1_HAL);
    if (tryGet<MINT32>(meta, MTK_P2NODE_HIGH_SPEED_VDO_FPS, &fps) &&
        tryGet<MSize>(meta, MTK_P2NODE_HIGH_SPEED_VDO_SIZE, &size)) {
      MBOOL enable = fps && size.w && size.h;
      ret = configVencStream(enable, fps, size);
    }
  }
  TRACE_S_FUNC_EXIT(request->mLog);
  return ret;
}

MBOOL BasicProcessor::configVencStream(MBOOL enable, MINT32 fps, MSize size) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MTRUE;
  if (enable != mEnableVencStream) {
    ret = enable ? mNormalStream->sendCommand(ESDCmd_CONFIG_VENC_DIRLK, fps,
                                              size.w, size.h)
                 : mNormalStream->sendCommand(ESDCmd_RELEASE_VENC_DIRLK);
    if (!ret) {
      MY_S_LOGW(mLog,
                "Config venc stream failed: enable(%d) fps(%d) size(%dx%d)",
                enable, fps, size.w, size.h);
    } else {
      mEnableVencStream = enable;
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

BasicProcessor::P2Payload::P2Payload() {}

BasicProcessor::P2Payload::P2Payload(const std::shared_ptr<P2Request>& request)
    : mRequest(request) {}

BasicProcessor::P2Payload::~P2Payload() {
  P2Util::releaseTuning(&mTuning);
}

BasicProcessor::P2Cookie::P2Cookie(BasicProcessor* parent,
                                   const std::shared_ptr<P2Payload>& payload)
    : mParent(parent) {
  mPayloads.push_back(payload);
}

BasicProcessor::P2Cookie::P2Cookie(
    BasicProcessor* parent,
    const std::vector<std::shared_ptr<P2Payload>>& payloads)
    : mParent(parent), mPayloads(payloads) {}

ILog BasicProcessor::P2Cookie::getILog() {
  ILog log;
  if (mPayloads.size() && mPayloads[0]->mRequest != nullptr) {
    log = mPayloads[0]->mRequest->mLog;
  }
  return log;
}

ILog BasicProcessor::getILog(const std::shared_ptr<P2Payload>& payload) {
  return payload->mRequest->mLog;
}

ILog BasicProcessor::getILog(
    const std::vector<std::shared_ptr<P2Payload>>& payloads) {
  return payloads[0]->mRequest->mLog;
}

template <typename T>
MBOOL BasicProcessor::processP2(T payload) {
  ILog log = getILog(payload);
  TRACE_S_FUNC_ENTER(log);
  MBOOL ret = MFALSE;
  P2Cookie* cookie = createCookie(log, payload);
  if (cookie != nullptr) {
    P2_CAM_TRACE_NAME(TRACE_ADVANCED, "P2_Basic:drv enq");
    QParams qparams = prepareEnqueQParams(payload);
    qparams.mpCookie = reinterpret_cast<void*>(cookie);
    qparams.mpfnCallback = BasicProcessor::p2CB;
    ret = mNormalStream->enque(&qparams);
    if (!ret) {
      MY_S_LOGW(log, "enque failed");
      updateResult(payload, MFALSE);
      freeCookie(cookie, NO_CHECK_ORDER);
    }
  }
  TRACE_S_FUNC_EXIT(log);
  return ret;
}

QParams BasicProcessor::prepareEnqueQParams(
    std::shared_ptr<P2Payload> payload) {
  TRACE_S_FUNC_ENTER(payload->mRequest->mLog);
  NSCam::Feature::P2Util::printQParams(getILog(payload), payload->mQParams);
  TRACE_S_FUNC_EXIT(payload->mRequest->mLog);
  return payload->mQParams;
}

QParams BasicProcessor::prepareEnqueQParams(
    std::vector<std::shared_ptr<P2Payload>> payloads) {
  TRACE_S_FUNC_ENTER(payloads[0]->mRequest->mLog);
  QParams qparams;
  for (std::shared_ptr<P2Payload> payload : payloads) {
    qparams.mvFrameParams.insert(qparams.mvFrameParams.end(),
                                 payload->mQParams.mvFrameParams.begin(),
                                 payload->mQParams.mvFrameParams.end());
  }
  TRACE_S_FUNC_EXIT(payloads[0]->mRequest->mLog);
  return qparams;
}

MVOID BasicProcessor::updateResult(std::shared_ptr<P2Payload> payload,
                                   MBOOL result) {
  TRACE_S_FUNC_ENTER(payload->mRequest->mLog);
  payload->mQParams.mDequeSuccess = result;
  payload->mRequest->updateResult(result);
  TRACE_S_FUNC_EXIT(payload->mRequest->mLog);
}

MVOID BasicProcessor::updateResult(
    std::vector<std::shared_ptr<P2Payload>> payloads, MBOOL result) {
  TRACE_S_FUNC_ENTER(payloads[0]->mRequest->mLog);
  for (std::shared_ptr<P2Payload> payload : payloads) {
    payload->mQParams.mDequeSuccess = result;
    payload->mRequest->updateResult(result);
  }
  TRACE_S_FUNC_EXIT(payloads[0]->mRequest->mLog);
}

MVOID BasicProcessor::processP2CB(const QParams& qparams, P2Cookie* cookie) {
  TRACE_S_FUNC_ENTER(mLog);
  if (cookie) {
    for (std::shared_ptr<P2Payload> payload : cookie->mPayloads) {
      payload->mQParams.mDequeSuccess = qparams.mDequeSuccess;
      onP2CB(payload->mQParams, payload);
    }
    freeCookie(cookie, CHECK_ORDER);
    cookie = nullptr;
  }
  TRACE_S_FUNC_EXIT(mLog);
}

template <typename T>
BasicProcessor::P2Cookie* BasicProcessor::createCookie(const ILog& log,
                                                       const T& payload) {
  TRACE_S_FUNC_ENTER(log);
  P2Cookie* cookie = nullptr;
  cookie = new P2Cookie(this, payload);
  if (cookie == nullptr) {
    MY_S_LOGE(log, "OOM: cannot create P2Cookie");
  } else {
    std::lock_guard<std::mutex> _lock(mP2CookieMutex);
    mP2CookieList.push_back(cookie);
  }
  TRACE_S_FUNC_EXIT(log);
  return cookie;
}

MBOOL BasicProcessor::freeCookie(P2Cookie* cookie, MBOOL checkOrder) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  if (cookie == nullptr) {
    MY_S_LOGW(mLog, "invalid cookie = nullptr");
  } else {
    std::lock_guard<std::mutex> _lock(mP2CookieMutex);
    auto it = find(mP2CookieList.begin(), mP2CookieList.end(), cookie);
    if (it != mP2CookieList.end()) {
      if (checkOrder && it != mP2CookieList.begin()) {
        MY_S_LOGW(cookie->getILog(), "callback out of order");
      }
      delete cookie;
      cookie = nullptr;
      mP2CookieList.erase(it);
      mP2Condition.notify_all();
      ret = MTRUE;
    } else {
      MY_S_LOGE(mLog, "Cookie not freed: invalid data=%p", cookie);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MVOID BasicProcessor::p2CB(QParams* pQparams) {
  TRACE_FUNC_ENTER();
  P2Cookie* cookie = reinterpret_cast<P2Cookie*>(pQparams->mpCookie);
  if (cookie && cookie->mParent) {
    cookie->mParent->processP2CB(*pQparams, cookie);
  }
  TRACE_FUNC_EXIT();
}

MVOID BasicProcessor::waitP2CBDone() {
  TRACE_S_FUNC_ENTER(mLog);
  std::unique_lock<std::mutex> _lock(mP2CookieMutex);
  while (mP2CookieList.size()) {
    mP2Condition.wait(_lock);
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MBOOL BasicProcessor::processBurst(const std::shared_ptr<P2Payload>& payload) {
  TRACE_S_FUNC_ENTER(payload->mRequest->mLog);
  MUINT32 burst = payload->mRequest->mP2Pack.getConfigInfo().mBurstNum;
  burst = FORCE_BURST ? 4 : burst;
  burst = payload->mRequest->isResized() ? burst : 0;

  MUINT8 reqtSmvrFps = (MUINT8)MTK_SMVR_FPS_30;
  if (payload->mRequest != nullptr) {
    std::shared_ptr<P2Meta> spHalMeta = payload->mRequest->getMeta(IN_P1_HAL);
    if (tryGet<MUINT8>(spHalMeta, MTK_HAL_REQUEST_SMVR_FPS, &reqtSmvrFps)) {
      MY_LOGE("!!err: tryGet IN_P1_HAL error");
    }
  }

  if (reqtSmvrFps != (MUINT8)MTK_SMVR_FPS_30) {
    if (burst > 1) {
      mBurstQueue.push_back(payload);
    }
    if (mBurstQueue.size() && mBurstQueue.size() >= burst) {
      processP2(mBurstQueue);
      mBurstQueue.clear();
    }
  }
  TRACE_S_FUNC_EXIT(payload->mRequest->mLog,
                    "reqtSmvrFps=%d, burst(%d) queueSize(%d)", reqtSmvrFps,
                    burst, mBurstQueue.size());
  return (burst > 1 && reqtSmvrFps != MTK_SMVR_FPS_30);
}

MBOOL BasicProcessor::checkBurst() {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  if (mBurstQueue.size()) {
    processP2(mBurstQueue);
    mBurstQueue.clear();
    ret = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

}  // namespace P2
