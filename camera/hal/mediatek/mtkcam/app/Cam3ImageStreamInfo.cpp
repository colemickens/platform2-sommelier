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

#undef LOG_TAG
#define LOG_TAG "MtkCam/StreamInfo"
//
#include "MyUtils.h"
#include "camera/hal/mediatek/mtkcam/app/Cam3ImageStreamInfo.h"

#include <memory>
#include <string>

//
#include <mtkcam/utils/gralloc/IGrallocHelper.h>
// using namespace NSCam;
// using namespace NSCam::v3;

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Cam3ImageStreamInfo>
NSCam::v3::Cam3ImageStreamInfo::cast(camera3_stream* stream) {
  MY_LOGW_IF(!stream || !stream->priv, "camera3_stream stream:%p", stream);
  std::shared_ptr<Cam3ImageStreamInfo> pStreamInfo;
  pStreamInfo.reset(
      static_cast<ThisT*>(stream->priv),
      [](Cam3ImageStreamInfo* p) { MY_LOGI("release implement"); });
  return pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Cam3ImageStreamInfo const>
NSCam::v3::Cam3ImageStreamInfo::cast(camera3_stream const* stream) {
  MY_LOGW_IF(!stream || !stream->priv, "camera3_stream:%p", stream);
  std::shared_ptr<Cam3ImageStreamInfo const> pStreamInfo;
  pStreamInfo.reset(
      static_cast<ThisT*>(stream->priv),
      [](Cam3ImageStreamInfo const* p) { MY_LOGI("release implement"); });
  return pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Cam3ImageStreamInfo::Cam3ImageStreamInfo(
    camera3_stream* pCamera3Stream,
    char const* streamName,
    StreamId_T streamId,
    MUINT usageForConsumer,
    MUINT usageForAllocator,
    MINT imgFormatToAlloc,
    MINT imgFormatInFact,
    BufPlanes_t const& bufPlanes,
    size_t maxBufNum,
    MUINT32 transform,
    MUINT32 dataSpace)
    : IImageStreamInfo(),
      mpCamera3Stream(pCamera3Stream),
      mStreamName(streamName),
      mStreamId(streamId),
      mStreamType(pCamera3Stream->stream_type),
      mUsageForConsumer(usageForConsumer),
      mImgFormat(imgFormatInFact),
      mImgFormatToAlloc(imgFormatToAlloc),
      mImgSize(pCamera3Stream->width, pCamera3Stream->height),
      mvbufPlanes(bufPlanes),
      mTransform(transform),
      mDataSpace(dataSpace) {
  switch (pCamera3Stream->stream_type) {
    case CAMERA3_STREAM_OUTPUT:
      mStreamType = eSTREAMTYPE_IMAGE_OUT;
      break;
    case CAMERA3_STREAM_INPUT:
      mStreamType = eSTREAMTYPE_IMAGE_IN;
      break;
    case CAMERA3_STREAM_BIDIRECTIONAL:
      mStreamType = eSTREAMTYPE_IMAGE_INOUT;
      break;
    default:
      break;
  }
  //
  //
  pCamera3Stream->priv = this;
  pCamera3Stream->max_buffers = maxBufNum;
  pCamera3Stream->usage = usageForAllocator;
  MY_LOGV("camera3_stream:%p this:%p", pCamera3Stream, this);
}

/******************************************************************************
 *
 ******************************************************************************/
camera3_stream* NSCam::v3::Cam3ImageStreamInfo::get_camera3_stream() const {
  return mpCamera3Stream;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Cam3ImageStreamInfo::getImgFormatToAlloc() const {
  return mImgFormatToAlloc;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Cam3ImageStreamInfo::getUsageForConsumer() const {
  return (MUINT64)(mUsageForConsumer);
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Cam3ImageStreamInfo::getUsageForAllocator() const {
  return (MUINT64)(mpCamera3Stream->usage);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Cam3ImageStreamInfo::getImgFormat() const {
  return mImgFormat;
}

/******************************************************************************
 *
 ******************************************************************************/
MSize NSCam::v3::Cam3ImageStreamInfo::getImgSize() const {
  return mImgSize;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Cam3ImageStreamInfo::BufPlanes_t const&
NSCam::v3::Cam3ImageStreamInfo::getBufPlanes() const {
  return mvbufPlanes;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Cam3ImageStreamInfo::getTransform() const {
  return mTransform;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Cam3ImageStreamInfo::setTransform(MUINT32 transform) {
  mTransform = transform;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Cam3ImageStreamInfo::getDataSpace() const {
  return mDataSpace;
}

/******************************************************************************
 *
 ******************************************************************************/
char const* NSCam::v3::Cam3ImageStreamInfo::getStreamName() const {
  return mStreamName.c_str();
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::StreamId_T NSCam::v3::Cam3ImageStreamInfo::getStreamId() const {
  return mStreamId;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Cam3ImageStreamInfo::getStreamType() const {
  return mStreamType;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Cam3ImageStreamInfo::getMaxBufNum() const {
  return mpCamera3Stream->max_buffers;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Cam3ImageStreamInfo::setMaxBufNum(size_t count) {
  mpCamera3Stream->max_buffers = count;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Cam3ImageStreamInfo::getMinInitBufNum() const {
  return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::Cam3ImageStreamInfo::toString() const {
  std::string s8Planes;
  for (size_t i = 0; i < mvbufPlanes.size(); i++) {
    auto const& plane = mvbufPlanes[i];
    s8Planes += base::StringPrintf(" %zu/%zu", plane.rowStrideInBytes,
                                   plane.sizeInBytes);
  }

  std::string s8RealFormat;
  std::string s8HalUsage;
  std::string s8Dataspace;
  if (auto pGrallocHelper = IGrallocHelper::singleton()) {
    s8RealFormat = pGrallocHelper->queryPixelFormatName(mImgFormat);
    s8HalUsage = pGrallocHelper->queryGrallocUsageName(mUsageForConsumer);
    s8Dataspace = pGrallocHelper->queryDataspaceName(mDataSpace);
  }

  char str_reqNum[255];
  snprintf(str_reqNum, sizeof(str_reqNum), "%02" PRIu64 " %4dx%-4d", mStreamId,
           mImgSize.w, mImgSize.h);
  std::string os(str_reqNum);

  return os;
}
