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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_COOKIESTORE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_COOKIESTORE_H_

#include "CookieStore_t.h"

#include <featurePipe/common/include/PipeLogHeaderBegin.h>
#include "DebugControl.h"
#define PIPE_TRACE TRACE_COOKIE_STORE
#define PIPE_CLASS_TAG "CookieStore"
#include <featurePipe/common/include/PipeLog.h>
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T_User>
CookieStore<T_User>::CookieStore() : mStoreCount(0) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename T_User>
CookieStore<T_User>::~CookieStore() {
  TRACE_FUNC_ENTER();
  waitAllCallDone();
  TRACE_FUNC_EXIT();
}

template <typename T_User>
void CookieStore<T_User>::enque(T_User* user,
                                std::shared_ptr<T_Stream> stream,
                                T_Param* pParam,
                                const T_Data& data) {
  TRACE_FUNC_ENTER();
  T_Token token = NULL;
  T_Cookie cookie(*pParam);
  token = CookieStore<T_User>::store(this, user, cookie, data);
  cookie.replace(pParam, token);
  signalEnque();
  if (!user->onCookieStoreEnque(stream, pParam)) {
    MY_LOGE("onCookieEnque failed, token=%p", token);
    CookieStore<T_User>::drop(token);
    cookie.restore(pParam);
    user->onCookieStoreCB(T_User::MSG_COOKIE_FAIL, pParam, data);
    signalDeque();
  }
  TRACE_FUNC_EXIT();
}

template <typename T_User>
void CookieStore<T_User>::waitAllCallDone() {
  TRACE_FUNC_ENTER();
  std::unique_lock<std::mutex> lock(mMutex);
  while (mStoreCount) {
    TRACE_FUNC("wait with count=%d", mStoreCount);
    mCondition.wait(lock);
  }
  TRACE_FUNC_EXIT();
}

template <typename T_User>
void CookieStore<T_User>::staticProcessCB(const T_Msg& msg,
                                          T_Param* pParam,
                                          T_Token token) {
  TRACE_FUNC_ENTER();
  T_User* user = NULL;
  T_Store* store = NULL;
  T_Cookie cookie;
  T_Data data;

  if (!T_Store::fetch(token, &store, &user, &cookie, &data) || !user) {
    MY_LOGE("Invalid data from token=%p, store=%p, user=%p", token, store,
            user);
  } else {
    cookie.restore(pParam);
    user->onCookieStoreCB(msg, pParam, data);
    store->signalDeque();
  }
  TRACE_FUNC_EXIT();
}

template <typename T_User>
typename CookieStore<T_User>::T_Token CookieStore<T_User>::store(
    T_Store* store, T_User* user, const T_Cookie& cookie, const T_Data& data) {
  TRACE_FUNC_ENTER();
  BACKUP_DATA_TYPE* backup = new BACKUP_DATA_TYPE(store, user, cookie, data);
  if (!backup) {
    MY_LOGE("OOM: cannot allocate memory for backup data");
  }
  TRACE_FUNC_EXIT();
  return (T_Token)backup;
}

template <typename T_User>
bool CookieStore<T_User>::fetch(T_Token token,
                                T_Store** store,
                                T_User** user,
                                T_Cookie* cookie,
                                T_Data* data) {
  TRACE_FUNC_ENTER();
  bool ret = false;
  BACKUP_DATA_TYPE* backup = NULL;
  backup = reinterpret_cast<BACKUP_DATA_TYPE*>(token);
  TRACE_FUNC("get token=%p backup=%p", token, backup);
  if (backup) {
    if (backup->mMagic != MAGIC_VALID) {
      MY_LOGE("Backup data is corrupted: token=%p backup=%p magic=0x%x", token,
              backup, backup->mMagic);
    } else {
      backup->mMagic = MAGIC_USED;
      *store = backup->mStore;
      *user = backup->mUser;
      *cookie = backup->mCookie;
      *data = backup->mData;
      delete backup;
      backup = NULL;
      ret = true;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T_User>
bool CookieStore<T_User>::drop(T_Token token) {
  TRACE_FUNC_ENTER();
  bool ret = false;
  BACKUP_DATA_TYPE* backup = NULL;
  backup = reinterpret_cast<BACKUP_DATA_TYPE*>(token);
  TRACE_FUNC("get token=%p backup=%p", token, backup);
  if (backup) {
    if (backup->mMagic != MAGIC_VALID) {
      MY_LOGE("Backup data is corrupted: token=%p backup=%p magic=0x%x", token,
              backup, backup->mMagic);
    } else {
      backup->mMagic = MAGIC_USED;
      delete backup;
      backup = NULL;
      ret = true;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T_User>
void CookieStore<T_User>::signalEnque() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  ++mStoreCount;
  TRACE_FUNC_EXIT();
}

template <typename T_User>
void CookieStore<T_User>::signalDeque() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  --mStoreCount;
  mCondition.notify_all();
  TRACE_FUNC_EXIT();
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#include <featurePipe/common/include/PipeLogHeaderEnd.h>
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_COOKIESTORE_H_
