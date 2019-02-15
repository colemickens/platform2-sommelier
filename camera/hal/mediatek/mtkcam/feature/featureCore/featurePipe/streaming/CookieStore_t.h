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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_COOKIESTORE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_COOKIESTORE_T_H_

#include <memory>
#include <mutex>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

typedef void* T_CookieStoreToken;

template <typename T_User>
class CookieStore {
 public:
  typedef CookieStore<T_User> T_Store;
  typedef typename T_User::T_Data T_Data;
  typedef typename T_User::T_Stream T_Stream;
  typedef typename T_User::T_Param T_Param;
  typedef typename T_User::T_Msg T_Msg;
  typedef typename T_User::T_Cookie T_Cookie;
  typedef T_CookieStoreToken T_Token;

  CookieStore();
  ~CookieStore();
  void enque(T_User* user,
             std::shared_ptr<T_Stream> stream,
             T_Param* pParam,
             const T_Data& data);
  void waitAllCallDone();
  static void staticProcessCB(const T_Msg& msg, T_Param* pParam, T_Token token);

 private:
  static T_Token store(T_Store* store,
                       T_User* user,
                       const T_Cookie& cookie,
                       const T_Data& data);
  static bool fetch(T_Token token,
                    T_Store** store,
                    T_User** user,
                    T_Cookie* cookie,
                    T_Data* data);
  static bool drop(T_Token token);

 private:
  void signalEnque();
  void signalDeque();

 private:
  std::mutex mMutex;
  std::condition_variable mCondition;
  unsigned mStoreCount;

 private:
  enum MAGIC {
    MAGIC_VALID = 0xabcd,
    MAGIC_USED = 0xdddd,
    MAGIC_FREED = 0xfaaf,
  };

  class BACKUP_DATA_TYPE {
   public:
    BACKUP_DATA_TYPE() : mStore(NULL), mUser(NULL), mMagic(MAGIC_VALID) {}

    BACKUP_DATA_TYPE(T_Store* store,
                     T_User* user,
                     const T_Cookie& cookie,
                     const T_Data& data)
        : mStore(store),
          mUser(user),
          mCookie(cookie),
          mData(data),
          mMagic(MAGIC_VALID) {}

    ~BACKUP_DATA_TYPE() { mMagic = MAGIC_FREED; }

    T_Store* mStore;
    T_User* mUser;
    T_Cookie mCookie;
    T_Data mData;
    MAGIC mMagic;
  };
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_COOKIESTORE_T_H_
