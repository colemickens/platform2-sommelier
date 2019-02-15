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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWFRAMEREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWFRAMEREQUEST_H_

#include "P2_Param.h"
#include "P2_Request.h"
#include "P2_MWData.h"

#include <map>
#include <memory>
#include <vector>

namespace P2 {

class MWFrameRequest : public P2FrameRequest {
 public:
  MWFrameRequest(const ILog& log,
                 const P2Pack& pack,
                 const std::shared_ptr<P2DataObj>& p2Data,
                 const std::shared_ptr<MWInfo>& mwInfo,
                 const std::shared_ptr<MWFrame>& frame,
                 const std::shared_ptr<P2InIDMap>& p2IdMap);
  virtual ~MWFrameRequest();

  virtual MVOID beginBatchRelease();
  virtual MVOID endBatchRelease();
  virtual MVOID notifyNextCapture();
  virtual std::vector<std::shared_ptr<P2Request>> extractP2Requests();

 private:
  typedef std::map<StreamId_T, std::shared_ptr<P2Img>> P2ImgMap;
  typedef std::map<StreamId_T, std::shared_ptr<P2Meta>> P2MetaMap;

  MBOOL addP2Img(P2ImgMap* imgMap,
                 const StreamId_T& sID,
                 ID_IMG id,
                 IO_DIR dir,
                 const IMG_INFO& info);
  MBOOL addP2Img(P2ImgMap* imgMap, const StreamId_T& sID, IO_DIR dir);
  MBOOL addP2Img(P2ImgMap* imgMap, ID_IMG id, IO_DIR dir);
  P2ImgMap createP2ImgMap(IPipelineFrame::ImageInfoIOMapSet const& imgSet);
  MBOOL addP2Meta(P2MetaMap* metaMap,
                  const StreamId_T& sID,
                  ID_META id,
                  IO_DIR dir,
                  const META_INFO& info);
  MBOOL addP2Meta(P2MetaMap* metaMap, const StreamId_T& sID, IO_DIR dir);
  MBOOL addP2Meta(P2MetaMap* metaMap, ID_META id, IO_DIR dir);
  MBOOL removeP2Meta(P2MetaMap* metaMap, ID_META id);
  P2MetaMap createP2MetaMap(IPipelineFrame::MetaInfoIOMapSet const& metaSet);
  MVOID configBufferSize(MUINT32 sensorID,
                         const std::shared_ptr<Cropper>& cropper);
  std::shared_ptr<P2Meta> findP2Meta(const P2MetaMap& metaMap,
                                     ID_META id) const;
  std::shared_ptr<P2Meta> findP2Meta(ID_META id) const;
  std::shared_ptr<P2Img> findP2Img(const P2ImgMap& imgMap, ID_IMG id) const;
  std::shared_ptr<P2Img> findP2Img(ID_IMG id) const;
  MVOID initP2FrameData();
  MVOID updateP2FrameData();
  MVOID updateP2SensorData();
  MVOID updateP2Metadata(MUINT32 sensorID,
                         const std::shared_ptr<Cropper>& cropper);
  MBOOL fillP2Img(const std::shared_ptr<P2Request>& request,
                  const std::shared_ptr<P2Img>& img);
  MVOID fillP2Img(const std::shared_ptr<P2Request>& request,
                  const IPipelineFrame::ImageInfoIOMap& imgInfoMap,
                  const P2ImgMap& p2ImgMap);
  MBOOL fillP2Meta(const std::shared_ptr<P2Request>& request,
                   const std::shared_ptr<P2Meta>& meta);
  MVOID fillP2Meta(const std::shared_ptr<P2Request>& request,
                   const IPipelineFrame::MetaInfoIOMap& metaInfoMap,
                   const P2MetaMap& p2MetaMap);
  MVOID fillDefaultP2Meta(const std::shared_ptr<P2Request>& request,
                          const P2MetaMap& metaMap,
                          MUINT32 sensorID);
  MVOID printIOMap(const IPipelineFrame::InfoIOMapSet& ioMap);
  std::vector<std::shared_ptr<P2Request>> createRequests(
      IPipelineFrame::InfoIOMapSet const& ioMap);
  MVOID doRegisterPlugin();

 private:
  std::shared_ptr<P2DataObj> mP2Data;
  std::shared_ptr<MWInfo> mMWInfo;
  std::shared_ptr<MWFrame> mMWFrame;
  MBOOL mExtracted;
  P2MetaMap mMetaMap;
  P2ImgMap mImgMap;
  MUINT32 mImgCount;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWFRAMEREQUEST_H_
