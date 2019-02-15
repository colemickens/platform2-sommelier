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

#define LOG_TAG "mmsdk/EffectHalBase"

#include <mtkcam/feature/effectHalBase/EffectHalBase.h>
#include <mtkcam/utils/std/Log.h>

#include <memory>
#include <string>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/

#define AT_STATE(candidate) (mState & (candidate))

//-----------------------------------------------------------------------------
// public: // may change state
//-----------------------------------------------------------------------------
EffectHalBase::EffectHalBase() : mState(STATE_UNINIT), mPrepared(0), mUid(0) {
  FUNCTION_LOG_START;
  FUNCTION_LOG_END_MUM;
}

EffectHalBase::~EffectHalBase() {
  FUNCTION_LOG_START;
  /*mpListener = NULL;*/
  FUNCTION_LOG_END_MUM;
}

MERROR
EffectHalBase::init() {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_UNINIT)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  ret = initImpl();

  mState = STATE_INIT;
  mUid = 0;

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::uninit() {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_INIT)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  //
  ret = uninitImpl();
  mState = STATE_UNINIT;

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::configure() {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_INIT)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  if (allParameterConfigured()) {
    mState = STATE_CONFIGURED;
  }

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::unconfigure() {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_CONFIGURED)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  ret = release();
  if (ret == OK) {
    mState = STATE_INIT;
  }

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

uint64_t EffectHalBase::start() {
  FUNCTION_LOG_START;
  MERROR ret = OK;
  static uint64_t uid = 0;

  // check state machine
  if (!AT_STATE(STATE_CONFIGURED)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  ++uid;
  // action
  ret = startImpl(&uid);
  if (ret == OK) {
    mState = STATE_RUNNING;
    mUid = uid;
  }
  ALOGD("[%s]: uid=%" PRIu64 ", mState=%d", __FUNCTION__, mUid, mState);

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return mUid;
}

MERROR
EffectHalBase::abort(EffectParameter const* parameter) {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  EffectResult result;
  std::shared_ptr<IEffectListener> listener;

  // check state machine
  if (!AT_STATE(STATE_RUNNING)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  // action
  ret = abortImpl(&result, parameter);
  if (ret == OK) {
    mState = STATE_CONFIGURED;
  }

  // listener
  listener = mpListener.lock();

  if (listener) {
    if (ret == OK) {
      listener->onAborted(NULL, result);
    } else {
      listener->onFailed(NULL, result);
    }
  }

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

//-----------------------------------------------------------------------------
// public: // would not change state
//-----------------------------------------------------------------------------
MERROR
EffectHalBase::getNameVersion(const EffectHalVersion& nameVersion) const {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_UNINIT | STATE_INIT | STATE_CONFIGURED | STATE_RUNNING)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  //
  ret = getNameVersionImpl(nameVersion);

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::setEffectListener(
    std::weak_ptr<IEffectListener> const& listener) {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_INIT | STATE_CONFIGURED)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  mpListener = listener;

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::setParameter(const std::string& key, const std::string& object) {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_INIT | STATE_CONFIGURED | STATE_RUNNING)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }
  // @todo make sure paramter could be change is this state.
  // e.g. parameters related with capture session are only allowed in STATE_INIT

  // action
  ALOGD("SET_PARAMETER key=%s, value:%s", key.c_str(), object.c_str());
  ret = setParameterImpl(key, object);

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::setParameters(
    std::shared_ptr<EffectParameter> const& parameter) {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_INIT | STATE_CONFIGURED | STATE_RUNNING)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }
  // action
  ret = setParametersImpl(parameter);

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::getCaptureRequirement(
    EffectParameter* inputParam,
    const std::vector<EffectCaptureRequirement>& requirements) const {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_CONFIGURED | STATE_RUNNING)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  // action
  ret = getCaptureRequirementImpl(inputParam, requirements);

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

// non-blocking
MERROR
EffectHalBase::prepare() {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_CONFIGURED)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }
  if (mPrepared) {
    ALOGD("skip prepare action since already prepared");
    goto FUNCTION_END;
  }

  //
  ret = prepareImpl();  // @todo use thread to handle this

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::prepareDone(const EffectResult& result, MERROR state) {
  FUNCTION_LOG_START;
  MERROR ret = state;

  // action
  if (ret == OK) {
    mPrepared = true;
  }
  ALOGD("prepareDone mPrepared=%d", mPrepared);

  // listener
  std::shared_ptr<IEffectListener> listener = mpListener.lock();
  if (listener) {
    if (ret == OK) {
      listener->onPrepared(NULL, result);
    } else {
      listener->onFailed(NULL, result);
    }
  }

  FUNCTION_LOG_END_MUM;
  return ret;
}

MERROR
EffectHalBase::release() {
  FUNCTION_LOG_START;
  MERROR ret = OK;

  // check state machine
  if (!AT_STATE(STATE_CONFIGURED)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }
  if (!mPrepared) {
    ALOGD("skip relase action since not prepared");
    goto FUNCTION_END;
  }

  //
  ret = releaseImpl();
  if (ret == OK) {
    mPrepared = false;
  }

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}

// non-blocking
MERROR
EffectHalBase::updateEffectRequest(
    std::shared_ptr<EffectRequest> const& request) {
  FUNCTION_LOG_START;
  MERROR ret = OK;
  // check state machine
  if (!AT_STATE(STATE_RUNNING)) {
    ret = INVALID_OPERATION;
    ALOGE("can't call this function at state %d", mState);
    goto FUNCTION_END;
  }

  // action
  ret = updateEffectRequestImpl(request);

FUNCTION_END:
  FUNCTION_LOG_END_MUM;
  return ret;
}
