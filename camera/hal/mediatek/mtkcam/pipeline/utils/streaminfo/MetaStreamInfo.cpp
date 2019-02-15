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
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/utils/std/Log.h>
#include <string>
//
// using namespace NSCam::v3::Utils;

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::Utils::MetaStreamInfoBuilder::build() const
    -> std::shared_ptr<IMetaStreamInfo> {
  std::shared_ptr<IMetaStreamInfo> pStreamInfo =
      std::make_shared<MetaStreamInfo>(mStreamName.c_str(), mStreamId,
                                       mStreamType, mMaxBufNum, mMinInitBufNum);

  if (CC_UNLIKELY(pStreamInfo == nullptr)) {
    MY_LOGE("Fail on build a new MetaStreamInfo: %#" PRIx64 "(%s)", mStreamId,
            mStreamName.c_str());
  }
  return pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::MetaStreamInfo::MetaStreamInfo(char const* streamName,
                                                 StreamId_T streamId,
                                                 MUINT32 streamType,
                                                 size_t maxBufNum,
                                                 size_t minInitBufNum)
    : IMetaStreamInfo(),
      mImp(streamName, streamId, streamType, maxBufNum, minInitBufNum) {}

/******************************************************************************
 *
 ******************************************************************************/
char const* NSCam::v3::Utils::MetaStreamInfo::getStreamName() const {
  return mImp.getStreamName();
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::IStreamInfo::StreamId_T
NSCam::v3::Utils::MetaStreamInfo::getStreamId() const {
  return mImp.getStreamId();
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::MetaStreamInfo::getStreamType() const {
  return mImp.getStreamType();
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::MetaStreamInfo::getMaxBufNum() const {
  return mImp.getMaxBufNum();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::MetaStreamInfo::setMaxBufNum(size_t count) {
  mImp.setMaxBufNum(count);
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::MetaStreamInfo::getMinInitBufNum() const {
  return mImp.getMinInitBufNum();
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::Utils::MetaStreamInfo::toString() const {
  std::string os = base::StringPrintf("%#" PRIx64
                                      " "
                                      "maxBuffers:%zu minInitBufNum:%zu "
                                      "%s",
                                      getStreamId(), getMaxBufNum(),
                                      getMinInitBufNum(), getStreamName());
  return os;
}
