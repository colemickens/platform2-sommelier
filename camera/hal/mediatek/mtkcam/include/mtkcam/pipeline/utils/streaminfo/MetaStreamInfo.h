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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_METASTREAMINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_METASTREAMINFO_H_
//
#include <memory>
#include <string>
//
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include "BaseStreamInfoImp.h"
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * meta stream info builder.
 */
class VISIBILITY_PUBLIC MetaStreamInfoBuilder {
 public:
  virtual ~MetaStreamInfoBuilder() = default;
  virtual auto build() const -> std::shared_ptr<IMetaStreamInfo>;

 public:
  virtual auto setStreamName(std::string&& name) -> MetaStreamInfoBuilder& {
    mStreamName = name;
    return *this;
  }

  virtual auto setStreamId(StreamId_T streamId) -> MetaStreamInfoBuilder& {
    mStreamId = streamId;
    return *this;
  }

  virtual auto setStreamType(MUINT32 streamType) -> MetaStreamInfoBuilder& {
    mStreamType = streamType;
    return *this;
  }

  virtual auto setMaxBufNum(size_t maxBufNum) -> MetaStreamInfoBuilder& {
    mMaxBufNum = maxBufNum;
    return *this;
  }

  virtual auto setMinInitBufNum(size_t minInitBufNum)
      -> MetaStreamInfoBuilder& {
    mMinInitBufNum = minInitBufNum;
    return *this;
  }

 protected:  ////    Data Members.
  std::string mStreamName{"unknown"};
  StreamId_T mStreamId = -1L;
  MUINT32 mStreamType = 0;

  size_t mMaxBufNum = 0;
  size_t mMinInitBufNum = 0;
};

/**
 * metadata stream info.
 */
class VISIBILITY_PUBLIC MetaStreamInfo : public IMetaStreamInfo {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamInfo Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  char const* getStreamName() const override;

  StreamId_T getStreamId() const override;

  MUINT32 getStreamType() const override;

  size_t getMaxBufNum() const override;

  MVOID setMaxBufNum(size_t count) override;

  size_t getMinInitBufNum() const override;

  std::string toString() const override;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Instantiation.
  MetaStreamInfo(char const* streamName,
                 StreamId_T streamId,
                 MUINT32 streamType,
                 size_t maxBufNum,
                 size_t minInitBufNum = 0);
  ~MetaStreamInfo() {}

 protected:               ////                    Data Members.
  BaseStreamInfoImp mImp; /**< base implementator. */
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_METASTREAMINFO_H_
