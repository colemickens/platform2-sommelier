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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_NORMALSTREAMBASE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_NORMALSTREAMBASE_T_H_

#include "CookieStore.h"
#include <memory>
#include "MtkHeader.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
class NormalStreamBase {
 public:
  typedef T T_Data;
  typedef NormalStreamBase<T_Data> T_User;
  enum T_Msg { MSG_COOKIE_DONE, MSG_COOKIE_FAIL, MSG_COOKIE_BLOCK };
  typedef NSCam::v4l2::INormalStream T_Stream;
  typedef NSCam::NSIoPipe::QParams T_Param;
  typedef T_CookieStoreToken T_Token;
  typedef CookieStore<T_User> T_Store;

  class QParams_Cookie {
   public:
    QParams_Cookie();
    explicit QParams_Cookie(const T_Param& param);
    void replace(T_Param* pParam, T_Token token) const;
    void restore(T_Param* pParam) const;
    static T_Token getToken(T_Param* pParam);

   private:
    void* mCookie;
    T_Param::PFN_CALLBACK_T mCB;
    T_Param::PFN_CALLBACK_T mFailCB;
    T_Param::PFN_CALLBACK_T mBlockCB;
  };
  typedef QParams_Cookie T_Cookie;

 public:
  virtual ~NormalStreamBase() {}
  virtual void enqueNormalStreamBase(std::shared_ptr<T_Stream> stream,
                                     T_Param* pParam,
                                     const T_Data& data);
  void waitNormalStreamBaseDone();

 protected:
  virtual void onNormalStreamBaseCB(T_Param* pParam, const T_Data& data) = 0;
  virtual void onNormalStreamBaseFailCB(T_Param* pParam, const T_Data& data);
  virtual void onNormalStreamBaseBlockCB(T_Param* pParam, const T_Data& data);

 public:
  static void staticOnNormalStreamCB(T_Param* pParam);
  static void staticOnNormalStreamFailCB(T_Param* pParam);
  static void staticOnNormalStreamBlockCB(T_Param* pParam);

 private:
  virtual bool onCookieStoreEnque(std::shared_ptr<T_Stream> stream,
                                  T_Param* pParam);
  virtual void onCookieStoreCB(const T_Msg& msg,
                               T_Param* pParam,
                               const T_Data& data);

 private:
  friend T_Store;
  T_Store mCookieStore;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_NORMALSTREAMBASE_T_H_
