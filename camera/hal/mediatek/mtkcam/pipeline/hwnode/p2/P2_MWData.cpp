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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_MWData.h"
#include <mtkcam/utils/debug/P2_DebugControl.h>

#include <list>
#include <memory>
#include <string.h>
#include <vector>

namespace P2 {

#undef P2_CLASS_TAG
#undef P2_TRACE
#define P2_CLASS_TAG MWInfo
#define P2_TRACE TRACE_MW_INFO
#include "P2_LogHeader.h"

MWInfo::MWInfo(const NSCam::v3::P2StreamingNode::ConfigParams& param) {
  TRACE_FUNC_ENTER();
  initMetaInfo(IN_APP, param.pInAppMeta);
  initMetaInfo(IN_P1_APP, param.pInAppRetMeta);
  initMetaInfo(IN_P1_HAL, param.pInHalMeta);
  initMetaInfo(IN_P1_APP_2, param.pInAppRetMeta_Sub);
  initMetaInfo(IN_P1_HAL_2, param.pInHalMeta_Sub);
  initMetaInfo(OUT_APP, param.pOutAppMeta);
  initMetaInfo(OUT_HAL, param.pOutHalMeta);
  initImgInfo(IN_OPAQUE, param.pvInOpaque);
  initImgInfo(IN_FULL, param.pvInFullRaw);
  initImgInfo(IN_RESIZED, param.pInResizedRaw);
  initImgInfo(IN_LCSO, param.pInLcsoRaw);
  initImgInfo(IN_RSSO, param.pInRssoRaw);
  initImgInfo(IN_OPAQUE_2, param.pvInOpaque_Sub);
  initImgInfo(IN_FULL_2, param.pvInFullRaw_Sub);
  initImgInfo(IN_RESIZED_2, param.pInResizedRaw_Sub);
  initImgInfo(IN_LCSO_2, param.pInLcsoRaw_Sub);
  initImgInfo(IN_RSSO_2, param.pInRssoRaw_Sub);
  initImgInfo(IN_REPROCESS, param.pInYuvImage);
  initImgInfo(OUT_YUV, param.vOutImage);
  initImgInfo(OUT_FD, param.pOutFDImage);
  mBurstNum = param.burstNum;
  mCustomOption = param.customOption;
  TRACE_FUNC_EXIT();
}

MWInfo::MWInfo(const NSCam::v3::P2CaptureNode::ConfigParams& param) {
  TRACE_FUNC_ENTER();
  initMetaInfo(IN_APP, param.pInAppMeta);
  initMetaInfo(IN_P1_APP, param.pInAppRetMeta);
  initMetaInfo(IN_P1_HAL, param.pInHalMeta);
  initMetaInfo(IN_P1_APP_2, param.pInAppRetMeta2);
  initMetaInfo(IN_P1_HAL_2, param.pInHalMeta2);
  initMetaInfo(OUT_APP, param.pOutAppMeta);
  initMetaInfo(OUT_HAL, param.pOutHalMeta);
  initImgInfo(IN_OPAQUE, param.vpInOpaqueRaws);
  initImgInfo(IN_FULL, param.pInFullRaw);
  initImgInfo(IN_RESIZED, param.pInResizedRaw);
  initImgInfo(IN_LCSO, param.pInLcsoRaw);
  initImgInfo(IN_FULL_2, param.pInFullRaw2);
  initImgInfo(IN_RESIZED_2, param.pInResizedRaw2);
  initImgInfo(IN_LCSO_2, param.pInLcsoRaw2);
  initImgInfo(IN_REPROCESS, param.pInFullYuv);
  initImgInfo(OUT_YUV, param.vpOutImages);
  initImgInfo(OUT_JPEG_YUV, param.pOutJpegYuv);
  initImgInfo(OUT_THN_YUV, param.pOutThumbnailYuv);
  initImgInfo(OUT_POSTVIEW, param.pOutPostViewYuv);
  mCustomOption = param.uCustomOption;
  TRACE_FUNC_EXIT();
}
MWInfo::~MWInfo() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL MWInfo::isValid(const ILog& log) const {
  TRACE_S_FUNC_ENTER(log);
  MBOOL ret = MTRUE;
  MBOOL hasFull = hasImg(IN_FULL);
  MBOOL hasResized = hasImg(IN_RESIZED);
  MBOOL hasFD = hasImg(OUT_FD);
  MBOOL hasYUV = hasImg(OUT_YUV) || hasImg(OUT_JPEG_YUV);
  MBOOL hasInApp = hasMeta(IN_APP);
  MBOOL hasInHal = hasMeta(IN_P1_HAL);
  if (!(hasFull || hasResized) || !(hasFD || hasYUV) || !hasInApp ||
      !hasInHal) {
    MY_S_LOGW(log,
              "missing necessary stream: full(%d) resized(%d) fd(%d) yuv(%d) "
              "inApp(%d) inHal(%d)",
              hasFull, hasResized, hasFD, hasYUV, hasInApp, hasInHal);
    ret = MFALSE;
  }
  if (!ret || log.getLogLevel() || true) {
    this->print(log);
  }
  TRACE_S_FUNC_EXIT(log);
  return ret;
}

std::shared_ptr<IMetaStreamInfo> MWInfo::findMetaInfo(ID_META id) const {
  TRACE_FUNC_ENTER();
  std::shared_ptr<IMetaStreamInfo> info;
  auto it = mMetaInfoMap.find(id);
  if (it != mMetaInfoMap.end() && it->second.size()) {
    info = it->second[0];
  }
  TRACE_FUNC_EXIT();
  return info;
}

std::shared_ptr<IImageStreamInfo> MWInfo::findImgInfo(ID_IMG id) const {
  TRACE_FUNC_ENTER();
  std::shared_ptr<IImageStreamInfo> info;
  auto it = mImgInfoMap.find(id);
  if (it != mImgInfoMap.end() && it->second.size()) {
    info = it->second[0];
  }
  TRACE_FUNC_EXIT();
  return info;
}

ID_META MWInfo::toMetaID(StreamId_T sID) const {
  ID_META id = ID_META_INVALID;
  auto it = mMetaIDMap.find(sID);
  if (it != mMetaIDMap.end()) {
    id = it->second;
  }
  return id;
}

ID_IMG MWInfo::toImgID(StreamId_T sID) const {
  ID_IMG id = ID_IMG_INVALID;
  auto it = mImgIDMap.find(sID);
  if (it != mImgIDMap.end()) {
    id = it->second;
  }
  return id;
}

IMG_TYPE MWInfo::getImgType(StreamId_T sID) const {
  IMG_TYPE type = IMG_TYPE_EXTRA;
  auto it = mSIDTypeMap.find(sID);
  if (it != mSIDTypeMap.end()) {
    type = it->second;
  }
  return type;
}

MBOOL MWInfo::isCaptureIn(StreamId_T sID) const {
  MBOOL ret = MFALSE;
  auto it = mImgIDMap.find(sID);
  if (it != mImgIDMap.end()) {
    ret = (it->second == IN_OPAQUE) || (it->second == IN_FULL) ||
          (it->second == IN_REPROCESS);
  }
  return ret;
}

MUINT32 MWInfo::getBurstNum() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mBurstNum;
}

MUINT32 MWInfo::getCustomOption() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mCustomOption;
}

MBOOL MWInfo::supportClearZoom() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mCustomOption;
}

MBOOL MWInfo::supportDRE() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mCustomOption;
}

MVOID MWInfo::print(const ILog& log) const {
  TRACE_S_FUNC_ENTER(log);
  for (auto& info : P2Img::InfoMap) {
    std::vector<std::shared_ptr<IImageStreamInfo>> mwInfo =
        findStreamInfo(info.first);
    for (unsigned i = 0, size = mwInfo.size(); i < size; ++i) {
      std::shared_ptr<IImageStreamInfo> img = mwInfo[i];
      const char* name = "NA";
      StreamId_T id = 0;
      MUINT64 allocator = 0, consumer = 0;
      MINT imgFmt = 0;
      MSize imgSize;
      IMG_TYPE type = IMG_TYPE_EXTRA;
      if (img != nullptr) {
        id = img->getStreamId();
        allocator = img->getUsageForAllocator();
        consumer = img->getUsageForConsumer();
        imgFmt = img->getImgFormat();
        imgSize = img->getImgSize();
        if (mSIDTypeMap.count(id) != 0) {
          type = mSIDTypeMap.at(id);
        }
      }
      MY_S_LOGD(log,
                "StreamInfo: [img:0x%09" PRIx64 "] (A/C:0x%09" PRIx64
                "/0x%09" PRIx64 ") %s[%d]/%s (%dx%d) (fmt:0x%08x) type(%s)",
                id, allocator, consumer, info.second.name.c_str(), i, name,
                imgSize.w, imgSize.h, imgFmt, ImgType2Name(type));
    }
  }
  for (auto& info : P2Meta::InfoMap) {
    std::vector<std::shared_ptr<IMetaStreamInfo>> mwInfo =
        findStreamInfo(info.first);
    for (unsigned i = 0, size = mwInfo.size(); i < size; ++i) {
      std::shared_ptr<IMetaStreamInfo> meta = mwInfo[i];
      const char* name = "NA";
      StreamId_T id = 0;
      if (meta != nullptr) {
        name = meta->getStreamName();
        id = meta->getStreamId();
      }
      MY_S_LOGD(log, "StreamInfo: [meta:0x%09" PRIx64 "] %s[%d]/%s", id,
                info.second.name.c_str(), i, name);
    }
  }
  TRACE_S_FUNC_EXIT(log);
}

MVOID MWInfo::initMetaInfo(ID_META id,
                           const std::shared_ptr<IMetaStreamInfo>& info) {
  TRACE_FUNC_ENTER();
  if (info != nullptr) {
    mMetaInfoMap[id].push_back(info);
    mMetaIDMap[info->getStreamId()] = id;
  }
  TRACE_FUNC_EXIT();
}

MVOID MWInfo::initMetaInfo(
    ID_META id, const std::vector<std::shared_ptr<IMetaStreamInfo>>& infos) {
  TRACE_FUNC_ENTER();
  for (auto&& info : infos) {
    if (info != nullptr) {
      mMetaInfoMap[id].push_back(info);
      mMetaIDMap[info->getStreamId()] = id;
    }
  }
  TRACE_FUNC_EXIT();
}

MVOID MWInfo::initImgInfo(ID_IMG id,
                          const std::shared_ptr<IImageStreamInfo>& info) {
  TRACE_FUNC_ENTER();
  if (info != nullptr) {
    mImgInfoMap[id].push_back(info);
    mImgIDMap[info->getStreamId()] = id;
    if (id == OUT_FD) {
      mSIDTypeMap[info->getStreamId()] = IMG_TYPE_FD;
    }
  }
  TRACE_FUNC_EXIT();
}

MVOID MWInfo::initImgInfo(
    ID_IMG id, const std::vector<std::shared_ptr<IImageStreamInfo>>& infos) {
  TRACE_FUNC_ENTER();
  std::list<StreamId_T> hwCompIds;
  std::list<StreamId_T> hwTextureIds;
  std::list<StreamId_T> hwEncodeIds;
  for (auto&& info : infos) {
    if (info != nullptr) {
      MUINT32 usage = info->getUsageForAllocator();
      StreamId_T sId = info->getStreamId();

      mImgInfoMap[id].push_back(info);
      mImgIDMap[info->getStreamId()] = id;
      // TODO(mtk): add physical stream consideration
      /*
      if(strlen(info->getPhysicalCameraId()) > 0)
      {
          mSIDTypeMap[sId] = IMG_TYPE_PHYSICAL;
      }
      */
      if (usage & GRALLOC_USAGE_HW_COMPOSER) {
        hwCompIds.push_back(sId);
      }
      if (usage & GRALLOC_USAGE_HW_TEXTURE) {
        hwTextureIds.push_back(sId);
      }
      if (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
        hwEncodeIds.push_back(sId);
      }
    }
  }

  auto getUnOccupiedID = [&](std::list<StreamId_T>& idList,
                             StreamId_T& out) -> bool {
    for (auto&& id : idList) {
      if (mSIDTypeMap.count(id) == 0) {
        out = id;
        return true;
      }
    }
    return false;
  };

  // Record -> Display -> Unknown
  StreamId_T sID = (StreamId_T)(-1);
  if (getUnOccupiedID(hwEncodeIds, sID)) {
    mSIDTypeMap[sID] = IMG_TYPE_RECORD;
  }

  if (getUnOccupiedID(hwCompIds, sID)) {
    mSIDTypeMap[sID] = IMG_TYPE_DISPLAY;
  } else if (getUnOccupiedID(hwTextureIds, sID)) {
    mSIDTypeMap[sID] = IMG_TYPE_DISPLAY;
  }

  TRACE_FUNC_EXIT();
}

std::vector<std::shared_ptr<IMetaStreamInfo>> MWInfo::findStreamInfo(
    ID_META id) const {
  auto it = mMetaInfoMap.find(id);
  return (it != mMetaInfoMap.end())
             ? it->second
             : std::vector<std::shared_ptr<IMetaStreamInfo>>();
}

std::vector<std::shared_ptr<IImageStreamInfo>> MWInfo::findStreamInfo(
    ID_IMG id) const {
  auto it = mImgInfoMap.find(id);
  return (it != mImgInfoMap.end())
             ? it->second
             : std::vector<std::shared_ptr<IImageStreamInfo>>();
}

MBOOL MWInfo::hasMeta(ID_META id) const {
  auto&& it = mMetaInfoMap.find(id);
  return it != mMetaInfoMap.end() && it->second.size();
}

MBOOL MWInfo::hasImg(ID_IMG id) const {
  auto&& it = mImgInfoMap.find(id);
  return it != mImgInfoMap.end() && it->second.size();
}

#undef P2_CLASS_TAG
#undef P2_TRACE
#define P2_CLASS_TAG MWMeta
#define P2_TRACE TRACE_MW_META
#include "P2_LogHeader.h"

MWMeta::MWMeta(const ILog& log,
               const P2Pack& p2Pack,
               const std::shared_ptr<MWFrame>& frame,
               const StreamId_T& streamID,
               IO_DIR dir,
               const META_INFO& info)
    : P2Meta(log, p2Pack, info.id),
      mMWFrame(frame),
      mStreamID(streamID),
      mDir(dir),
      mStatus(IO_STATUS_INVALID),
      mMetadata(nullptr),
      mLockedMetadata(nullptr),
      mMetadataCopy(nullptr) {
  TRACE_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "acquireMeta");
  mStreamBuffer = mMWFrame->acquireMetaStream(mStreamID);
  if (mStreamBuffer != nullptr) {
    mLockedMetadata = mMWFrame->acquireMeta(mStreamBuffer, mDir);
  }
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  if (mLockedMetadata != nullptr) {
    if (dir == IO_DIR_IN && (info.flag & IO_FLAG_COPY)) {
      mMetadataCopy = new IMetadata();
      *mMetadataCopy = *mLockedMetadata;
      mMetadata = mMetadataCopy;
      mStatus = IO_STATUS_READY;
      TRACE_S_FUNC(mLog, "meta=%p count= %d", mMetadata, mMetadata->count());
    } else {
      mMetadata = mLockedMetadata;
    }
    mStatus = IO_STATUS_READY;
    TRACE_S_FUNC(mLog, "meta=%p count= %d", mMetadata, mMetadata->count());
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MWMeta::~MWMeta() {
  TRACE_S_FUNC_ENTER(
      mLog, "name(%s), count(%d)",
      (mStreamBuffer != nullptr) ? mStreamBuffer->getName() : "??",
      (mLockedMetadata != nullptr) ? mLockedMetadata->count() : -1);

  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "~MWMeta->releaseMeta");
  mMWFrame->releaseMeta(mStreamBuffer, mLockedMetadata);
  mMetadata = mLockedMetadata = nullptr;
  mMWFrame->releaseMetaStream(mStreamBuffer, mDir, mStatus);
  mStreamBuffer = nullptr;
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  mMWFrame->notifyRelease();
  mMWFrame = nullptr;

  if (mMetadataCopy != nullptr) {
    P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "~MWMetag->freeCopyMeta");
    delete mMetadataCopy;
    mMetadataCopy = nullptr;
    P2_CAM_TRACE_END(TRACE_ADVANCED);
  }
  TRACE_S_FUNC_EXIT(mLog);
}

StreamId_T MWMeta::getStreamID() const {
  return mStreamID;
}

MBOOL MWMeta::isValid() const {
  return (mMetadata != nullptr);
}

IO_DIR MWMeta::getDir() const {
  return mDir;
}

MVOID MWMeta::updateResult(MBOOL result) {
  TRACE_S_FUNC_ENTER(mLog);
  if ((mDir & IO_DIR_OUT) && mStatus != IO_STATUS_INVALID) {
    mStatus = result ? IO_STATUS_OK : IO_STATUS_ERROR;
  }
  TRACE_S_FUNC_EXIT(mLog);
}

IMetadata* MWMeta::getIMetadataPtr() const {
  return mMetadata;
}

IMetadata::IEntry MWMeta::getEntry(MUINT32 tag) const {
  IMetadata::IEntry entry;
  if (mMetadata) {
    entry = mMetadata->entryFor(tag);
  }
  return entry;
}

MBOOL MWMeta::setEntry(MUINT32 tag, const IMetadata::IEntry& entry) {
  MBOOL ret = MFALSE;
  if (mMetadata) {
    ret = (mMetadata->update(tag, entry) == OK);
  }
  return ret;
}

#undef P2_CLASS_TAG
#undef P2_TRACE
#define P2_CLASS_TAG MWImg
#define P2_TRACE TRACE_MW_IMG
#include "P2_LogHeader.h"

MWImg::MWImg(const ILog& log,
             const P2Pack& p2Pack,
             const std::shared_ptr<MWFrame>& frame,
             const StreamId_T& streamID,
             IO_DIR dir,
             IMG_TYPE type,
             const IMG_INFO& info,
             MUINT32 debugIndex,
             MBOOL needSWRW)
    : P2Img(log, p2Pack, info.id, debugIndex),
      mMWFrame(frame),
      mStreamID(streamID),
      mDir(dir),
      mStatus(IO_STATUS_INVALID),
      mTransform(0),
      mUsage(0),
      mType(type) {
  TRACE_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "acquireImage");
  mStreamBuffer = mMWFrame->acquireImageStream(mStreamID);
  if (mStreamBuffer != nullptr) {
    if (info.id == IN_OPAQUE) {
      mImageBuffer = mMWFrame->acquireOpaqueImage(mStreamBuffer, dir);
    } else {
      mImageBuffer = mMWFrame->acquireImage(mStreamBuffer, dir, needSWRW);
    }
  }
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  if (mImageBuffer != nullptr) {
    mTransform = mStreamBuffer->getStreamInfo()->getTransform();
    mUsage = mStreamBuffer->getStreamInfo()->getUsageForAllocator();
    mStatus = IO_STATUS_READY;
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MWImg::~MWImg() {
  TRACE_S_FUNC_ENTER(mLog);
  processPlugin();
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "~MWImg->releaseImage");
  mMWFrame->releaseImage(mStreamBuffer, mImageBuffer);
  mImageBuffer = nullptr;
  mMWFrame->releaseImageStream(mStreamBuffer, mDir, mStatus);
  mStreamBuffer = nullptr;
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  mMWFrame->notifyRelease();
  mMWFrame = nullptr;
  TRACE_S_FUNC_EXIT(mLog);
}

StreamId_T MWImg::getStreamID() const {
  return mStreamID;
}

MBOOL MWImg::isValid() const {
  return (mImageBuffer != nullptr);
}

IO_DIR MWImg::getDir() const {
  return mDir;
}

MVOID MWImg::registerPlugin(
    const std::list<std::shared_ptr<P2ImgPlugin>>& plugin) {
  TRACE_S_FUNC_ENTER(mLog);
  mPlugin = plugin;
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWImg::updateResult(MBOOL result) {
  TRACE_S_FUNC_ENTER(mLog);
  if ((mDir & IO_DIR_OUT) && mStatus != IO_STATUS_INVALID) {
    mStatus = result ? IO_STATUS_OK : IO_STATUS_ERROR;
  }
  TRACE_S_FUNC_EXIT(mLog);
}

IImageBuffer* MWImg::getIImageBufferPtr() const {
  return mImageBuffer.get();
}

MUINT32 MWImg::getTransform() const {
  return mTransform;
}

MUINT32 MWImg::getUsage() const {
  return mUsage;
}

MBOOL MWImg::isDisplay() const {
  return mType == IMG_TYPE_DISPLAY;
}

MBOOL MWImg::isRecord() const {
  return mType == IMG_TYPE_RECORD;
}

MBOOL MWImg::isPhysicalStream() const {
  return mType == IMG_TYPE_PHYSICAL;
}

MBOOL MWImg::isCapture() const {
  return !(mUsage &
           (GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_VIDEO_ENCODER));
}

MVOID MWImg::processPlugin() const {
  TRACE_S_FUNC_ENTER(mLog);
  if (mStatus != IO_STATUS_ERROR) {
    for (auto plugin : mPlugin) {
      plugin->onPlugin(this);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
}

}  // namespace P2
