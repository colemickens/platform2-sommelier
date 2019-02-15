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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_NORMALSTREAMBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_NORMALSTREAMBASE_H_

#include "NormalStreamBase_t.h"

#include <featurePipe/common/include/PipeLogHeaderBegin.h>
#include "DebugControl.h"
#define PIPE_TRACE TRACE_NORMAL_STREAM_BASE
#define PIPE_CLASS_TAG "NormalStreamBase"
#include <featurePipe/common/include/PipeLog.h>

#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
NormalStreamBase<T>::QParams_Cookie::QParams_Cookie()
    : mCookie(NULL), mCB(NULL), mFailCB(NULL), mBlockCB(NULL) {}

template <typename T>
NormalStreamBase<T>::QParams_Cookie::QParams_Cookie(const T_Param& param)
    : mCookie(param.mpCookie),
      mCB(param.mpfnCallback),
      mFailCB(param.mpfnEnQFailCallback),
      mBlockCB(param.mpfnEnQBlockCallback) {}

template <typename T>
void NormalStreamBase<T>::QParams_Cookie::replace(T_Param* pParam,
                                                  T_Token token) const {
  pParam->mpCookie = token;
  pParam->mpfnCallback = staticOnNormalStreamCB;
  pParam->mpfnEnQFailCallback = staticOnNormalStreamFailCB;
  pParam->mpfnEnQBlockCallback = staticOnNormalStreamBlockCB;
}

template <typename T>
void NormalStreamBase<T>::QParams_Cookie::restore(T_Param* pParam) const {
  pParam->mpCookie = mCookie;
  pParam->mpfnCallback = mCB;
  pParam->mpfnEnQFailCallback = mFailCB;
  pParam->mpfnEnQBlockCallback = mBlockCB;
}

template <typename T>
typename NormalStreamBase<T>::T_Token
NormalStreamBase<T>::QParams_Cookie::getToken(T_Param* pParam) {
  return pParam->mpCookie;
}

template <typename T>
void NormalStreamBase<T>::enqueNormalStreamBase(
    std::shared_ptr<T_Stream> stream, T_Param* pParam, const T_Data& data) {
  TRACE_FUNC_ENTER();
  mCookieStore.enque(this, stream, pParam, data);
  TRACE_FUNC_EXIT();
}

template <typename T>
void NormalStreamBase<T>::waitNormalStreamBaseDone() {
  TRACE_FUNC_ENTER();
  mCookieStore.waitAllCallDone();
  TRACE_FUNC_EXIT();
}

template <typename T>
void NormalStreamBase<T>::onNormalStreamBaseFailCB(T_Param* pParam,
                                                   const T_Data& data) {
  TRACE_FUNC_ENTER();
  this->onNormalStreamBaseCB(pParam, data);
  TRACE_FUNC_EXIT();
}

template <typename T>
void NormalStreamBase<T>::onNormalStreamBaseBlockCB(T_Param* pParam,
                                                    const T_Data& data) {
  TRACE_FUNC_ENTER();
  this->onNormalStreamBaseCB(pParam, data);
  TRACE_FUNC_EXIT();
}

template <typename T>
void NormalStreamBase<T>::staticOnNormalStreamCB(T_Param* pParam) {
  TRACE_FUNC_ENTER();
  T_Token token = T_Cookie::getToken(pParam);
  T_Store::staticProcessCB(MSG_COOKIE_DONE, pParam, token);
  TRACE_FUNC_EXIT();
}

template <typename T>
void NormalStreamBase<T>::staticOnNormalStreamFailCB(T_Param* pParam) {
  TRACE_FUNC_ENTER();
  T_Token token = T_Cookie::getToken(pParam);
  T_Store::staticProcessCB(MSG_COOKIE_FAIL, pParam, token);
  TRACE_FUNC_EXIT();
}

template <typename T>
void NormalStreamBase<T>::staticOnNormalStreamBlockCB(T_Param* pParam) {
  TRACE_FUNC_ENTER();
  void* token = T_Cookie::getToken(pParam);
  T_Store::staticProcessCB(MSG_COOKIE_BLOCK, pParam, token);
  TRACE_FUNC_EXIT();
}

template <typename T>
bool NormalStreamBase<T>::onCookieStoreEnque(std::shared_ptr<T_Stream> stream,
                                             T_Param* pParam) {
  TRACE_FUNC_ENTER();
  bool ret = false;
  if (stream) {
    ret = stream->enque(pParam);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T>
void NormalStreamBase<T>::onCookieStoreCB(const T_Msg& msg,
                                          T_Param* pParam,
                                          const T_Data& data) {
  TRACE_FUNC_ENTER();
  switch (msg) {
    case MSG_COOKIE_FAIL:
      pParam->mDequeSuccess = MFALSE;
      this->onNormalStreamBaseFailCB(pParam, data);
      break;
    case MSG_COOKIE_BLOCK:
      pParam->mDequeSuccess = MFALSE;
      this->onNormalStreamBaseBlockCB(pParam, data);
      break;
    case MSG_COOKIE_DONE:
    default:
      this->onNormalStreamBaseCB(pParam, data);
      break;
  }
  TRACE_FUNC_EXIT();
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#include <featurePipe/common/include/PipeLogHeaderEnd.h>
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_NORMALSTREAMBASE_H_
