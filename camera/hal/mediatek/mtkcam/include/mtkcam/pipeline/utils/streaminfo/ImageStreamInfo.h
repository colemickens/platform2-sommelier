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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_IMAGESTREAMINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_IMAGESTREAMINFO_H_
//
#include <memory>
#include <string>
//
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include "BaseStreamInfoImp.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * image stream info builder.
 */
class VISIBILITY_PUBLIC ImageStreamInfoBuilder {
 public:
  virtual ~ImageStreamInfoBuilder() = default;
  virtual auto build() const -> std::shared_ptr<IImageStreamInfo>;

 public:
  virtual auto setStreamName(std::string&& name) -> ImageStreamInfoBuilder& {
    mStreamName = name;
    return *this;
  }

  virtual auto setStreamId(StreamId_T streamId) -> ImageStreamInfoBuilder& {
    mStreamId = streamId;
    return *this;
  }

  virtual auto setStreamType(MUINT32 streamType) -> ImageStreamInfoBuilder& {
    mStreamType = streamType;
    return *this;
  }

  virtual auto setMaxBufNum(size_t maxBufNum) -> ImageStreamInfoBuilder& {
    mMaxBufNum = maxBufNum;
    return *this;
  }

  virtual auto setMinInitBufNum(size_t minInitBufNum)
      -> ImageStreamInfoBuilder& {
    mMinInitBufNum = minInitBufNum;
    return *this;
  }

  virtual auto setUsageForAllocator(MUINT usage) -> ImageStreamInfoBuilder& {
    mUsageForAllocator = usage;
    return *this;
  }

  virtual auto setImgFormat(MINT format) -> ImageStreamInfoBuilder& {
    mImgFormat = format;
    return *this;
  }

  virtual auto setImgSize(MSize const& imgSize) -> ImageStreamInfoBuilder& {
    mImgSize = imgSize;
    return *this;
  }

  virtual auto setBufPlanes(IImageStreamInfo::BufPlanes_t&& bufPlanes)
      -> ImageStreamInfoBuilder& {
    mBufPlanes = bufPlanes;
    return *this;
  }

  virtual auto setTransform(MUINT32 transform) -> ImageStreamInfoBuilder& {
    mTransform = transform;
    return *this;
  }

  virtual auto setDataSpace(MUINT32 dataSpace) -> ImageStreamInfoBuilder& {
    mDataSpace = dataSpace;
    return *this;
  }

 protected:  ////    Data Members.
  std::string mStreamName{"unknown"};
  StreamId_T mStreamId = -1L;
  MUINT32 mStreamType = 0;

  size_t mMaxBufNum = 0;
  size_t mMinInitBufNum = 0;

  MUINT mUsageForAllocator = 0;
  MINT mImgFormat = 0;
  MSize mImgSize;
  IImageStreamInfo::BufPlanes_t mBufPlanes;

  MUINT32 mTransform = 0;
  MUINT32 mDataSpace = 0;
};

/**
 * image stream info.
 */
class VISIBILITY_PUBLIC ImageStreamInfo : public IImageStreamInfo {
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
  //  IImageStreamInfo Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  MUINT64 getUsageForConsumer() const override;

  MUINT64 getUsageForAllocator() const override;

  MINT getImgFormat() const override;

  MSize getImgSize() const override;

  BufPlanes_t const& getBufPlanes() const override;

  MUINT32 getTransform() const override;

  MERROR setTransform(MUINT32 transform) override;

  virtual MUINT32 getDataSpace() const;

  virtual MERROR updateStreamInfo(
      const std::shared_ptr<IImageStreamInfo>& pStreamInfo);

  MBOOL getSecureInfo() const override;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Instantiation.
  ImageStreamInfo(char const* streamName,
                  StreamId_T streamId,
                  MUINT32 streamType,
                  size_t maxBufNum,
                  size_t minInitBufNum,
                  MUINT usageForAllocator,
                  MINT imgFormat,
                  MSize const& imgSize,
                  BufPlanes_t const& bufPlanes,
                  MUINT32 transform = 0,
                  MUINT32 dataSpace = 0,
                  MBOOL secure = MFALSE);
  virtual ~ImageStreamInfo() {}

 protected:                 ////                    Data Members.
  BaseStreamInfoImp mImp;   /**< base implementator. */
  MUINT mUsageForAllocator; /**< usage for buffer allocator. */
  MINT mImgFormat;
  MSize mImgSize;
  BufPlanes_t mvbufPlanes;
  MUINT32 mTransform;
  MUINT32 mDataSpace;
  MBOOL mSecure;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMINFO_IMAGESTREAMINFO_H_
