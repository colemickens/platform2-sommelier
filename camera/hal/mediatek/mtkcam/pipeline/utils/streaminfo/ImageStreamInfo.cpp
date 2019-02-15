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

#define LOG_TAG "MtkCam/StreamInfo"
//
#include <inttypes.h>
//
#include <cutils/compiler.h>
//
#include <memory>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <string>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Format.h>
//
// using namespace NSCam::v3::Utils;
/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::Utils::ImageStreamInfoBuilder::build() const
    -> std::shared_ptr<IImageStreamInfo> {
  std::shared_ptr<NSCam::v3::Utils::ImageStreamInfo> pStreamInfo =
      std::make_shared<NSCam::v3::Utils::ImageStreamInfo>(
          mStreamName.c_str(), mStreamId, mStreamType, mMaxBufNum,
          mMinInitBufNum, mUsageForAllocator, mImgFormat, mImgSize, mBufPlanes,
          mTransform, mDataSpace);

  if (CC_UNLIKELY(pStreamInfo == nullptr)) {
    MY_LOGE("Fail on build a new ImageStreamInfo: %#" PRIx64 "(%s)", mStreamId,
            mStreamName.c_str());
  }
  return pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::ImageStreamInfo::ImageStreamInfo(char const* streamName,
                                                   StreamId_T streamId,
                                                   MUINT32 streamType,
                                                   size_t maxBufNum,
                                                   size_t minInitBufNum,
                                                   MUINT usageForAllocator,
                                                   MINT imgFormat,
                                                   MSize const& imgSize,
                                                   BufPlanes_t const& bufPlanes,
                                                   MUINT32 transform,
                                                   MUINT32 dataSpace,
                                                   MBOOL secure)
    : IImageStreamInfo(),
      mImp(streamName, streamId, streamType, maxBufNum, minInitBufNum),
      mUsageForAllocator(usageForAllocator),
      mImgFormat(imgFormat),
      mImgSize(imgSize),
      mvbufPlanes(bufPlanes),
      mTransform(transform),
      mDataSpace(dataSpace),
      mSecure(secure) {}

/******************************************************************************
 *
 ******************************************************************************/
char const* NSCam::v3::Utils::ImageStreamInfo::getStreamName() const {
  return mImp.getStreamName();
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::IStreamInfo::StreamId_T
NSCam::v3::Utils::ImageStreamInfo::getStreamId() const {
  return mImp.getStreamId();
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::ImageStreamInfo::getStreamType() const {
  return mImp.getStreamType();
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::ImageStreamInfo::getMaxBufNum() const {
  return mImp.getMaxBufNum();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::ImageStreamInfo::setMaxBufNum(size_t count) {
  mImp.setMaxBufNum(count);
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::ImageStreamInfo::getMinInitBufNum() const {
  return mImp.getMinInitBufNum();
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Utils::ImageStreamInfo::getUsageForConsumer() const {
  return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Utils::ImageStreamInfo::getUsageForAllocator() const {
  return mUsageForAllocator;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::ImageStreamInfo::getImgFormat() const {
  return mImgFormat;
}

/******************************************************************************
 *
 ******************************************************************************/
MSize NSCam::v3::Utils::ImageStreamInfo::getImgSize() const {
  return mImgSize;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::IImageStreamInfo::BufPlanes_t const&
NSCam::v3::Utils::ImageStreamInfo::getBufPlanes() const {
  return mvbufPlanes;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::ImageStreamInfo::getTransform() const {
  return mTransform;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::ImageStreamInfo::setTransform(MUINT32 transform) {
  mTransform = transform;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::ImageStreamInfo::getDataSpace() const {
  return mDataSpace;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Utils::ImageStreamInfo::getSecureInfo() const {
  return mSecure;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::ImageStreamInfo::updateStreamInfo(
    const std::shared_ptr<IImageStreamInfo>& pStreamInfo) {
  BaseStreamInfoImp baseInfo(
      pStreamInfo->getStreamName(), pStreamInfo->getStreamId(),
      pStreamInfo->getStreamType(), pStreamInfo->getMaxBufNum(),
      pStreamInfo->getMinInitBufNum());
  mImp.updateStreamInfo(baseInfo);
  mUsageForAllocator = pStreamInfo->getUsageForAllocator();
  mImgFormat = pStreamInfo->getImgFormat();
  mImgSize = pStreamInfo->getImgSize();
  mvbufPlanes = pStreamInfo->getBufPlanes();
  mTransform = pStreamInfo->getTransform();

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::Utils::ImageStreamInfo::toString() const {
  std::string s8Planes;
  auto const& planes = getBufPlanes();
  for (size_t i = 0; i < planes.size(); i++) {
    auto const& plane = planes[i];
    s8Planes += base::StringPrintf(" %zu/%zu", plane.rowStrideInBytes,
                                   plane.sizeInBytes);
  }

  std::string os = base::StringPrintf(
      "%#" PRIx64
      " %4dx%-4d "
      "t:%d maxBufNum:%zu minInitBufNum:%zu "
      "format:%#x(%s) "
      "rowStrideInBytes/sizeInBytes:%s %s",
      getStreamId(), getImgSize().w, getImgSize().h, getTransform(),
      getMaxBufNum(), getMinInitBufNum(), getImgFormat(),
      NSCam::Utils::Format::queryImageFormatName(getImgFormat()),
      s8Planes.c_str(), getStreamName());
  return os;
}
