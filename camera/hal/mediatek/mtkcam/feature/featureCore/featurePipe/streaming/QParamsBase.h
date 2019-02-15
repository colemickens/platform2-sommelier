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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_QPARAMSBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_QPARAMSBASE_H_

#include "QParamsBase_t.h"

#include <featurePipe/common/include/PipeLogHeaderBegin.h>
#include "DebugControl.h"
#define PIPE_TRACE TRACE_QPARAMS_BASE
#define PIPE_CLASS_TAG "QParamsBase"
#include <featurePipe/common/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
QParamsBase<T>::QParamsBase() : mCount(0) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename T>
QParamsBase<T>::~QParamsBase() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename T>
MBOOL QParamsBase<T>::onQParamsFailCB(const NSCam::NSIoPipe::QParams& param,
                                      const T& data) {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = onQParamsCB(param, data);
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T>
MBOOL QParamsBase<T>::onQParamsBlockCB(const NSCam::NSIoPipe::QParams& param,
                                       const T& data) {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = onQParamsCB(param, data);
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T>
void QParamsBase<T>::processCB(NSCam::NSIoPipe::QParams param, CB_TYPE type) {
  TRACE_FUNC_ENTER();
  BACKUP_DATA_TYPE* backup = NULL;

  backup = checkOutBackup(param.mpCookie);
  if (!backup || !backup->mParent) {
    MY_LOGE("Cannot retrieve QParams data from backup=%p", backup);
  } else {
    backup->restore(param);
    switch (type) {
      case CB_DONE:
        backup->mParent->onQParamsCB(param, backup->mData);
        break;
      case CB_FAIL:
        param.mDequeSuccess = MFALSE;
        backup->mParent->onQParamsFailCB(param, backup->mData);
        break;
      case CB_BLOCK:
        param.mDequeSuccess = MFALSE;
        backup->mParent->onQParamsBlockCB(param, backup->mData);
        break;
      default:
        MY_LOGE("Unknown CB type = %d", type);
        break;
    }
    backup->mParent->signalDone();
  }
  if (backup) {
    delete backup;
    backup = NULL;
  }
  TRACE_FUNC_EXIT();
}

template <typename T>
void QParamsBase<T>::staticQParamsCB(NSCam::NSIoPipe::QParams* param) {
  TRACE_FUNC_ENTER();
  processCB(*param, CB_DONE);
  TRACE_FUNC_EXIT();
}

template <typename T>
void QParamsBase<T>::staticQParamsFailCB(NSCam::NSIoPipe::QParams* param) {
  TRACE_FUNC_ENTER();
  processCB(*param, CB_FAIL);
  TRACE_FUNC_EXIT();
}

template <typename T>
void QParamsBase<T>::staticQParamsBlockCB(NSCam::NSIoPipe::QParams* param) {
  TRACE_FUNC_ENTER();
  processCB(*param, CB_BLOCK);
  TRACE_FUNC_EXIT();
}

template <typename T>
MBOOL QParamsBase<T>::enqueQParams(
    NSCam::NSIoPipe::NSPostProc::INormalStream* stream,
    NSCam::NSIoPipe::QParams param,
    const T& data) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  BACKUP_DATA_TYPE* backup = NULL;
  if (stream == NULL) {
    MY_LOGE("Invalid stream: NULL");
    ret = MFALSE;
  } else if ((backup = new BACKUP_DATA_TYPE(this, param, data)) == NULL) {
    MY_LOGE("OOM: cannot allocate memory for backup data");
    param.mDequeSuccess = MFALSE;
    this->onQParamsFailCB(param, data);
  } else {
    param.mpCookie = reinterpret_cast<void*>(backup);
    param.mpfnCallback = QParamsBase<T>::staticQParamsCB;
    param.mpfnEnQFailCallback = QParamsBase<T>::staticQParamsFailCB;
    param.mpfnEnQBlockCallback = QParamsBase<T>::staticQParamsBlockCB;
    signalEnque();
    if (!stream->enque(param)) {
      MY_LOGE("normal stream enque failed, backup=%p", backup);
      staticQParamsFailCB(param);
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T>
MVOID QParamsBase<T>::waitEnqueQParamsDone() {
  TRACE_FUNC_ENTER();
  std::unique_lock<std::mutex> lock(mMutex);
  while (mCount) {
    mCondition.wait(mMutex);
  }
  TRACE_FUNC_EXIT();
}

template <typename T>
MVOID QParamsBase<T>::signalDone() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  --mCount;
  mCondition.broadcast();
  TRACE_FUNC_EXIT();
}

template <typename T>
MVOID QParamsBase<T>::signalEnque() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  ++mCount;
  TRACE_FUNC_EXIT();
}

template <typename T>
typename QParamsBase<T>::BACKUP_DATA_TYPE* QParamsBase<T>::checkOutBackup(
    MVOID* handle) {
  TRACE_FUNC_ENTER();
  BACKUP_DATA_TYPE* backup;
  backup = reinterpret_cast<BACKUP_DATA_TYPE*>(handle);
  if (backup) {
    if (backup->mMagic != MAGIC_VALID) {
      MY_LOGE("Backup data is corrupted: backup=%p magic=0x%x", backup,
              backup->mMagic);
    } else {
      backup->mMagic = MAGIC_USED;
    }
  }
  TRACE_FUNC("get backup=%p", backup);
  TRACE_FUNC_EXIT();
  return backup;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#include <featurePipe/common/include/PipeLogHeaderEnd.h>
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_QPARAMSBASE_H_
