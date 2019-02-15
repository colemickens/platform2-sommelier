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

#define PIPE_CLASS_TAG "P2CamContext"
#define PIPE_TRACE TRACE_P2_CAM_CONTEXT
#include <featurePipe/common/include/DebugControl.h>
#include <featurePipe/common/include/PipeLog.h>
#include <memory>
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include <mtkcam/feature/featureCore/featurePipe/streaming/P2CamContext.h>

using NS3Av3::IHal3A;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

const char* P2CamContext::MODULE_NAME = "FeaturePipe_P2";

std::mutex P2CamContext::sMutex;
std::shared_ptr<P2CamContext>
    P2CamContext::spInstance[P2CamContext::SENSOR_INDEX_MAX];

P2CamContext::P2CamContext(MUINT32 sensorIndex)
    : mRefCount(0),
      mSensorIndex(sensorIndex),
      mIsInited(MFALSE),
      mp3dnr(NULL),
      mPrevFullImg(NULL),
      mp3A(NULL) {}

P2CamContext::~P2CamContext() {
  MY_LOGD("P2CamContext[%d]: destructor is called", mSensorIndex);
  uninit();
}

std::shared_ptr<P2CamContext> P2CamContext::createInstance(
    MUINT32 sensorIndex, const StreamingFeaturePipeUsage& pipeUsage) {
  if (sensorIndex >= SENSOR_INDEX_MAX) {
    return NULL;
  }

  std::lock_guard<std::mutex> lock(sMutex);

  std::shared_ptr<P2CamContext> inst = spInstance[sensorIndex];

  if (inst == NULL) {
    inst = std::make_shared<P2CamContext>(sensorIndex);
    inst->init(pipeUsage);
    spInstance[sensorIndex] = inst;
  }

  inst->mRefCount++;
  MY_LOGD("P2CamContext[%d]: mRefCount increased: %d", sensorIndex,
          inst->mRefCount);

  return inst;
}

void P2CamContext::destroyInstance(MUINT32 sensorIndex) {
  if (sensorIndex >= SENSOR_INDEX_MAX) {
    return;
  }

  std::lock_guard<std::mutex> lock(sMutex);

  std::shared_ptr<P2CamContext> inst = spInstance[sensorIndex];

  if (inst != NULL) {
    // Do not call uninit() here, because the instance may be still hold
    // by some running thread
    inst->mRefCount--;
    MY_LOGD("P2CamContext[%d]: mRefCount decreased: %d", sensorIndex,
            inst->mRefCount);
    if (inst->mRefCount <= 0) {
      spInstance[sensorIndex] = NULL;
    }
  }
}

std::shared_ptr<P2CamContext> P2CamContext::getInstance(MUINT32 sensorIndex) {
  if (sensorIndex >= SENSOR_INDEX_MAX) {
    return NULL;
  }

  // NOTE: sp<> is not atomic, should be kept in critical section
  std::lock_guard<std::mutex> lock(sMutex);

  std::shared_ptr<P2CamContext> inst = spInstance[sensorIndex];
  if (inst == NULL) {
    CAM_LOGF("P2CamContext[%d] was not created!", sensorIndex);
  }

  return inst;
}

void P2CamContext::init(const StreamingFeaturePipeUsage& pipeUsage) {
  TRACE_FUNC_ENTER();

  if (!mIsInited) {
    if (mp3A == nullptr && SUPPORT_3A_HAL) {
      MAKE_Hal3A(
          mp3A, [](IHal3A* p) { p->destroyInstance(PIPE_CLASS_TAG); },
          mSensorIndex, PIPE_CLASS_TAG);
    }
    CAM_LOGD("mp3dnr->init!");
    if (pipeUsage.support3DNR()) {
      mp3dnr = NSCam::NSIoPipe::NSPostProc::hal3dnrBase::createInstance(
          MODULE_NAME, mSensorIndex);
      mp3dnr->init(pipeUsage.is3DNRModeMaskEnable(
          NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT));
    }
    mIsInited = MTRUE;
  }

  TRACE_FUNC_EXIT();
}

// This function shall be called only by ~P2CamContext().
// As nobody will know when the last instance will be released,
// all instances are managed by sp<>.
void P2CamContext::uninit() {
  TRACE_FUNC_ENTER();

  if (mIsInited) {
    if (mp3dnr != nullptr) {
      mp3dnr->uninit();
      mp3dnr = nullptr;
    }
    mIsInited = MFALSE;
  }

  TRACE_FUNC_EXIT();
}

std::shared_ptr<P2CamContext> getP2CamContext(MUINT32 sensorIndex) {
  return P2CamContext::getInstance(sensorIndex);
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
