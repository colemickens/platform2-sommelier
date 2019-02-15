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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_STDEXIF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_STDEXIF_H_
//
#include <map>
#include <string>
#include <vector>
//
#include "IBaseCamExif.h"
//
class IBaseExif;
struct exifAPP1Info_s;
//
/*******************************************************************************
 * Camera EXIF Command
 ******************************************************************************/
enum eDebugExifId {
  //
  ID_ERROR = 0x00000001,
  //
  ID_EIS = 0x00000002,
  ID_AAA = 0x00000004,
  ID_ISP = 0x00000008,
  ID_CMN = 0x00000010,
  ID_MF = 0x00000020,
  ID_N3D = 0x00000040,
  ID_SENSOR = 0x00000080,
  ID_RESERVE1 = 0x00000100,
  ID_RESERVE2 = 0x00000200,
  ID_RESERVE3 = 0x00000400,
  ID_SHAD_TABLE = 0x00001000
  //
};
enum {
  EXIF_ICC_PROFILE_DCI_P3 = 6,
  EXIF_ICC_PROFILE_SRGB = 7,
  EXIF_ICC_PROFILE_MAX
};
struct ExifIdMap {
  typedef MUINT32 VAL_T;
  typedef std::string STR_T;
  //
  ExifIdMap() : m_vStr2Val(), m_vVal2Str() {
    m_vStr2Val.clear();
    m_vVal2Str.clear();
#define _ADD_STRING_VALUE_MAP_(_str_, _val_)       \
  do {                                             \
    m_vStr2Val.emplace(std::string(_str_), _val_); \
    m_vVal2Str.emplace(_val_, std::string(_str_)); \
  } while (0)
    _ADD_STRING_VALUE_MAP_("ERROR", ID_ERROR);
    _ADD_STRING_VALUE_MAP_("AAA", ID_AAA);
    _ADD_STRING_VALUE_MAP_("ISP", ID_ISP);
    _ADD_STRING_VALUE_MAP_("COMMON", ID_CMN);
    _ADD_STRING_VALUE_MAP_("MF", ID_MF);
    _ADD_STRING_VALUE_MAP_("N3D", ID_N3D);
    _ADD_STRING_VALUE_MAP_("SENSOR", ID_SENSOR);
    _ADD_STRING_VALUE_MAP_("EIS", ID_EIS);
    _ADD_STRING_VALUE_MAP_("SHAD/RESERVE1", ID_RESERVE1);
    _ADD_STRING_VALUE_MAP_("RESERVE2", ID_RESERVE2);
    _ADD_STRING_VALUE_MAP_("RESERVE3", ID_RESERVE3);
    _ADD_STRING_VALUE_MAP_("SHAD_TABLE", ID_SHAD_TABLE);
#undef _ADD_STRING_VALUE_MAP_
  }
  virtual ~ExifIdMap() {}
  //
  virtual VAL_T valueFor(STR_T const& str) const {
    if (m_vStr2Val.find(str) != m_vStr2Val.end()) {
      return m_vStr2Val.at(str);
    }
    return 0;
  }
  virtual STR_T stringFor(VAL_T const& val) const {
    if (m_vVal2Str.find(val) != m_vVal2Str.end()) {
      return m_vVal2Str.at(val);
    }
    return std::string("");
  }
  //
 private:
  std::map<STR_T, VAL_T> m_vStr2Val;
  std::map<VAL_T, STR_T> m_vVal2Str;
};

/*******************************************************************************
 *
 ******************************************************************************/
struct DbgInfo {
  mutable MUINT8* puDbgBuf;
  MUINT32 u4BufSize;

  explicit DbgInfo(MUINT8* _puDbgBuf = NULL, MUINT32 _u4BufSize = 0)
      : puDbgBuf(_puDbgBuf), u4BufSize(_u4BufSize) {}
};

/*******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC StdExif : public IBaseCamExif {
 public:  ////    Constructor/Destructor
  StdExif();
  ~StdExif();

 public:
  MBOOL init(ExifParams const& rExifParams, MBOOL const enableDbgExif = false);
  MBOOL uninit();
  MBOOL reset(ExifParams const& rExifParams, MBOOL const enableDbgExif = false);

  virtual MBOOL sendCommand(MINT32 cmd,
                            MINT32 arg1 = 0,
                            MUINTPTR arg2 = 0,
                            MINT32 arg3 = 0);

  // get the size of standard exif (with out thumbnail)
  size_t getStdExifSize() const { return mApp1Size; }

  // get the size of debug exif
  size_t getDbgExifSize() const { return mDbgAppnSize; }

  // get the size of jpeg header (including standard exif, thumbnail, debug exif
  // size)
  size_t getHeaderSize() const;

  // set maximum thumbnail size
  void setMaxThumbnail(size_t const thumbnailSize);

  // make standard and debug exif
  NSCam::MERROR make(MUINTPTR const outputExifBuf, size_t* rOutputExifSize);

 private:
  MUINTPTR getBufAddr() const { return mpOutputExifBuf; }

  size_t getThumbnailSize() const { return mMaxThumbSize; }

  // Data content(mICCSize) + tag(2 bytes) + Data Size(2bytes)}
  size_t getAPP2Size() const { return mICCSize == 0 ? 0 : mICCSize + 4; }
  MBOOL isEnableDbgExif() const { return mbEnableDbgExif; }

  void updateStdExif(exifAPP1Info_s* exifApp1Info);

  void updateDbgExif();

  void setCamCommonDebugInfo();

  MBOOL getCamDebugInfo(MUINT8* const pDbgInfo,
                        MUINT32 const rDbgSize,
                        MINT32 const dbgModuleID);

  MBOOL appendDebugInfo(
      MINT32 const dbgModuleID,  //  [I] debug module ID
      MINT32 const dbgAppn,      //  [I] APPn
      MUINT8** const ppuAppnBuf /*  [O] Pointer to APPn Buffer */);

  MBOOL appendCamDebugInfo(
      MUINT32 const dbgAppn,  //  [I] APPn for CAM module
      MUINT8** const puAppnBuf /*  [O] Pointer to APPn Buffer */);

  MINT32 determineExifOrientation(MUINT32 const u4DeviceOrientation,
                                  MBOOL const bIsFacing,
                                  MBOOL const bIsFacingFlip = MFALSE);

 private:  ////
  //
  ExifParams mExifParam;
  IBaseExif* mpBaseExif;
  //
  MBOOL mbEnableDbgExif;
  size_t mApp1Size;
  size_t mDbgAppnSize;
  size_t mMaxThumbSize;
  MUINTPTR mpOutputExifBuf;
  //
  std::vector<DbgInfo> mDbgInfo;
  std::map<MUINT32, MUINT32> mMapModuleID;
  MINT32 mi4DbgModuleType;
  ExifIdMap* mpDebugIdMap;
  MINT32 mICCIdx;
  MINT32 mICCSize;
  //
  MINT32 mLogLevel;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_STDEXIF_H_
