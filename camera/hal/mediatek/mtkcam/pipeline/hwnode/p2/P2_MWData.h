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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWDATA_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWDATA_H_

#include <mtkcam/pipeline/hwnode/P2CaptureNode.h>
#include <mtkcam/pipeline/hwnode/P2StreamingNode.h>
#include "P2_Param.h"
#include "P2_MWFrame.h"

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

namespace P2 {

class MWInfo {
 public:
  explicit MWInfo(const NSCam::v3::P2StreamingNode::ConfigParams& param);
  explicit MWInfo(const NSCam::v3::P2CaptureNode::ConfigParams& param);
  virtual ~MWInfo();
  MBOOL isValid(const ILog& log) const;
  std::shared_ptr<IMetaStreamInfo> findMetaInfo(ID_META id) const;
  std::shared_ptr<IImageStreamInfo> findImgInfo(ID_IMG id) const;
  ID_META toMetaID(StreamId_T sID) const;
  ID_IMG toImgID(StreamId_T sID) const;
  IMG_TYPE getImgType(StreamId_T sID) const;
  MBOOL isCaptureIn(StreamId_T sID) const;
  MUINT32 getBurstNum() const;
  MUINT32 getCustomOption() const;
  MBOOL supportClearZoom() const;
  MBOOL supportDRE() const;
  MVOID print(const ILog& log) const;

 private:
  std::unordered_map<ID_META, std::vector<std::shared_ptr<IMetaStreamInfo>>>
      mMetaInfoMap;
  std::unordered_map<ID_IMG, std::vector<std::shared_ptr<IImageStreamInfo>>>
      mImgInfoMap;
  std::unordered_map<StreamId_T, ID_META> mMetaIDMap;
  std::unordered_map<StreamId_T, ID_IMG> mImgIDMap;
  std::unordered_map<StreamId_T, IMG_TYPE> mSIDTypeMap;
  MUINT32 mBurstNum = 0;
  MUINT32 mCustomOption = 0;

 private:
  MVOID initMetaInfo(ID_META id, const std::shared_ptr<IMetaStreamInfo>& info);
  MVOID initMetaInfo(
      ID_META id, const std::vector<std::shared_ptr<IMetaStreamInfo>>& infos);
  MVOID initImgInfo(ID_IMG id, const std::shared_ptr<IImageStreamInfo>& info);
  MVOID initImgInfo(
      ID_IMG id, const std::vector<std::shared_ptr<IImageStreamInfo>>& infos);
  std::vector<std::shared_ptr<IMetaStreamInfo>> findStreamInfo(
      ID_META id) const;
  std::vector<std::shared_ptr<IImageStreamInfo>> findStreamInfo(
      ID_IMG id) const;
  MBOOL hasMeta(ID_META id) const;
  MBOOL hasImg(ID_IMG id) const;
};

class MWMeta : virtual public P2Meta {
 public:
  MWMeta(const ILog& log,
         const P2Pack& p2Pack,
         const std::shared_ptr<MWFrame>& frame,
         const StreamId_T& streamID,
         IO_DIR dir,
         const META_INFO& info);
  virtual ~MWMeta();
  virtual StreamId_T getStreamID() const;
  virtual MBOOL isValid() const;
  virtual IO_DIR getDir() const;
  virtual MVOID updateResult(MBOOL result);
  virtual IMetadata* getIMetadataPtr() const;
  virtual IMetadata::IEntry getEntry(MUINT32 tag) const;
  virtual MBOOL setEntry(MUINT32 tag, const IMetadata::IEntry& entry);

 private:
  std::shared_ptr<MWFrame> mMWFrame;
  StreamId_T mStreamID;
  IO_DIR mDir;
  IO_STATUS mStatus;
  std::shared_ptr<IMetaStreamBuffer> mStreamBuffer;
  IMetadata* mMetadata;
  IMetadata* mLockedMetadata;
  IMetadata* mMetadataCopy;
};

class MWImg : virtual public P2Img {
 public:
  MWImg(const ILog& log,
        const P2Pack& p2Pack,
        const std::shared_ptr<MWFrame>& frame,
        const StreamId_T& streamID,
        IO_DIR dir,
        IMG_TYPE type,
        const IMG_INFO& info,
        MUINT32 debugIndex,
        MBOOL needSWRW);
  virtual ~MWImg();
  virtual StreamId_T getStreamID() const;
  virtual MBOOL isValid() const;
  virtual IO_DIR getDir() const;
  MVOID registerPlugin(const std::list<std::shared_ptr<P2ImgPlugin>>& plugin);
  virtual MVOID updateResult(MBOOL result);
  virtual IImageBuffer* getIImageBufferPtr() const;
  virtual MUINT32 getTransform() const;
  virtual MUINT32 getUsage() const;
  virtual MBOOL isDisplay() const;
  virtual MBOOL isRecord() const;
  virtual MBOOL isCapture() const;
  virtual MBOOL isPhysicalStream() const;

 private:
  MVOID processPlugin() const;

 private:
  std::shared_ptr<MWFrame> mMWFrame;
  StreamId_T mStreamID;
  IO_DIR mDir;
  IO_STATUS mStatus;
  std::shared_ptr<IImageStreamBuffer> mStreamBuffer;
  std::shared_ptr<IImageBuffer> mImageBuffer;
  MUINT32 mTransform;
  MUINT32 mUsage;
  std::list<std::shared_ptr<P2ImgPlugin>> mPlugin;
  IMG_TYPE mType = IMG_TYPE_EXTRA;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWDATA_H_
