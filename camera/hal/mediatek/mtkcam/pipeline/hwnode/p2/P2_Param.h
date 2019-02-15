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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PARAM_H_

#include "P2_Header.h"
#include "P2_Common.h"
#include "P2_Info.h"

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace P2 {

enum IO_DIR {
  IO_DIR_UNKNOWN = 0,
  IO_DIR_IN = (1 << 0),
  IO_DIR_OUT = (1 << 1),
};

enum IO_STATUS {
  IO_STATUS_INVALID,
  IO_STATUS_READY,
  IO_STATUS_OK,
  IO_STATUS_ERROR
};
enum IO_FLAG {
  IO_FLAG_DEFAULT = 0,
  IO_FLAG_INVALID = (1 << 0),
  IO_FLAG_COPY = (1 << 1),
};

enum ID_META {
  NO_META,
  IN_APP,
  IN_P1_APP,
  IN_P1_HAL,
  // === Sub input ===
  IN_P1_APP_2,
  IN_P1_HAL_2,
  // === Sub input end ===
  OUT_APP,
  OUT_HAL,
  ID_META_MAX,
  ID_META_INVALID,
};

enum ID_IMG {
  NO_IMG,
  IN_REPROCESS,
  // === MaieSub input ===
  IN_OPAQUE,
  IN_FULL,
  IN_RESIZED,
  IN_LCSO,
  IN_RSSO,
  // === Sub input ===
  IN_OPAQUE_2,
  IN_FULL_2,
  IN_RESIZED_2,
  IN_LCSO_2,
  IN_RSSO_2,
  // === Sub input end ===
  OUT_FD,
  OUT_YUV,
  OUT_JPEG_YUV,
  OUT_THN_YUV,
  OUT_POSTVIEW,
  ID_IMG_MAX,
  ID_IMG_INVALID,
};

enum IMG_TYPE {
  IMG_TYPE_EXTRA,
  IMG_TYPE_DISPLAY,
  IMG_TYPE_RECORD,
  IMG_TYPE_PHYSICAL,
  IMG_TYPE_FD,
};

static inline const char* ImgType2Name(const IMG_TYPE type) {
  switch (type) {
    case IMG_TYPE_DISPLAY:
      return "disp";
    case IMG_TYPE_RECORD:
      return "rec";
    case IMG_TYPE_PHYSICAL:
      return "physical";
    case IMG_TYPE_FD:
      return "fd";
    default:
      return "extra";
  }
}

struct META_INFO {
  ID_META id = NO_META;
  ID_META mirror = NO_META;
  IO_DIR dir = IO_DIR_UNKNOWN;
  std::string name;
  MUINT32 flag = IO_FLAG_DEFAULT;

  META_INFO();
  META_INFO(ID_META sID,
            ID_META sMirror,
            IO_DIR sDir,
            const std::string& sName,
            MUINT32 sFlag);
};

struct IMG_INFO {
  ID_IMG id = NO_IMG;
  ID_IMG mirror = NO_IMG;
  IO_DIR dir = IO_DIR_UNKNOWN;
  std::string name;
  MUINT32 flag = IO_FLAG_DEFAULT;

  IMG_INFO();
  IMG_INFO(ID_IMG sID,
           ID_IMG sMirror,
           IO_DIR sDir,
           const std::string& sName,
           MUINT32 sFlag);
};

class P2InIDMap {
  typedef std::unordered_map<ID_META, ID_META> META_ID_MAP;
  typedef std::unordered_map<ID_IMG, ID_IMG> IMG_ID_MAP;

 public:
  static const META_ID_MAP MainMeta;
  static const IMG_ID_MAP MainImg;
  static const META_ID_MAP SubMeta;
  static const IMG_ID_MAP SubImg;

 public:
  P2InIDMap(const std::vector<MUINT32>& sensorIDList,
            const MUINT32 mainSensorID);
  virtual ~P2InIDMap() {}

  ID_META getMetaID(const MUINT32 sensorID, const ID_META inID);
  ID_IMG getImgID(const MUINT32 sensorID, const ID_IMG inID);
  MBOOL isEmpty(const MUINT32 sensorID);

 public:
  const MUINT32 mMainSensorID;

 private:
  std::unordered_map<MUINT32, META_ID_MAP> mSensor2MetaID;
  std::unordered_map<MUINT32, IMG_ID_MAP> mSensor2ImgID;
};

class P2MetaSet {
 public:
  P2MetaSet();

 public:
  MBOOL mHasOutput;
  IMetadata mInApp;
  IMetadata mInHal;
  IMetadata mOutApp;
  IMetadata mOutHal;
};

class P2Meta {
 public:
  static const std::unordered_map<ID_META, META_INFO> InfoMap;
  static const META_INFO& getMetaInfo(ID_META id);
  static const char* getName(ID_META id);

 public:
  P2Meta(const ILog& log, const P2Pack& p2Pack, ID_META id);
  virtual ~P2Meta() {}
  ID_META getID() const;

 public:
  virtual StreamId_T getStreamID() const = 0;
  virtual MBOOL isValid() const = 0;
  virtual IO_DIR getDir() const = 0;
  virtual MVOID updateResult(MBOOL result) = 0;
  virtual IMetadata* getIMetadataPtr() const = 0;
  virtual IMetadata::IEntry getEntry(MUINT32 tag) const = 0;
  virtual MBOOL setEntry(MUINT32 tag, const IMetadata::IEntry& entry) = 0;

  template <typename T>
  MBOOL tryGet(MUINT32 tag, T* val);
  template <typename T>
  MBOOL trySet(MUINT32 tag, const T& val);

 protected:
  ILog mLog;

 private:
  P2Pack mP2Pack;
  ID_META mMetaID;
};

class P2ImgPlugin;
class P2Img {
 public:
  static const std::unordered_map<ID_IMG, IMG_INFO> InfoMap;
  static const IMG_INFO& getImgInfo(ID_IMG id);
  static const char* getName(ID_IMG id);
  static const char* Fmt2Name(MINT fmt);

 public:
  P2Img(const ILog& log, const P2Pack& p2Pack, ID_IMG id, MUINT32 debugIndex);
  virtual ~P2Img() {}
  ID_IMG getID() const;
  const char* getHumanName() const;

 public:
  virtual StreamId_T getStreamID() const = 0;
  virtual MBOOL isValid() const = 0;
  virtual IO_DIR getDir() const = 0;
  virtual MVOID registerPlugin(
      const std::list<std::shared_ptr<P2ImgPlugin>>& plugin) = 0;
  virtual MVOID updateResult(MBOOL result) = 0;
  virtual IImageBuffer* getIImageBufferPtr() const = 0;
  virtual MUINT32 getTransform() const = 0;
  virtual MUINT32 getUsage() const = 0;
  virtual MBOOL isDisplay() const = 0;
  virtual MBOOL isRecord() const = 0;
  virtual MBOOL isCapture() const = 0;
  virtual MBOOL isPhysicalStream() const = 0;
  MSize getImgSize() const;
  MSize getTransformSize() const;
  MVOID dumpBuffer() const;
  MVOID dumpNddBuffer() const;
  MINT32 getMagic3A() const;

 protected:
  ILog mLog;

 private:
  P2Pack mP2Pack;
  ID_IMG mImgID;
  MUINT32 mDebugIndex;
};

class P2ImgPlugin {
 public:
  P2ImgPlugin() {}
  virtual ~P2ImgPlugin() {}
  virtual MBOOL onPlugin(const std::shared_ptr<P2Img>& img);
  virtual MBOOL onPlugin(const P2Img* img) = 0;
};

MBOOL isValid(const P2Meta* holder);
MBOOL isValid(const P2Img* holder);
MBOOL isValid(const std::shared_ptr<P2Meta>& holder);
MBOOL isValid(const std::shared_ptr<P2Img>& holder);

IMetadata* toIMetadataPtr(const std::shared_ptr<P2Meta>& meta);
IImageBuffer* toIImageBufferPtr(const std::shared_ptr<P2Img>& img);

template <typename T>
MBOOL P2Meta::tryGet(MUINT32 tag, T* val) {
  MBOOL ret = MFALSE;
  ret = ::P2::tryGet<T>(this->getIMetadataPtr(), tag, val);
  return ret;
}

template <typename T>
MBOOL P2Meta::trySet(MUINT32 tag, const T& val) {
  MBOOL ret = MFALSE;
  ret = ::P2::trySet<T>(this->getIMetadataPtr(), tag, val);
  return ret;
}

template <typename T>
MBOOL tryGet(const std::shared_ptr<P2Meta>& meta, MUINT32 tag, T* val) {
  MBOOL ret = MFALSE;
  if (meta != nullptr) {
    ret = meta->tryGet<T>(tag, val);
  }
  return ret;
}

template <typename T>
T getMeta(const std::shared_ptr<P2Meta>& meta, MUINT32 tag, T val) {
  if (meta != nullptr) {
    meta->tryGet<T>(tag, &val);
  }
  return val;
}

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PARAM_H_
