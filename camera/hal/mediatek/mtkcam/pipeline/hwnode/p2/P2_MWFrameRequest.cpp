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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_MWFrameRequest.h"

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG MWFrameRequest
#define P2_TRACE TRACE_MW_FRAME_REQUEST
#include "P2_LogHeader.h"

#include <memory>
#include <vector>

namespace P2 {

MWFrameRequest::MWFrameRequest(const ILog& log,
                               const P2Pack& pack,
                               const std::shared_ptr<P2DataObj>& p2Data,
                               const std::shared_ptr<MWInfo>& mwInfo,
                               const std::shared_ptr<MWFrame>& frame,
                               const std::shared_ptr<P2InIDMap>& p2IdMap)
    : P2FrameRequest(log, pack, p2IdMap),
      mP2Data(p2Data),
      mMWInfo(mwInfo),
      mMWFrame(frame),
      mExtracted(MFALSE),
      mImgCount(0) {
  TRACE_S_FUNC_ENTER(mLog);
  if (mMWFrame != nullptr) {
    initP2FrameData();
    IPipelineFrame::InfoIOMapSet ioMap;
    if (mMWFrame->getInfoIOMapSet(&ioMap)) {
      printIOMap(ioMap);
      if (mLog.getLogLevel() >= 1) {
        mMWFrame->print(mLog, ioMap);
      }
      P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "createP2MetaMap");
      mMetaMap = createP2MetaMap(ioMap.mMetaInfoIOMapSet);
      P2_CAM_TRACE_END(TRACE_ADVANCED);
      if (mMWInfo != nullptr) {
        // NOTE : prevent IOMapSet forget to add input
        addP2Meta(&mMetaMap, IN_APP, IO_DIR_IN);
        addP2Meta(&mMetaMap, IN_P1_HAL, IO_DIR_IN);
        addP2Meta(&mMetaMap, IN_P1_APP, IO_DIR_IN);
        addP2Meta(&mMetaMap, IN_P1_HAL_2, IO_DIR_IN);
        addP2Meta(&mMetaMap, IN_P1_APP_2, IO_DIR_IN);
      }
      P2_CAM_TRACE_BEGIN(TRACE_ADVANCED,
                         "updateP2FrameData_updateP2SensorData");
      updateP2FrameData();
      updateP2SensorData();
      P2_CAM_TRACE_END(TRACE_ADVANCED);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MWFrameRequest::~MWFrameRequest() {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::beginBatchRelease() {
  TRACE_S_FUNC_ENTER(mLog);
  if (mMWFrame != nullptr) {
    mMWFrame->beginBatchRelease();
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::endBatchRelease() {
  TRACE_S_FUNC_ENTER(mLog);
  if (mMWFrame != nullptr) {
    mMWFrame->endBatchRelease();
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::notifyNextCapture() {
  TRACE_S_FUNC_ENTER(mLog);
  if (mMWFrame != nullptr) {
    mMWFrame->notifyNextCapture();
  }
  TRACE_S_FUNC_EXIT(mLog);
}

std::vector<std::shared_ptr<P2Request>> MWFrameRequest::extractP2Requests() {
  TRACE_S_FUNC_ENTER(mLog);
  std::vector<std::shared_ptr<P2Request>> requests;
  if (mExtracted) {
    MY_S_LOGE(mLog, "Requests already extracted, should only extracted once");
  } else {
    mExtracted = MTRUE;
    IPipelineFrame::InfoIOMapSet ioMap;
    if (mMWFrame->getInfoIOMapSet(&ioMap)) {
      requests = createRequests(ioMap);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return requests;
}

MBOOL MWFrameRequest::addP2Img(P2ImgMap* imgMap,
                               const StreamId_T& sID,
                               ID_IMG id,
                               IO_DIR dir,
                               const IMG_INFO& info) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  auto it = imgMap->find(sID);
  if (it == imgMap->end()) {
    if (id != info.id) {
      MY_S_LOGW(mLog,
                "Invalid img info(0x%09" PRIx64 "/%d:%s) id=(%d)/info.id=(%d)",
                sID, id, info.name.c_str(), id, info.id);
    } else if (!(info.dir & dir)) {
      MY_S_LOGW(mLog,
                "Invalid img info(0x%09" PRIx64
                "/%d:%s) dir=wanted(%d)/listed(%d)",
                sID, id, info.name.c_str(), dir, info.dir);
    } else if (info.flag & IO_FLAG_INVALID) {
      MY_S_LOGW(mLog,
                "Invalid img info(0x%09" PRIx64
                "/%d:%s) Invalid IO_INFO: flag(%d)",
                sID, id, info.name.c_str(), info.flag);
    } else {
      P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "new MWImg");
      std::shared_ptr<P2Img> holder = std::make_shared<MWImg>(
          mLog, mP2Pack, mMWFrame, sID, dir, mMWInfo->getImgType(sID), info,
          mImgCount++, mNeedImageSWRW);
      P2_CAM_TRACE_END(TRACE_ADVANCED);
      if (holder == nullptr) {
        MY_S_LOGW(mLog, "OOM: cannot create MWImg");
      } else {
        (*imgMap)[sID] = holder;
        ret = MTRUE;
      }
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL MWFrameRequest::addP2Img(P2ImgMap* imgMap,
                               const StreamId_T& sID,
                               IO_DIR dir) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  ID_IMG id = mMWInfo->toImgID(sID);
  const IMG_INFO& info = P2Img::getImgInfo(id);
  ret = addP2Img(imgMap, sID, id, dir, info);
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL MWFrameRequest::addP2Img(P2ImgMap* imgMap, ID_IMG id, IO_DIR dir) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  auto mwImgInfo = mMWInfo->findImgInfo(id);
  if (mwImgInfo != nullptr) {
    const IMG_INFO& info = P2Img::getImgInfo(id);
    ret = addP2Img(imgMap, mwImgInfo->getStreamId(), id, dir, info);
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MWFrameRequest::P2ImgMap MWFrameRequest::createP2ImgMap(
    IPipelineFrame::ImageInfoIOMapSet const& imgSet) {
  TRACE_S_FUNC_ENTER(mLog);
  P2ImgMap holders;
  for (unsigned i = 0, n = imgSet.size(); i < n; ++i) {
    for (auto& in : imgSet[i].vIn) {
      addP2Img(&holders, in.first, IO_DIR_IN);
    }
    for (auto& out : imgSet[i].vOut) {
      addP2Img(&holders, out.first, IO_DIR_OUT);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return holders;
}

MBOOL MWFrameRequest::addP2Meta(P2MetaMap* metaMap,
                                const StreamId_T& sID,
                                ID_META id,
                                IO_DIR dir,
                                const META_INFO& info) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  auto it = metaMap->find(sID);
  if (it == metaMap->end()) {
    if (id != info.id) {
      MY_LOGE("Invalid img info(0x%09" PRIx64 "/%d:%s) id=(%d)/info.id=(%d)",
              sID, id, info.name.c_str(), id, info.id);
    } else if (!(info.dir & dir)) {
      MY_LOGE("Invalid meta info(0x%09" PRIx64
              "/%d:%s) dir=wanted(%d)/listed(%d)",
              sID, id, info.name.c_str(), dir, info.dir);
    } else if (info.flag & IO_FLAG_INVALID) {
      MY_LOGE("Invalid meta info(0x%09" PRIx64
              "/%d:%s) Invalid IO_INFO: flag(%d)",
              sID, id, info.name.c_str(), info.flag);
    } else {
      P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "new MWMeta");
      std::shared_ptr<P2Meta> holder =
          std::make_shared<MWMeta>(mLog, mP2Pack, mMWFrame, sID, dir, info);
      P2_CAM_TRACE_END(TRACE_ADVANCED);
      if (holder == nullptr) {
        MY_LOGE("OOM: cannot create MWMeta");
      } else {
        (*metaMap)[sID] = holder;
        ret = MTRUE;
      }
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL MWFrameRequest::addP2Meta(P2MetaMap* metaMap,
                                const StreamId_T& sID,
                                IO_DIR dir) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  ID_META id = mMWInfo->toMetaID(sID);
  const META_INFO& info = P2Meta::getMetaInfo(id);
  ret = addP2Meta(metaMap, sID, id, dir, info);
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL MWFrameRequest::addP2Meta(P2MetaMap* metaMap, ID_META id, IO_DIR dir) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  auto mwMetaInfo = mMWInfo->findMetaInfo(id);
  if (mwMetaInfo != nullptr) {
    const META_INFO& info = P2Meta::getMetaInfo(id);
    ret = addP2Meta(metaMap, mwMetaInfo->getStreamId(), id, dir, info);
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL MWFrameRequest::removeP2Meta(P2MetaMap* metaMap, ID_META id) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  auto mwMetaInfo = mMWInfo->findMetaInfo(id);
  if (mwMetaInfo != nullptr) {
    StreamId_T sID = mwMetaInfo->getStreamId();
    auto it = metaMap->find(sID);
    if (it != metaMap->end()) {
      metaMap->erase(it);
      ret = MTRUE;
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MWFrameRequest::P2MetaMap MWFrameRequest::createP2MetaMap(
    IPipelineFrame::MetaInfoIOMapSet const& metaSet) {
  TRACE_S_FUNC_ENTER(mLog);
  P2MetaMap holders;
  for (unsigned i = 0, n = metaSet.size(); i < n; ++i) {
    for (auto& in : metaSet[i].vIn) {
      addP2Meta(&holders, in.first, IO_DIR_IN);
    }
    for (auto& out : metaSet[i].vOut) {
      addP2Meta(&holders, out.first, IO_DIR_OUT);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return holders;
}

MVOID MWFrameRequest::configBufferSize(
    MUINT32 sensorID, const std::shared_ptr<Cropper>& cropper) {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<P2Img> rrzo = findP2Img(mImgMap, mapID(sensorID, IN_RESIZED));
  std::shared_ptr<P2Img> rsso = findP2Img(mImgMap, mapID(sensorID, IN_RSSO));
  if (isValid(rrzo)) {
    IImageBuffer* buffer = rrzo->getIImageBufferPtr();
    MSize size = cropper->getP1OutSize();
    if (size.w > 0 && size.h > 0) {
      TRACE_S_FUNC(mLog, "resize rrzo(%p) from %dx%d to %dx%d", buffer,
                   buffer->getImgSize().w, buffer->getImgSize().h, size.w,
                   size.h);
      buffer->setExtParam(size);
    }
  }
  if (isValid(rsso)) {
    MSize size(288, 162);  // default rsso buffer size
    if (!tryGet<MSize>(findP2Meta(IN_P1_HAL), MTK_P1NODE_RSS_SIZE, &size)) {
      MY_S_LOGE(mLog, "cannot get MTK_P1NODE_RSS_ZIE");
    }
    IImageBuffer* buffer = rsso->getIImageBufferPtr();
    if (size.w > 0 && size.h > 0) {
      TRACE_S_FUNC(mLog, "resize rsso(%p) from %dx%d to %dx%d", buffer,
                   buffer->getImgSize().w, buffer->getImgSize().h, size.w,
                   size.h);
      buffer->setExtParam(size);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
}

std::shared_ptr<P2Meta> MWFrameRequest::findP2Meta(const P2MetaMap& map,
                                                   ID_META id) const {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<P2Meta> meta;
  auto info = mMWInfo->findMetaInfo(id);
  if (info != nullptr) {
    auto it = map.find(info->getStreamId());
    if (it != map.end() && it->second != nullptr) {
      meta = it->second;
    }
  }
  TRACE_S_FUNC(mLog, "info = %p meta = %p", info.get(), meta.get());
  TRACE_S_FUNC_EXIT(mLog);
  return meta;
}

std::shared_ptr<P2Meta> MWFrameRequest::findP2Meta(ID_META id) const {
  return findP2Meta(mMetaMap, id);
}

std::shared_ptr<P2Img> MWFrameRequest::findP2Img(const P2ImgMap& map,
                                                 ID_IMG id) const {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<P2Img> img;
  auto info = mMWInfo->findImgInfo(id);
  if (info != nullptr) {
    auto it = map.find(info->getStreamId());
    if (it != map.end() && it->second != nullptr) {
      img = it->second;
    }
  }
  TRACE_S_FUNC(mLog, "info = %p img = %p", info.get(), img.get());
  TRACE_S_FUNC_EXIT(mLog);
  return img;
}

std::shared_ptr<P2Img> MWFrameRequest::findP2Img(ID_IMG id) const {
  return findP2Img(mImgMap, id);
}

MVOID MWFrameRequest::initP2FrameData() {
  TRACE_S_FUNC_ENTER(mLog);
  mP2Data->mFrameData.mMWFrameNo = mMWFrame->getMWFrameID();
  mP2Data->mFrameData.mMWFrameRequestNo = mMWFrame->getMWFrameRequestID();
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::updateP2FrameData() {
  TRACE_S_FUNC_ENTER(mLog);
  P2FrameData& data = mP2Data->mFrameData;
  // P1 HAL
  std::shared_ptr<P2Meta> inHalMeta = findP2Meta(mMetaMap, IN_P1_HAL);
  MINT32 appMode = getMeta<MINT32>(inHalMeta, MTK_FEATUREPIPE_APP_MODE,
                                   MTK_FEATUREPIPE_PHOTO_PREVIEW);
  data.mAppMode = appMode;
  data.mIsRecording = (appMode == MTK_FEATUREPIPE_VIDEO_RECORD) ||
                      (appMode == MTK_FEATUREPIPE_VIDEO_STOP);

  // TODO(mtk): get Master ID from hal metadata
  data.mMasterSensorID = mP2Pack.getConfigInfo().mMainSensorID;

  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::updateP2SensorData() {
  TRACE_S_FUNC_ENTER(mLog);

  for (MUINT32 sensorID : mP2Pack.getConfigInfo().mAllSensorID) {
    P2SensorData& data = mP2Data->mSensorDataMap[sensorID];
    std::shared_ptr<P2Meta> inApp =
        findP2Meta(mMetaMap, mapID(sensorID, IN_APP));
    std::shared_ptr<P2Meta> p1Hal =
        findP2Meta(mMetaMap, mapID(sensorID, IN_P1_HAL));
    std::shared_ptr<P2Meta> p1App =
        findP2Meta(mMetaMap, mapID(sensorID, IN_P1_APP));
    const P2SensorInfo& sensorInfo = mP2Pack.getSensorInfo(sensorID);

    // P1 HAL
    data.mSensorID = sensorID;
    data.mMWUniqueKey = getMeta<MINT32>(p1Hal, MTK_PIPELINE_UNIQUE_KEY, 0);
    data.mMagic3A = getMeta<MINT32>(p1Hal, MTK_P1NODE_PROCESSOR_MAGICNUM, 0);
    data.mIspProfile = getMeta<MUINT8>(p1Hal, MTK_3A_ISP_PROFILE, 0);
    extract(&data.mNDDHint, toIMetadataPtr(p1Hal));
    extract_by_SensorOpenId(&data.mNDDHint, sensorID);

    // P1 APP
    data.mP1TS = getMeta<MINT64>(p1App, MTK_SENSOR_TIMESTAMP, 0);
    if (!tryGet<MINT32>(p1App, MTK_SENSOR_SENSITIVITY, &(data.mISO))) {
      tryGet<MINT32>(inApp, MTK_SENSOR_SENSITIVITY, &(data.mISO));
    }

    // Cropper
    data.mSensorMode = getMeta<MINT32>(p1Hal, MTK_P1NODE_SENSOR_MODE, 0);
    data.mSensorSize =
        getMeta<MSize>(p1Hal, MTK_HAL_REQUEST_SENSOR_SIZE, MSize());
    if (!tryGet<MRect>(p1Hal, MTK_P1NODE_SCALAR_CROP_REGION, &(data.mP1Crop)) ||
        !tryGet<MRect>(p1Hal, MTK_P1NODE_DMA_CROP_REGION, &(data.mP1DMA)) ||
        !tryGet<MSize>(p1Hal, MTK_P1NODE_RESIZER_SIZE, &(data.mP1OutSize))) {
      data.mP1Crop = MRect(MPoint(0, 0), data.mSensorSize);
      data.mP1DMA = MRect(MPoint(0, 0), data.mSensorSize);
      data.mP1OutSize = data.mSensorSize;
    }

    if (!tryGet<MRect>(p1Hal, MTK_P1NODE_BIN_CROP_REGION, &(data.mP1BinCrop)) ||
        !tryGet<MSize>(p1Hal, MTK_P1NODE_BIN_SIZE, &(data.mP1BinSize))) {
      data.mP1BinCrop = MRect(MPoint(0, 0), data.mSensorSize);
      data.mP1BinSize = data.mSensorSize;
    }

    if (MTK_CONTROL_VIDEO_STABILIZATION_MODE_ON ==
            getMeta<MUINT8>(inApp, MTK_CONTROL_VIDEO_STABILIZATION_MODE,
                            MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF) ||
        MTK_EIS_FEATURE_EIS_MODE_ON ==
            getMeta<MINT32>(inApp, MTK_EIS_FEATURE_EIS_MODE,
                            MTK_EIS_FEATURE_EIS_MODE_OFF)) {
      data.mAppEISOn = MTRUE;
    }
    data.mAppCrop =
        getMeta<MRect>(inApp, MTK_SCALER_CROP_REGION,
                       MRect(MPoint(0, 0), sensorInfo.mActiveArray.s));
    MY_LOGD("extractLMVInfo +");
    LMVInfo lmvInfo = extractLMVInfo(mLog, toIMetadataPtr(p1Hal));
    data.mCropper =
        std::make_shared<P2Cropper>(mLog, &sensorInfo, &data, lmvInfo);
    MY_LOGD("extractLMVInfo new P2Cropper - ");
  }

  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::updateP2Metadata(
    MUINT32 sensorID, const std::shared_ptr<Cropper>& cropper) {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<P2Meta> inHalMeta;
  inHalMeta = findP2Meta(mMetaMap, mapID(sensorID, IN_P1_HAL));
  if (inHalMeta == nullptr) {
    return;
  }

  MRect rect;
  if (!inHalMeta->tryGet<MRect>(MTK_3A_PRV_CROP_REGION, &rect)) {
    std::shared_ptr<P2Img> display;
    for (auto& it : mImgMap) {
      if (it.second->isDisplay()) {
        display = it.second;
        break;
      }
    }
    if (isValid(display)) {
      std::shared_ptr<P2Img> rrzo =
          findP2Img(mImgMap, mapID(sensorID, IN_RESIZED));
      MBOOL resized = isValid(rrzo);

      MUINT32 cropFlag = 0;
      cropFlag |= resized ? Cropper::USE_RESIZED : 0;

      MCropRect crop =
          cropper->calcViewAngle(mLog, display->getTransformSize(), cropFlag);
      rect = cropper->toActive(crop, resized);
      P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "inHalMeta->trySet<MRect>");
      inHalMeta->trySet<MRect>(MTK_3A_PRV_CROP_REGION, rect);
      P2_CAM_TRACE_END(TRACE_ADVANCED);
    }
  }
  TRACE_S_FUNC_ENTER(mLog);
}

MBOOL MWFrameRequest::fillP2Img(const std::shared_ptr<P2Request>& request,
                                const std::shared_ptr<P2Img>& img) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  if (request != nullptr && img != nullptr) {
    ID_IMG id = img->getID();
    if (id == OUT_YUV) {
      request->mImgOutArray.push_back(img);
    } else {
      request->mImg[id] = img;
    }
    ret = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MBOOL MWFrameRequest::fillP2Meta(const std::shared_ptr<P2Request>& request,
                                 const std::shared_ptr<P2Meta>& meta) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  if (request != nullptr && meta != nullptr) {
    ID_META id = meta->getID();
    request->mMeta[id] = meta;
    ret = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MVOID MWFrameRequest::fillP2Img(
    const std::shared_ptr<P2Request>& request,
    const IPipelineFrame::ImageInfoIOMap& imgInfoMap,
    const P2ImgMap& p2ImgMap) {
  TRACE_S_FUNC_ENTER(mLog);
  for (auto& i : imgInfoMap.vIn) {
    auto it = p2ImgMap.find(i.first);
    if (it != p2ImgMap.end()) {
      fillP2Img(request, it->second);
    }
  }
  for (auto& i : imgInfoMap.vOut) {
    auto it = p2ImgMap.find(i.first);
    if (it != p2ImgMap.end()) {
      fillP2Img(request, it->second);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::fillP2Meta(
    const std::shared_ptr<P2Request>& request,
    const IPipelineFrame::MetaInfoIOMap& metaInfoMap,
    const P2MetaMap& p2MetaMap) {
  TRACE_S_FUNC_ENTER(mLog);
  for (auto& i : metaInfoMap.vIn) {
    auto it = p2MetaMap.find(i.first);
    if (it != p2MetaMap.end()) {
      fillP2Meta(request, it->second);
    }
  }
  for (auto& i : metaInfoMap.vOut) {
    auto it = p2MetaMap.find(i.first);
    if (it != p2MetaMap.end()) {
      fillP2Meta(request, it->second);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::fillDefaultP2Meta(
    const std::shared_ptr<P2Request>& request,
    const P2MetaMap& metaMap,
    MUINT32 sensorID) {
  TRACE_S_FUNC_ENTER(mLog);
  fillP2Meta(request, findP2Meta(metaMap, mapID(sensorID, IN_APP)));
  fillP2Meta(request, findP2Meta(metaMap, mapID(sensorID, IN_P1_HAL)));
  fillP2Meta(request, findP2Meta(metaMap, mapID(sensorID, IN_P1_APP)));
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrameRequest::printIOMap(const IPipelineFrame::InfoIOMapSet& ioMap) {
  TRACE_S_FUNC_ENTER(mLog);
  const IPipelineFrame::ImageInfoIOMapSet& imgSet = ioMap.mImageInfoIOMapSet;
  const IPipelineFrame::MetaInfoIOMapSet& metaSet = ioMap.mMetaInfoIOMapSet;
  char buffer[256];
  unsigned used = 0;

  used = snprintf(buffer, sizeof(buffer), "iomap:");

  unsigned imgSize = imgSet.size();
  unsigned metaSize = metaSet.size();
  unsigned maxSize = (imgSize >= metaSize) ? imgSize : metaSize;
  for (unsigned i = 0; i < maxSize; ++i) {
    unsigned imgIn = 0, imgOut = 0, metaIn = 0, metaOut = 0;
    if (i < imgSize) {
      imgIn = imgSet[i].vIn.size();
      imgOut = imgSet[i].vOut.size();
    }
    if (i < metaSize) {
      metaIn = metaSet[i].vIn.size();
      metaOut = metaSet[i].vOut.size();
    }
    if (used < sizeof(buffer)) {
      used += snprintf(buffer + used, sizeof(buffer) - used,
                       " [%d]=>img[%d/%d], meta[%d/%d]", i, imgIn, imgOut,
                       metaIn, metaOut);
    }
  }
  MY_S_LOGD(mLog, "%s", buffer);
  TRACE_S_FUNC_EXIT(mLog);
}

std::vector<std::shared_ptr<P2Request>> MWFrameRequest::createRequests(
    IPipelineFrame::InfoIOMapSet const& ioMap) {
  TRACE_S_FUNC_ENTER(mLog);
  std::vector<std::shared_ptr<P2Request>> requests;
  IPipelineFrame::ImageInfoIOMapSet const& imgSet = ioMap.mImageInfoIOMapSet;
  IPipelineFrame::MetaInfoIOMapSet const& metaSet = ioMap.mMetaInfoIOMapSet;

  if (imgSet.size() == 0 || metaSet.size() == 0 ||
      imgSet.size() != metaSet.size()) {
    MY_S_LOGW(mLog, "iomap image=%zu meta=%zu", imgSet.size(), metaSet.size());
  }
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED,
                     "MWFrameRequest::createRequests->createP2ImgMap");
  mImgMap = createP2ImgMap(imgSet);
  P2_CAM_TRACE_END(TRACE_ADVANCED);

  for (MUINT32 sensorID : mP2Pack.getConfigInfo().mAllSensorID) {
    std::shared_ptr<Cropper> cropper = mP2Pack.getSensorData(sensorID).mCropper;
    configBufferSize(sensorID, cropper);
    updateP2Metadata(sensorID, cropper);
  }

  TRACE_S_FUNC(mLog, "imgMap=%zu metaMap=%zu", mImgMap.size(), mMetaMap.size());

  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED,
                     "MWFrameRequest::createRequests->doRegisterPlugin");
  doRegisterPlugin();
  P2_CAM_TRACE_END(TRACE_ADVANCED);

  P2_CAM_TRACE_BEGIN(
      TRACE_ADVANCED,
      "MWFrameRequest::createRequests->newP2Request_fillImg_Meta");
  for (unsigned i = 0, n = imgSet.size(); i < n; ++i) {
    ILog reqLog = makeRequestLogger(mLog, i);
    std::shared_ptr<P2Request> request =
        std::make_shared<P2Request>(reqLog, mMWFrame, mP2Pack, mInIDMap);
    if (request == nullptr) {
      MY_S_LOGW(reqLog, "OOM: cannot allocate P2Request (%d/%d)", i, n);
      continue;
    }

    fillP2Img(request, imgSet[i], mImgMap);
    if (i < metaSet.size()) {
      fillP2Meta(request, metaSet[i], mMetaMap);
    }

    for (MUINT32 sensorID : mP2Pack.getConfigInfo().mAllSensorID) {
      fillDefaultP2Meta(request, mMetaMap, sensorID);
    }

    request->initIOInfo();
    requests.push_back(request);
  }
  P2_CAM_TRACE_END(TRACE_ADVANCED);

  this->beginBatchRelease();
  mImgMap.clear();
  mMetaMap.clear();
  this->endBatchRelease();

  TRACE_S_FUNC_EXIT(mLog);
  return requests;
}

MVOID MWFrameRequest::doRegisterPlugin() {
  TRACE_S_FUNC_ENTER(mLog);
  for (auto it : mImgMap) {
    if (it.second != nullptr) {
      it.second->registerPlugin(mImgPlugin);
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
}

}  // namespace P2
