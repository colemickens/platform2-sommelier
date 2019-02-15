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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_BASESTREAMINFOIMP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_BASESTREAMINFOIMP_H_
//
#include <mtkcam/def/common.h>
#include <string>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * Base Stream Info Implementation.
 */
class BaseStreamInfoImp {
 public:  ////                    Attributes.
  inline char const* getStreamName() const { return mStreamName.c_str(); }
  inline StreamId_T getStreamId() const { return mStreamId; }
  inline MUINT32 getStreamType() const { return mStreamType; }
  inline size_t getMaxBufNum() const { return mMaxBufNum; }
  inline size_t getMinInitBufNum() const { return mMinInitBufNum; }
  inline MVOID setMaxBufNum(size_t count) { mMaxBufNum = count; }
  virtual MERROR updateStreamInfo(const BaseStreamInfoImp& baseInfo) {
    mStreamName = baseInfo.getStreamName();
    mStreamId = baseInfo.getStreamId();
    mStreamType = baseInfo.getStreamType();
    mMaxBufNum = baseInfo.getMaxBufNum();
    mMinInitBufNum = baseInfo.getMinInitBufNum();
    return OK;
  }

 public:  ////                    Instantiation.
  BaseStreamInfoImp(char const* streamName,
                    StreamId_T streamId,
                    MUINT32 streamType,
                    size_t maxBufNum,
                    size_t minInitBufNum)
      : mStreamName(streamName),
        mStreamId(streamId),
        mStreamType(streamType),
        mMaxBufNum(maxBufNum),
        mMinInitBufNum(minInitBufNum) {}

 protected:  ////                    Data Members.
  std::string mStreamName;
  StreamId_T mStreamId;
  MUINT32 mStreamType;
  size_t mMaxBufNum;
  size_t mMinInitBufNum;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_BASESTREAMINFOIMP_H_
