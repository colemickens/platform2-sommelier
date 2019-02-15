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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_EFFECTREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_EFFECTREQUEST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mtkcam/feature/effectHalBase/BasicParameters.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>

namespace NSCam {

typedef int64_t frame_no;

class VISIBILITY_PUBLIC EffectFrameInfo {
 public:
  typedef MVOID (*PFN_CALLBACK_T)(
      MVOID* tag, const std::shared_ptr<EffectFrameInfo>& frame);

  EffectFrameInfo(MUINT32 _reqNo = 0,
                  MUINT32 _frameNo = 0,
                  PFN_CALLBACK_T _cb = NULL,
                  MVOID* _tag = NULL)
      : mpOnFrameProcessed(_cb),
        mpTag(_tag),
        mFrameNo(_frameNo),
        mRequestNo(_reqNo),
        mIsFrameReady(0),
        mFrame(NULL),
        mpFrameParameter(NULL),
        mpFrameResult(NULL)

  {}

  ///< copy constructor
  EffectFrameInfo(const EffectFrameInfo& other);

  ///< operator constructor
  EffectFrameInfo& operator=(const EffectFrameInfo& other);

  bool isFrameBufferReady();
  status_t getFrameBuffer(std::shared_ptr<IImageBuffer>* frame);
  std::shared_ptr<EffectParameter> getFrameParameter();
  std::shared_ptr<EffectResult> getFrameResult();

  status_t setFrameBuffer(std::shared_ptr<IImageBuffer> frame);
  status_t setFrameParameter(std::shared_ptr<EffectParameter> result);
  status_t setFrameResult(std::shared_ptr<EffectResult> result);
  status_t setFrameReady(bool ready);
  status_t setFrameNo(frame_no num);

  /**
   *  @brief          get Frame number of this EffectFrameInfo object
   *  @return         Frame number
   */
  frame_no getFrameNo() const { return mFrameNo; }
  /**
   *  @brief          get Request number of this EffectFrameInfo object of
   * EffectRequest object
   *  @return         Request number
   */
  MUINT32 getRequestNo() const { return mRequestNo; }

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Variables.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  // e.g. mpOnRequestProcessed(mpTag, this);
  PFN_CALLBACK_T mpOnFrameProcessed;
  // callback tag; it shouldn't be modified by the client(user).
  // e.g. the tag may be a pointer to client(this).
  MVOID* mpTag;

 private:
  frame_no mFrameNo;
  MUINT32 mRequestNo;
  bool mIsFrameReady;
  std::shared_ptr<IImageBuffer> mFrame;

  std::shared_ptr<EffectParameter> mpFrameParameter;
  std::shared_ptr<EffectResult> mpFrameResult;
};

/**
 *  @brief EffectRequest is the basic class to passing data from client to
 * effectHal. The client can be a Client or a Pipeline Node.
 *
 */
class EffectRequest {
 public:  // EffectRequest
  typedef MVOID (*PFN_CALLBACK_T)(
      MVOID* tag,
      std::string status,
      const std::shared_ptr<EffectRequest>& request);  // completed, failed
  EffectRequest(MUINT32 _reqNo = 0,
                PFN_CALLBACK_T _cb = NULL,
                MVOID* _tag = NULL)
      : mpOnRequestProcessed(_cb),
        mpTag(_tag),
        mRequestNo(_reqNo),
        mpRequestParameter(nullptr),
        mpRequestResult(nullptr)

  {}

  ///< copy constructor
  EffectRequest(const EffectRequest& other);

  ///< operator constructor
  EffectRequest& operator=(const EffectRequest& other);

  /**
   *  @brief          get Request number of this EffectRequest object
   *  @return         request number
   */
  MUINT32 getRequestNo() const { return mRequestNo; }

  /**
   *  @brief          get the pointer of EffectParameter object that keep all
   * input paramters related to this EffectRequest
   *  @param[in]      parameter pointer to the EffectParameter object
   *  @return         std::shared_ptr<EffectParameter>
   */
  const std::shared_ptr<EffectParameter> getRequestParameter();
  /**
   *  @brief          get the pointer of EffectResult object that keep all
   * output results related to this EffectRequest
   *  @param[in]      result pointer to the EffectResult object
   *  @return         status_t
   */
  const std::shared_ptr<EffectResult> getRequestResult();
  /**
   *  @brief          set the pointer of EffectParameter object that keep all
   * input results related to this EffectRequest
   *  @param[in]      result pointer to the EffectResult object
   *  @return         status_t
   */
  status_t setRequestParameter(std::shared_ptr<EffectParameter> parameter);
  /**
   *  @brief          set the pointer of EffectResult object that keep all
   * output results related to this EffectRequest
   *  @param[in]      result pointer to the EffectResult object
   *  @return         status_t
   */
  status_t setRequestResult(std::shared_ptr<EffectResult> result);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Variables.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  std::map<frame_no, std::shared_ptr<EffectFrameInfo> > vInputFrameInfo;
  std::map<frame_no, std::shared_ptr<EffectFrameInfo> > vOutputFrameInfo;

  // e.g. mpOnRequestProcessed(mpTag, "completed", this);
  PFN_CALLBACK_T mpOnRequestProcessed;
  // callback tag; it shouldn't be modified by the client(user).
  // e.g. the tag may be a pointer to clinet(this of node or effectHalClinet).
  MVOID* mpTag;

 private:
  MUINT32 mRequestNo;
  std::shared_ptr<EffectParameter> mpRequestParameter;
  std::shared_ptr<EffectResult> mpRequestResult;
};
}  // end namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EFFECTHALBASE_EFFECTREQUEST_H_
