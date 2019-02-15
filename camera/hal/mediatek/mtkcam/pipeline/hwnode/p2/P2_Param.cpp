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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_Param.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using NSCam::TuningUtils::FILE_DUMP_NAMING_HINT;
using NSCam::TuningUtils::RAW_PORT_IMGO;
using NSCam::TuningUtils::RAW_PORT_RRZO;
using NSCam::TuningUtils::YUV_PORT_UNDEFINED;
using NSCam::TuningUtils::YUV_PORT_WDMAO;
using NSCam::TuningUtils::YUV_PORT_WROTO;

namespace P2 {

#define META_INFO_1(id, dir, name, flag) \
  { id, META_INFO(id, id, dir, name, flag) }
#define META_INFO_2(id, dir, name, flag) \
  { id##_2, META_INFO(id##_2, id, dir, name "_2", flag) }

#define IMG_INFO_1(id, dir, name, flag) \
  { id, IMG_INFO(id, id, dir, name, flag) }
#define IMG_INFO_2(id, dir, name, flag) \
  { id##_2, IMG_INFO(id##_2, id, dir, name "_2", flag) }

META_INFO::META_INFO() {}

META_INFO::META_INFO(ID_META sID,
                     ID_META sMirror,
                     IO_DIR sDir,
                     const std::string& sName,
                     MUINT32 sFlag)
    : id(sID), mirror(sMirror), dir(sDir), name(sName), flag(sFlag) {}

IMG_INFO::IMG_INFO() {}

IMG_INFO::IMG_INFO(ID_IMG sID,
                   ID_IMG sMirror,
                   IO_DIR sDir,
                   const std::string& sName,
                   MUINT32 sFlag)
    : id(sID), mirror(sMirror), dir(sDir), name(sName), flag(sFlag) {}

const std::unordered_map<ID_META, META_INFO> P2Meta::InfoMap = {
    META_INFO_1(IN_APP, IO_DIR_IN, "inApp", IO_FLAG_DEFAULT),
    META_INFO_1(IN_P1_APP, IO_DIR_IN, "inP1App", IO_FLAG_COPY),
    META_INFO_1(IN_P1_HAL, IO_DIR_IN, "inP1Hal", IO_FLAG_COPY),
    META_INFO_1(OUT_APP, IO_DIR_OUT, "outApp", IO_FLAG_DEFAULT),
    META_INFO_1(OUT_HAL, IO_DIR_OUT, "outHal", IO_FLAG_DEFAULT),

    META_INFO_2(IN_P1_APP, IO_DIR_IN, "inP1App", IO_FLAG_COPY),
    META_INFO_2(IN_P1_HAL, IO_DIR_IN, "inP1Hal", IO_FLAG_COPY),
};

const std::unordered_map<ID_IMG, IMG_INFO> P2Img::InfoMap = {
    IMG_INFO_1(IN_REPROCESS, IO_DIR_IN, "inReprocess", IO_FLAG_DEFAULT),
    IMG_INFO_1(IN_OPAQUE, IO_DIR_IN, "inOpaque", IO_FLAG_DEFAULT),
    IMG_INFO_1(IN_FULL, IO_DIR_IN, "inFull", IO_FLAG_DEFAULT),
    IMG_INFO_1(IN_RESIZED, IO_DIR_IN, "inResized", IO_FLAG_DEFAULT),
    IMG_INFO_1(IN_LCSO, IO_DIR_IN, "inLCSO", IO_FLAG_DEFAULT),
    IMG_INFO_1(IN_RSSO, IO_DIR_IN, "inRSSO", IO_FLAG_DEFAULT),
    IMG_INFO_1(OUT_FD, IO_DIR_OUT, "outFD", IO_FLAG_DEFAULT),
    IMG_INFO_1(OUT_THN_YUV, IO_DIR_OUT, "outThumbnailYUV", IO_FLAG_DEFAULT),
    IMG_INFO_1(OUT_JPEG_YUV, IO_DIR_OUT, "outJpegYUV", IO_FLAG_DEFAULT),
    IMG_INFO_1(OUT_YUV, IO_DIR_OUT, "outYUV", IO_FLAG_DEFAULT),
    IMG_INFO_1(OUT_POSTVIEW, IO_DIR_OUT, "outPostView", IO_FLAG_DEFAULT),

    IMG_INFO_2(IN_OPAQUE, IO_DIR_IN, "inOpaque", IO_FLAG_DEFAULT),
    IMG_INFO_2(IN_FULL, IO_DIR_IN, "inFull", IO_FLAG_DEFAULT),
    IMG_INFO_2(IN_RESIZED, IO_DIR_IN, "inResized", IO_FLAG_DEFAULT),
    IMG_INFO_2(IN_LCSO, IO_DIR_IN, "inLCSO", IO_FLAG_DEFAULT),
    IMG_INFO_2(IN_RSSO, IO_DIR_IN, "inRSSO", IO_FLAG_DEFAULT),
};

const std::unordered_map<ID_META, ID_META> P2InIDMap::MainMeta = {
    {IN_APP, IN_APP},
    {IN_P1_APP, IN_P1_APP},
    {IN_P1_HAL, IN_P1_HAL},
};

const std::unordered_map<ID_IMG, ID_IMG> P2InIDMap::MainImg = {
    {IN_REPROCESS, IN_REPROCESS}, {IN_OPAQUE, IN_OPAQUE}, {IN_FULL, IN_FULL},
    {IN_RESIZED, IN_RESIZED},     {IN_LCSO, IN_LCSO},     {IN_RSSO, IN_RSSO},
};

const std::unordered_map<ID_META, ID_META> P2InIDMap::SubMeta = {
    {IN_APP, IN_APP},
    {IN_P1_APP, IN_P1_APP_2},
    {IN_P1_HAL, IN_P1_HAL_2},
};

const std::unordered_map<ID_IMG, ID_IMG> P2InIDMap::SubImg = {
    {IN_REPROCESS, IN_REPROCESS}, {IN_OPAQUE, IN_OPAQUE_2},
    {IN_FULL, IN_FULL_2},         {IN_RESIZED, IN_RESIZED_2},
    {IN_LCSO, IN_LCSO_2},         {IN_RSSO, IN_RSSO_2},
};

P2InIDMap::P2InIDMap(const std::vector<MUINT32>& sensorIDList,
                     const MUINT32 mainSensorID)
    : mMainSensorID(mainSensorID) {
  for (MUINT32 sId : sensorIDList) {
    if (sId == mainSensorID) {
      mSensor2MetaID[sId] = MainMeta;
      mSensor2ImgID[sId] = MainImg;
    } else {
      // TODO(mtk): If more than 2 sensors, need add logic here
      mSensor2MetaID[sId] = SubMeta;
      mSensor2ImgID[sId] = SubImg;
    }
  }
}

ID_META P2InIDMap::getMetaID(const MUINT32 sensorID, const ID_META inID) {
  return mSensor2MetaID[sensorID][inID];
}
ID_IMG P2InIDMap::getImgID(const MUINT32 sensorID, const ID_IMG inID) {
  return mSensor2ImgID[sensorID][inID];
}

MBOOL P2InIDMap::isEmpty(const MUINT32 sensorID) {
  return (mSensor2ImgID[sensorID].empty() || mSensor2MetaID[sensorID].empty());
}

const META_INFO& P2Meta::getMetaInfo(ID_META id) {
  static const META_INFO sInvalid = {ID_META_INVALID, ID_META_INVALID,
                                     IO_DIR_UNKNOWN, "invalid",
                                     IO_FLAG_INVALID};

  auto it = P2Meta::InfoMap.find(id);
  return (it != P2Meta::InfoMap.end()) ? it->second : sInvalid;
}

const IMG_INFO& P2Img::getImgInfo(ID_IMG id) {
  static const IMG_INFO sInvalid = {ID_IMG_INVALID, ID_IMG_INVALID,
                                    IO_DIR_UNKNOWN, "invalid", IO_FLAG_INVALID};

  auto it = P2Img::InfoMap.find(id);
  return (it != P2Img::InfoMap.end()) ? it->second : sInvalid;
}

const char* P2Meta::getName(ID_META id) {
  const char* name = "unknown";
  auto it = P2Meta::InfoMap.find(id);
  if (it != P2Meta::InfoMap.end()) {
    name = it->second.name.c_str();
  }
  return name;
}

const char* P2Img::getName(ID_IMG id) {
  const char* name = "unknown";
  auto it = P2Img::InfoMap.find(id);
  if (it != P2Img::InfoMap.end()) {
    name = it->second.name.c_str();
  }
  return name;
}

P2MetaSet::P2MetaSet() : mHasOutput(MFALSE) {}

P2Meta::P2Meta(const ILog& log, const P2Pack& p2Pack, ID_META id)
    : mLog(log), mP2Pack(p2Pack), mMetaID(id) {}

ID_META P2Meta::getID() const {
  return mMetaID;
}

#define P2_CLASS_TAG P2Img
#define P2_TRACE TRACE_P2_IMG
#include "P2_LogHeader.h"

P2Img::P2Img(const ILog& log,
             const P2Pack& p2Pack,
             ID_IMG id,
             MUINT32 debugIndex)
    : mLog(log), mP2Pack(p2Pack), mImgID(id), mDebugIndex(debugIndex) {}

ID_IMG P2Img::getID() const {
  return mImgID;
}

const char* P2Img::getHumanName() const {
  const char* name = getName(mImgID);
  if (mImgID == OUT_FD) {
    name = "fd";
  } else if (mImgID == OUT_YUV) {
    name = isDisplay() ? "display" : isRecord() ? "record" : "previewCB";
  }
  return name;
}

MSize P2Img::getImgSize() const {
  MSize size(0, 0);
  if (isValid()) {
    IImageBuffer* img = getIImageBufferPtr();
    if (img) {
      size = img->getImgSize();
    }
  }
  return size;
}

MSize P2Img::getTransformSize() const {
  MSize size(0, 0);
  if (isValid()) {
    IImageBuffer* img = getIImageBufferPtr();
    if (img) {
      size = img->getImgSize();
      if (getTransform() & eTransform_ROT_90) {
        size = MSize(size.h, size.w);
      }
    }
  }
  return size;
}

const char* P2Img::Fmt2Name(MINT fmt) {
  switch (fmt) {
    case NSCam::eImgFmt_RGBA8888:
      return "rgba";
    case NSCam::eImgFmt_RGB888:
      return "rgb";
    case NSCam::eImgFmt_RGB565:
      return "rgb565";
    case NSCam::eImgFmt_STA_BYTE:
      return "byte";
    case NSCam::eImgFmt_YVYU:
      return "yvyu";
    case NSCam::eImgFmt_UYVY:
      return "uyvy";
    case NSCam::eImgFmt_VYUY:
      return "vyuy";
    case NSCam::eImgFmt_YUY2:
      return "yuy2";
    case NSCam::eImgFmt_YV12:
      return "yv12";
    case NSCam::eImgFmt_YV16:
      return "yv16";
    case NSCam::eImgFmt_NV16:
      return "nv16";
    case NSCam::eImgFmt_NV61:
      return "nv61";
    case NSCam::eImgFmt_NV12:
      return "nv12";
    case NSCam::eImgFmt_NV21:
      return "nv21";
    case NSCam::eImgFmt_I420:
      return "i420";
    case NSCam::eImgFmt_I422:
      return "i422";
    case NSCam::eImgFmt_Y800:
      return "y800";
    case NSCam::eImgFmt_BAYER8:
      return "bayer8";
    case NSCam::eImgFmt_BAYER10:
      return "bayer10";
    case NSCam::eImgFmt_BAYER12:
      return "bayer12";
    case NSCam::eImgFmt_BAYER14:
      return "bayer14";
    case NSCam::eImgFmt_FG_BAYER8:
      return "fg_bayer8";
    case NSCam::eImgFmt_FG_BAYER10:
      return "fg_bayer10";
    case NSCam::eImgFmt_FG_BAYER12:
      return "fg_bayer12";
    case NSCam::eImgFmt_FG_BAYER14:
      return "fg_bayer14";
    default:
      return "unknown";
  }
}

MVOID P2Img::dumpBuffer() const {
  IImageBuffer* buffer = this->getIImageBufferPtr();
  if (buffer) {
    MUINT32 stride, pbpp, ibpp, width, height, size;
    MINT format = buffer->getImgFormat();
    stride = buffer->getBufStridesInBytes(0);
    pbpp = buffer->getPlaneBitsPerPixel(0);
    ibpp = buffer->getImgBitsPerPixel();
    size = buffer->getBufSizeInBytes(0);
    pbpp = pbpp ? pbpp : 8;
    width = stride * 8 / pbpp;
    width = width ? width : 1;
    ibpp = ibpp ? ibpp : 8;
    height = size / width;
    if (buffer->getPlaneCount() == 1) {
      height = height * 8 / ibpp;
    }

    char path[256];
    snprintf(path, sizeof(path), DUMP_PATH "/%04d_%02d_%s_%dx%d_%d.%s.bin",
             mLog.getLogFrameID(), mDebugIndex, getHumanName(), width, height,
             stride, Fmt2Name(format));
    buffer->saveToFile(path);
  }
}

MVOID P2Img::dumpNddBuffer() const {
  IImageBuffer* buffer = this->getIImageBufferPtr();
  if (buffer) {
    char filename[256] = {0};

    IMG_INFO info = getImgInfo(mImgID);

    FILE_DUMP_NAMING_HINT hint;
    hint = mP2Pack.getSensorData().mNDDHint;
    extract(&hint, buffer);

    switch (info.mirror) {
      case IN_FULL:
        genFileName_RAW(filename, sizeof(filename), &hint, RAW_PORT_IMGO);
        break;

      case IN_FULL_2:
        genFileName_RAW(filename, sizeof(filename), &hint, RAW_PORT_IMGO,
                        info.name.c_str());
        break;

      case IN_RESIZED:
        genFileName_RAW(filename, sizeof(filename), &hint, RAW_PORT_RRZO);
        break;

      case IN_RESIZED_2:
        genFileName_RAW(filename, sizeof(filename), &hint, RAW_PORT_RRZO,
                        info.name.c_str());
        break;

      case IN_LCSO:
        genFileName_LCSO(filename, sizeof(filename), &hint);
        break;

      case IN_LCSO_2:
        genFileName_LCSO(filename, sizeof(filename), &hint, info.name.c_str());
        break;

      case OUT_YUV: {
        if (this->isDisplay()) {
          genFileName_YUV(filename, sizeof(filename), &hint, YUV_PORT_WDMAO);
        } else if (this->isRecord()) {
          genFileName_YUV(filename, sizeof(filename), &hint, YUV_PORT_WROTO);
        } else {
          genFileName_YUV(filename, sizeof(filename), &hint,
                          YUV_PORT_UNDEFINED);
        }
      } break;
      default:
        break;
    }

    if (filename[0]) {
      MY_S_LOGD(mP2Pack.mLog, "dump to: %s", filename);
      buffer->saveToFile(filename);
    }
  }
}

MINT32 P2Img::getMagic3A() const {
  return mP2Pack.getSensorData().mMagic3A;
}

MBOOL P2ImgPlugin::onPlugin(const std::shared_ptr<P2Img>& img) {
  return onPlugin(img.get());
}

MBOOL isValid(const P2Meta* meta) {
  return (meta != nullptr) && meta->isValid();
}

MBOOL isValid(const P2Img* img) {
  return (img != nullptr) && img->isValid();
}

MBOOL isValid(const std::shared_ptr<P2Meta>& meta) {
  return (meta != nullptr) && meta->isValid();
}

MBOOL isValid(const std::shared_ptr<P2Img>& img) {
  return (img != nullptr) && img->isValid();
}

IMetadata* toIMetadataPtr(const std::shared_ptr<P2Meta>& meta) {
  return meta != nullptr ? meta->getIMetadataPtr() : nullptr;
}

IImageBuffer* toIImageBufferPtr(const std::shared_ptr<P2Img>& img) {
  return img != nullptr ? img->getIImageBufferPtr() : nullptr;
}

}  // namespace P2
