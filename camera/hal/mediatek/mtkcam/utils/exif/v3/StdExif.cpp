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

#define LOG_TAG "mtkcam/exif"
//
#include <fstream>
#include <limits>
#include <string>
#include <stdlib.h>
#include <sys/time.h>
#include <inttypes.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>
//
#include <mtkcam/custom/ExifFactory.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/exif/IBaseCamExif.h>
#include <mtkcam/utils/exif/IICCProfile.h>
#include <mtkcam/utils/exif/StdExif.h>
#include <mtkcam/utils/std/Log.h>
//
#include "../common/Exif.h"
#include "../common/IBaseExif.h"
#include <vector>
//
using NSCam::Exif::icc_profile_display_p3;
using NSCam::Exif::icc_profile_srgb;

/**************************  Basic format for Marker **************************
 *  [ 0xFF+Marker Number(1 byte) ] + [ DataSize desciptor (2 bytes) ] + [ Data
 *(n bytes) ] DataSize desciptor should be (2+n). For example: FF E1 00 0C means
 *marker APP1(0xFFE1) has 0x000C(equal 12)bytes of data. But the data size '12'
 *includes "Data size" descriptor, it follows only 10 bytes of data after
 *0x000C. DEFAULT_DATA: Data content DEFAULT_SIZE: DataSize desciptor =
 *DEFAULT_DATA + 0x02
 **************************************************************************/
#define DEFAULT_DATA (0xFF7C)
#define DEFAULT_SIZE \
  (DEFAULT_DATA + 0x02)  // 0xFF7E: include "Data size" descriptor
#define APPN_SIZE \
  (DEFAULT_SIZE + 0x02)  // 0xFF80: should be 128-aligned after include 0xFFEn.
#define APPN_COUNT (4)

/******************************************************************************
 *
 ******************************************************************************/
static auto getDebugExif() {
  static auto const inst = MAKE_DebugExif();
  return inst;
}

static auto getBufInfo_cam() {
  static auto const inst =
      ((NULL != getDebugExif())
           ? getDebugExif()->getBufInfo(DEBUG_EXIF_KEYID_CAM)
           : NULL);
  return inst;
}

/******************************************************************************
 *
 ******************************************************************************/
static MUINT32 mapMID(MUINT32 mid) {
  switch (mid) {
#define MY_MAP(id1, id2) \
  case id1:              \
    return id2

    MY_MAP(DEBUG_EXIF_MID_CAM_CMN, ID_CMN);
    MY_MAP(DEBUG_EXIF_MID_CAM_MF, ID_MF);
    MY_MAP(DEBUG_EXIF_MID_CAM_N3D, ID_N3D);
    MY_MAP(DEBUG_EXIF_MID_CAM_SENSOR, ID_SENSOR);
    MY_MAP(DEBUG_EXIF_MID_CAM_RESERVE1, ID_RESERVE1);
    MY_MAP(DEBUG_EXIF_MID_CAM_RESERVE2, ID_RESERVE2);
    MY_MAP(DEBUG_EXIF_MID_CAM_RESERVE3, ID_RESERVE3);

#undef MY_MAP
  }
  return ID_ERROR;
}

/*****************************************************************************
 *
 ******************************************************************************/
StdExif::StdExif()
    : mpBaseExif(NULL)
      //
      ,
      mbEnableDbgExif(MFALSE),
      mApp1Size(0),
      mDbgAppnSize(0),
      mMaxThumbSize(0),
      mpOutputExifBuf(0)
      //
      ,
      mi4DbgModuleType(0),
      mpDebugIdMap(NULL),
      mICCIdx(0),
      mICCSize(0) {
  mLogLevel = ::property_get_int32("vendor.debug.camera.log", 0);
  if (mLogLevel == 0) {
    mLogLevel = ::property_get_int32("vendor.debug.camera.log.exif", 0);
  }
  MY_LOGI_IF(mLogLevel >= 1, "- this:%p", this);
}

/*****************************************************************************
 *
 ******************************************************************************/
StdExif::~StdExif() {
  MY_LOGI_IF(mLogLevel >= 1, "- this:%p", this);
}

/*****************************************************************************
 *
 ******************************************************************************/
MBOOL
StdExif::init(ExifParams const& rExifParams, MBOOL const enableDbgExif) {
  mExifParam = rExifParams;
  MY_LOGI(
      "mpExifParam(%p) ImageSize(%dx%d) FNumber(%d/10) FocalLegth(%d/1000) "
      "AWBMode(%d) Strobe(%d) AEMeterMode(%d) AEExpBias(%d)",
      &mExifParam, mExifParam.u4ImageWidth, mExifParam.u4ImageHeight,
      mExifParam.u4FNumber, mExifParam.u4FocalLength, mExifParam.u4AWBMode,
      mExifParam.u4FlashLightTimeus, mExifParam.u4AEMeterMode,
      mExifParam.i4AEExpBias);
  MY_LOGI(
      "CapExposureTime(%d) AEISOSpeed(%d) LightSource(%d) ExpProgram(%d) "
      "SceneCapType(%d) Orientation(%d) ZoomRatio(%d) Facing(%d) ICC(%d)",
      mExifParam.u4CapExposureTime, mExifParam.u4AEISOSpeed,
      mExifParam.u4LightSource, mExifParam.u4ExpProgram,
      mExifParam.u4SceneCapType, mExifParam.u4Orientation,
      mExifParam.u4ZoomRatio, mExifParam.u4Facing, mExifParam.u4ICCIdx);
  MY_LOGI(
      "GPS(%d), Altitude(%d), Latitude(%s), Longitude(%s), TimeStamp(%s), "
      "ProcessingMethod(%s)",
      mExifParam.u4GpsIsOn, mExifParam.u4GPSAltitude, mExifParam.uGPSLatitude,
      mExifParam.uGPSLongitude, mExifParam.uGPSTimeStamp,
      mExifParam.uGPSProcessingMethod);

  // DebugExif: reset debug information
  mDbgInfo.clear();
  mMapModuleID.clear();
  //
  // Exif Utilitis
  mpBaseExif = new ExifUtils();
  if (!(mpBaseExif->init(mExifParam.u4GpsIsOn))) {
    MY_LOGE("mpBaseExif->init() fail");
    return MFALSE;
  }
  //
  mpDebugIdMap = new ExifIdMap();
  //
  mbEnableDbgExif = enableDbgExif;
  mApp1Size = mpBaseExif->exifApp1SizeGet();
  mDbgAppnSize = isEnableDbgExif() ? (APPN_SIZE * APPN_COUNT) : 0;
  mICCIdx = mExifParam.u4ICCIdx;  // sRGB
  mICCIdx = ::property_get_int32("jpeg.exif.icc.profile", mExifParam.u4ICCIdx);
  mICCSize = mICCIdx == EXIF_ICC_PROFILE_SRGB
                 ? sizeof(icc_profile_srgb) / sizeof(int8_t)
                 : (mICCIdx == EXIF_ICC_PROFILE_DCI_P3
                        ? sizeof(icc_profile_display_p3) / sizeof(int8_t)
                        : 0);
  MY_LOGD_IF(mLogLevel, "ICCIdx %d ICCSize %zu", mICCIdx, mICCSize);
  //
  if (!getDebugExif()) {
    MY_LOGE("bad getDebugExif()");
    return MFALSE;
  }
  //
  if (!getBufInfo_cam()) {
    MY_LOGE("bad getBufInfo_cam()");
    return MFALSE;
  }
  //
  return MTRUE;
}

/*****************************************************************************
 *
 ******************************************************************************/
MBOOL
StdExif::uninit() {
  //
  for (MUINT32 idx = 0; idx < mDbgInfo.size(); idx++) {
    if (NULL != mDbgInfo[idx].puDbgBuf) {
      MY_LOGD_IF(mLogLevel >= 1, "idx %d", idx);
      delete[] mDbgInfo[idx].puDbgBuf;
      mDbgInfo[idx].puDbgBuf = NULL;
    }
  }

  //
  if (mpBaseExif != NULL) {
    if (!(mpBaseExif->uninit())) {
      MY_LOGE("mpBaseExif->uninit() fail");
    }
    delete mpBaseExif;
    mpBaseExif = NULL;
  }
  //
  if (mpDebugIdMap != NULL) {
    delete mpDebugIdMap;
    mpDebugIdMap = NULL;
  }
  //
  return MTRUE;
}

/*****************************************************************************
 *
 ******************************************************************************/
MBOOL
StdExif::reset(ExifParams const& rExifParams, MBOOL const enableDbgExif) {
  uninit();
  init(rExifParams, enableDbgExif);

  return MTRUE;
}

/*****************************************************************************
 *
 ******************************************************************************/
size_t StdExif::getHeaderSize() const {
  size_t size =
      getStdExifSize() + getThumbnailSize() + getDbgExifSize() + getAPP2Size();
  MY_LOGI_IF(
      mLogLevel >= 1,
      "header size(%zu) thumbSize(%zu) getDbgExifSize(%zu) getAPP2Size(%zu)",
      size, getThumbnailSize(), getDbgExifSize(), getAPP2Size());
  return size;
}

/*****************************************************************************
 *
 ******************************************************************************/
void StdExif::setMaxThumbnail(size_t const thumbnailSize) {
  mMaxThumbSize = thumbnailSize;
}

/*****************************************************************************
 *
 ******************************************************************************/
void StdExif::updateStdExif(exifAPP1Info_s* exifApp1Info) {
  ::memset(exifApp1Info, 0, sizeof(exifAPP1Info_s));

  /***************************************************************************
                                         GPS
  ****************************************************************************/
  if (mExifParam.u4GpsIsOn == 1) {
    float latitude = atof(reinterpret_cast<char*>(mExifParam.uGPSLatitude));
    float longitude = atof(reinterpret_cast<char*>(mExifParam.uGPSLongitude));
    int64_t timestamp = atol(reinterpret_cast<char*>(mExifParam.uGPSTimeStamp));
    char const* pgpsProcessingMethod =
        reinterpret_cast<char*>(mExifParam.uGPSProcessingMethod);
    //
    // Set GPS Info
    if (latitude >= 0) {
      strncpy(reinterpret_cast<char*>(exifApp1Info->gpsLatitudeRef), "N",
              sizeof(exifApp1Info->gpsLatitudeRef) / sizeof(char));
    } else {
      strncpy(reinterpret_cast<char*>(exifApp1Info->gpsLatitudeRef), "S",
              sizeof(exifApp1Info->gpsLatitudeRef) / sizeof(char));
      latitude *= -1;  // make it positive
    }
    if (longitude >= 0) {
      strncpy(reinterpret_cast<char*>(exifApp1Info->gpsLongitudeRef), "E",
              sizeof(exifApp1Info->gpsLongitudeRef) / sizeof(char));
    } else {
      strncpy(reinterpret_cast<char*>(exifApp1Info->gpsLongitudeRef), "W",
              sizeof(exifApp1Info->gpsLongitudeRef) / sizeof(char));
      longitude *= -1;  // make it positive
    }
    exifApp1Info->gpsIsOn = 1;
    // Altitude
    exifApp1Info->gpsAltitude[0] = mExifParam.u4GPSAltitude;
    exifApp1Info->gpsAltitude[1] = 1;
    // Latitude
    exifApp1Info->gpsLatitude[0] = static_cast<int>(latitude);
    exifApp1Info->gpsLatitude[1] = 1;
    latitude -= exifApp1Info->gpsLatitude[0];
    latitude *= 60;
    exifApp1Info->gpsLatitude[2] = static_cast<int>(latitude);
    exifApp1Info->gpsLatitude[3] = 1;
    latitude -= exifApp1Info->gpsLatitude[2];
    latitude *= 60;
    latitude *= 10000;
    exifApp1Info->gpsLatitude[4] = static_cast<int>(latitude);
    exifApp1Info->gpsLatitude[5] = 10000;
    // Longtitude
    exifApp1Info->gpsLongitude[0] = static_cast<int>(longitude);
    exifApp1Info->gpsLongitude[1] = 1;
    longitude -= exifApp1Info->gpsLongitude[0];
    longitude *= 60;
    exifApp1Info->gpsLongitude[2] = static_cast<int>(longitude);
    exifApp1Info->gpsLongitude[3] = 1;
    longitude -= exifApp1Info->gpsLongitude[2];
    longitude *= 60;
    longitude *= 10000;
    exifApp1Info->gpsLongitude[4] = static_cast<int>(longitude);
    exifApp1Info->gpsLongitude[5] = 10000;

    // Timestamp
    if (timestamp >= 0) {
      time_t tim = (time_t)timestamp;
      struct tm rtime;
      gmtime_r(&tim, &rtime);
      struct tm* ptime = &rtime;
      exifApp1Info->gpsTimeStamp[0] = ptime->tm_hour;
      exifApp1Info->gpsTimeStamp[1] = 1;
      exifApp1Info->gpsTimeStamp[2] = ptime->tm_min;
      exifApp1Info->gpsTimeStamp[3] = 1;
      exifApp1Info->gpsTimeStamp[4] = ptime->tm_sec;
      exifApp1Info->gpsTimeStamp[5] = 1;
      snprintf(reinterpret_cast<char*>(exifApp1Info->gpsDateStamp),
               sizeof(exifApp1Info->gpsDateStamp), "%04d:%02d:%02d",
               ptime->tm_year + 1900, ptime->tm_mon + 1, ptime->tm_mday);
    } else {
      MY_LOGE("wrong timestamp(%lld)", timestamp);
    }
    // ProcessingMethod
    const char exifAsciiPrefix[] = {0x41, 0x53, 0x43, 0x49,
                                    0x49, 0x0,  0x0,  0x0};  // ASCII
    int len1, len2, maxLen;
    len1 = sizeof(exifAsciiPrefix);
    memcpy(exifApp1Info->gpsProcessingMethod, exifAsciiPrefix, len1);
    maxLen = sizeof(exifApp1Info->gpsProcessingMethod) - len1;
    len2 = strlen(pgpsProcessingMethod);
    if (len2 > maxLen) {
      len2 = maxLen;
    }
    memcpy(&exifApp1Info->gpsProcessingMethod[len1], pgpsProcessingMethod,
           len2);
  }

  /****************************************************************************
                                         common
  ****************************************************************************/
  // software information
  memset(exifApp1Info->strSoftware, 0, 32);
  char _strSoftware[] = "MediaTek Camera Application";
  strncpy(reinterpret_cast<char*>(exifApp1Info->strSoftware),
          "MediaTek Camera Application", sizeof(_strSoftware) / sizeof(char));

  // get datetime
  struct tm r_tm;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &r_tm);
  strftime(reinterpret_cast<char*>(exifApp1Info->strDateTime), 20,
           "%Y:%m:%d %H:%M:%S", &r_tm);
  snprintf(reinterpret_cast<char*>(exifApp1Info->strSubSecTime),
           sizeof(reinterpret_cast<char*>(exifApp1Info->strSubSecTime)) - 1,
           "%02ld\n", tv.tv_usec);
  MY_LOGI("strDateTime(%s), strSubSecTime(%s)", exifApp1Info->strDateTime,
          exifApp1Info->strSubSecTime);

  // [digital zoom ratio]
  exifApp1Info->digitalZoomRatio[0] = (unsigned int)mExifParam.u4ZoomRatio;
  exifApp1Info->digitalZoomRatio[1] = 100;
  // [orientation]
  exifApp1Info->orientation = (uint16_t)determineExifOrientation(
      mExifParam.u4Orientation, mExifParam.u4Facing);

  /****************************************************************************
                                         3A
  ****************************************************************************/
  // [f number]
  exifApp1Info->fnumber[0] = (unsigned int)mExifParam.u4FNumber;
  exifApp1Info->fnumber[1] = 10;

  // [focal length]
  exifApp1Info->focalLength[0] = (unsigned int)mExifParam.u4FocalLength;
  exifApp1Info->focalLength[1] = 1000;

  // [35mm focal length]
  exifApp1Info->focalLength35mm = (uint16_t)mExifParam.u4FocalLength35mm;

  // [iso speed]
  exifApp1Info->isoSpeedRatings = (uint16_t)mExifParam.u4AEISOSpeed;

  // [exposure time]
  if (mExifParam.u4CapExposureTime == 0) {
    // YUV sensor
    exifApp1Info->exposureTime[0] = 0;
    exifApp1Info->exposureTime[1] = 0;
  } else {
    // RAW sensor
    if (mExifParam.u4CapExposureTime > 1000000) {  // 1 sec
      exifApp1Info->exposureTime[0] = mExifParam.u4CapExposureTime / 100000;
      exifApp1Info->exposureTime[1] = 10;
    } else {  // us
      exifApp1Info->exposureTime[0] = mExifParam.u4CapExposureTime;
      exifApp1Info->exposureTime[1] = 1000000;
    }
  }

  // [flashlight]
  exifApp1Info->flash = (0 != mExifParam.u4FlashLightTimeus) ? 1 : 0;

  // [white balance mode]
  exifApp1Info->whiteBalanceMode = mExifParam.u4AWBMode;

  // [light source]
  exifApp1Info->lightSource = mExifParam.u4LightSource;

  // [metering mode]
  exifApp1Info->meteringMode = mExifParam.u4AEMeterMode;

  // [exposure program] , [scene mode]
  exifApp1Info->exposureProgram = mExifParam.u4ExpProgram;
  exifApp1Info->sceneCaptureType = mExifParam.u4SceneCapType;

  // [Ev offset]
  exifApp1Info->exposureBiasValue[0] = (unsigned int)mExifParam.i4AEExpBias;
  exifApp1Info->exposureBiasValue[1] = 10;

  /**************************************************************************
                                         update customized exif
  **************************************************************************/
  {
    char make[PROPERTY_VALUE_MAX] = {'\0'};
    char model[PROPERTY_VALUE_MAX] = {'\0'};
    std::ifstream stream;
    stream.open(CAMERAPROP, std::ifstream::in);
    std::string str;
    while (std::getline(stream, str)) {
      std::size_t found;
      found = str.find("ro.product.manufacturer");
      if (found != std::string::npos) {
        std::size_t length = str.size();
        str.copy(make, (length - found - 24), (found + 24));
        continue;
      }
      found = str.find("ro.product.model");
      if (found != std::string::npos) {
        std::size_t length = str.size();
        str.copy(model, (length - found - 17), (found + 17));
        continue;
      }
    }
    MY_LOGD("model:%s,make:%s", model, make);
    ::memset(exifApp1Info->strMake, 0, 32);
    ::strncpy(reinterpret_cast<char*>(exifApp1Info->strMake),
              reinterpret_cast<const char*>(make), 31);

    ::memset(exifApp1Info->strModel, 0, 32);
    ::strncpy(reinterpret_cast<char*>(exifApp1Info->strModel),
              reinterpret_cast<const char*>(model), 31);
  }

  /***************************************************************************
                                         MISC
  ***************************************************************************/
  // [flashPixVer]
  memcpy(exifApp1Info->strFlashPixVer, "0100 ", 5);
  // [exposure mode]
  exifApp1Info->exposureMode = 0;  // 0 means Auto exposure
}

/****************************************************************************
 *
 ****************************************************************************/
void StdExif::updateDbgExif() {
  MUINT32 u4AppnCAM = 5;
  MUINT32 u4AppnAAA = 6;
  MUINT32 u4AppnISP = 7;
  MUINT32 u4AppnSHAD_TBL = 8;
  MUINT8* pDst = reinterpret_cast<MUINT8*>(getBufAddr()) + getAPP2Size() +
                 getStdExifSize() + getThumbnailSize();

  //// CAM debug info
  appendCamDebugInfo(u4AppnCAM, &pDst);

  //// AAA debug info
  appendDebugInfo(ID_AAA, u4AppnAAA, &pDst);

  //// ISP debug info
  appendDebugInfo(ID_ISP, u4AppnISP, &pDst);

  //// SHAD_ARRAY debug info
  appendDebugInfo(ID_SHAD_TABLE, u4AppnSHAD_TBL, &pDst);
}

/***************************************************************************
 *
 ***************************************************************************/
NSCam::MERROR StdExif::make(MUINTPTR const outputExifBuf,
                            size_t* rOutputExifSize) {
  int ret = 0;
  mpOutputExifBuf = outputExifBuf;
  // set 0 first for error return
  *rOutputExifSize = 0;
  MY_LOGI("out buffer(%#" PRIxPTR ")", getBufAddr());

  unsigned int u4OutputExifSize = 0;
  exifAPP1Info_s exifApp1Info;
  exifImageInfo_s exifImgInfo;

  //  (1) Fill exifApp1Info
  updateStdExif(&exifApp1Info);

  //  (2) Fill exifImgInfo
  ::memset(&exifImgInfo, 0, sizeof(exifImageInfo_t));
  exifImgInfo.bufAddr = getBufAddr();
  exifImgInfo.mainWidth = mExifParam.u4ImageWidth;
  exifImgInfo.mainHeight = mExifParam.u4ImageHeight;
  exifImgInfo.thumbSize = getThumbnailSize();

  ret =
      mpBaseExif->exifApp1Make(&exifImgInfo, &exifApp1Info, &u4OutputExifSize);
  *rOutputExifSize = (size_t)u4OutputExifSize;

  //  (4) Append App2
  int app2 = 2;
  unsigned int app2ReturnSize = 0;
  int size = mICCSize;  // Data(n bytes)
  unsigned char* pAddr = reinterpret_cast<unsigned char*>(getBufAddr()) +
                         getStdExifSize() + getThumbnailSize();
  MY_LOGD_IF(mLogLevel, "%#" PRIxPTR " offset %zu buf %p ", getBufAddr(),
             getStdExifSize() + getThumbnailSize(), pAddr);
  if (mICCIdx == EXIF_ICC_PROFILE_SRGB) {
    ret = mpBaseExif->exifAppnMake(
        app2, pAddr, reinterpret_cast<unsigned char*>(&icc_profile_srgb), size,
        &app2ReturnSize, 0);
  } else if (mICCIdx == EXIF_ICC_PROFILE_DCI_P3) {
    ret = mpBaseExif->exifAppnMake(
        app2, pAddr, reinterpret_cast<unsigned char*>(&icc_profile_display_p3),
        size, &app2ReturnSize, 0);
  } else {
    MY_LOGE("not support ICC profile %d", mICCIdx);
  }
  // return app2ReturnSize is mICCSize + 2 +2 (Data(n bytes) + Data size(2
  // bytes) + Data tag(2 bytes)) (3) Append debug exif
  if (isEnableDbgExif()) {
    updateDbgExif();
  }

  return (NSCam::MERROR)ret;
}

/****************************************************************************
 *
 ****************************************************************************/
MINT32
StdExif::determineExifOrientation(MUINT32 const u4DeviceOrientation,
                                  MBOOL const bIsFacing,
                                  MBOOL const bIsFacingFlip) {
  MINT32 result = -1;

  if (bIsFacing && bIsFacingFlip) {
    //  Front Camera with Flip
    switch (u4DeviceOrientation) {
      case 0:
        result = 1;
        break;
      case 90:
        result = 8;
        break;
      case 180:
        result = 3;
        break;
      case 270:
        result = 6;
        break;
      default:
        result = 0;
        break;
    }
  } else {
    //  Rear Camera or Front Camera without Flip
    switch (u4DeviceOrientation) {
      case 0:
        result = 1;
        break;
      case 90:
        result = 6;
        break;
      case 180:
        result = 3;
        break;
      case 270:
        result = 8;
        break;
      default:
        result = 0;
        break;
    }
  }

  return result;
}

/****************************************************************************
 *
 ****************************************************************************/
static inline void setDebugTag(void* buffer,
                               MUINT32 module_id,
                               MINT32 fieldID,
                               MINT32 fieldValue) {
  debug_exif_field* tag = reinterpret_cast<debug_exif_field*>(buffer);
  tag[fieldID].u4FieldID = DBGEXIF_TAG(module_id, fieldID, 0);
  tag[fieldID].u4FieldValue = fieldValue;
}

/****************************************************************************
 *
 ****************************************************************************/
void StdExif::setCamCommonDebugInfo() {
  auto it = getBufInfo_cam()->body_layout.find(DEBUG_EXIF_MID_CAM_CMN);
  if (it == getBufInfo_cam()->body_layout.end()) {
    MY_LOGE("cannot find the layout: DEBUG_EXIF_MID_CAM_CMN");
    return;
  }
  auto const& info = it->second;
  //
  std::vector<MUINT8> buffer(info.size, 0);
  MUINT32 u4ID = 0;
  MBOOL ret = MTRUE;

  switch (info.version) {
    case 0: {
      // Cam Debug Version
      setDebugTag(buffer.data(), info.module_id,
                  dbg_cam_common_param_0::CMN_TAG_VERSION, info.version);
      // Shot mode
      //        setDebugTag(buffer.data(), info.module_id, CMN_TAG_SHOT_MODE,
      //        mCamDbgParam.u4ShotMode);

      // Camera mode: Normal, Engineer
      //        setDebugTag(buffer.data(), info.module_id, CMN_TAG_CAM_MODE,
      //        mCamDbgParam.u4CamMode);
    } break;

    default: {
      MY_LOGE("unsupported camera common version:%d", info.version);
    } break;
  }

  ret = sendCommand(CMD_REGISTER, DEBUG_EXIF_MID_CAM_CMN,
                    reinterpret_cast<MUINTPTR>(&u4ID));
  if (!ret) {
    MY_LOGE("ERROR: CMD_REGISTER");
  }

  ret = sendCommand(CMD_SET_DBG_EXIF, u4ID, (MUINTPTR)buffer.data(),
                    buffer.size());
  if (!ret) {
    MY_LOGE("ERROR: ID_CMN");
  }
}

/*****************************************************************************
 *
 *****************************************************************************/
MBOOL
StdExif::getCamDebugInfo(MUINT8* const pDbgInfo,
                         MUINT32 const rDbgSize,
                         MINT32 const dbgModuleID) {
  MUINT8* pDbgModuleInfo = NULL;
  MUINT32 dbgModuleSize = 0;
  if (mi4DbgModuleType & dbgModuleID) {
    MUINT32 moduleIndex = 0;
    if (mMapModuleID.find(dbgModuleID) != mMapModuleID.end()) {
      moduleIndex = mMapModuleID.at(dbgModuleID);
    }
    //
    MY_LOGD_IF(mLogLevel, "Get: ID_%s(0x%04x), Size(%d), Addr(%p)",
               mpDebugIdMap->stringFor(dbgModuleID).c_str(), dbgModuleID,
               mDbgInfo[moduleIndex].u4BufSize, mDbgInfo[moduleIndex].puDbgBuf);
    //
    pDbgModuleInfo = mDbgInfo[moduleIndex].puDbgBuf;
    dbgModuleSize = mDbgInfo[moduleIndex].u4BufSize;
    //
  } else {
    //        MY_LOGW("ID_%s(0x%04x) did not exist.",
    //        mpDebugIdMap->stringFor(dbgModuleID).string(), dbgModuleID);
    return MFALSE;
  }
  //
  if (dbgModuleSize > rDbgSize) {
    MY_LOGE("ID_%s(0x%04x) ERROR size! (%d)>(%d)",
            mpDebugIdMap->stringFor(dbgModuleID).c_str(), dbgModuleID,
            dbgModuleSize, rDbgSize);
    return MFALSE;
  } else {
    memcpy(pDbgInfo, pDbgModuleInfo, rDbgSize);
  }
  //
  return MTRUE;
}

/*****************************************************************************
 *
 *****************************************************************************/
MBOOL
StdExif::appendCamDebugInfo(
    MUINT32 const dbgAppn,     //  [I] APPn for CAM module
    MUINT8** const ppuAppnBuf  //  [O] Pointer to APPn Buffer
) {
  MBOOL ret = MFALSE;

  //// CAM debug info: Common, MF, N3D, Sensor, Reserve
  {
    auto pBufInfo = getBufInfo_cam();
    if (pBufInfo == NULL) {
      MY_LOGE("Cannot get debug info!");
      return ret;
    }
    std::vector<MUINT8> aBuffer_cam(pBufInfo->header_size + pBufInfo->body_size,
                                    0);
    if (pBufInfo->header_context) {
      ::memcpy(aBuffer_cam.data(), pBufInfo->header_context,
               pBufInfo->header_size);
    } else {
      MY_LOGE("bad header_context: DEBUG_EXIF_KEYID_CAM");
      return MFALSE;
    }

    for (auto const& i : pBufInfo->body_layout) {
      auto const& v = i.second;
      getCamDebugInfo((aBuffer_cam.data() + v.offset), v.size,
                      mapMID(v.module_id));
    }
    //
    MUINT32 app5ReturnSize = 0;
    MUINT8* pDbgCamInfo = reinterpret_cast<MUINT8*>(aBuffer_cam.data());
    MUINT32 dbgCamSize = aBuffer_cam.size();
    //
    if (pDbgCamInfo && ((dbgCamSize >= 0) && (dbgCamSize <= DEFAULT_SIZE))) {
      mpBaseExif->exifAppnMake(dbgAppn, *ppuAppnBuf, pDbgCamInfo, dbgCamSize,
                               &app5ReturnSize, DEFAULT_SIZE);
      MY_LOGD_IF(mLogLevel > 1,
                 "addr: %p dbgCamSize: %d app5ReturnSize: %d DEFAULT_SIZE: %d",
                 *ppuAppnBuf, dbgCamSize, app5ReturnSize, DEFAULT_SIZE);
      //
      *ppuAppnBuf += app5ReturnSize;
    } else {
      MY_LOGE("dbgCamSize(%d) < 0", dbgCamSize);
      goto lbExit;
    }
  }

  ret = MTRUE;

lbExit:
  return ret;
}

/*****************************************************************************
 *
 ****************************************************************************/
MBOOL
StdExif::appendDebugInfo(
    MINT32 const dbgModuleID,  //  [I] debug module ID
    MINT32 const dbgAppn,      //  [I] APPn
    MUINT8** const ppuAppnBuf  //  [O] Pointer to APPn Buffer
) {
  MBOOL ret = MTRUE;
  MUINT32 appnReturnSize = 0;

  if (mi4DbgModuleType & dbgModuleID) {
    MUINT32 dbgModuleIndex = 0;
    if (mMapModuleID.find(dbgModuleID) != mMapModuleID.end()) {
      dbgModuleIndex = mMapModuleID.at(dbgModuleID);
    }
    //
    MY_LOGD_IF(mLogLevel > 1,
               "Get: ID_%s(0x%04x), Index(%d), Size(%d), Addr(%p), APP%d",
               mpDebugIdMap->stringFor(dbgModuleID).c_str(), dbgModuleID,
               dbgModuleIndex, mDbgInfo[dbgModuleIndex].u4BufSize,
               mDbgInfo[dbgModuleIndex].puDbgBuf, dbgAppn);
    //
    MUINT8* pDbgModuleInfo = mDbgInfo[dbgModuleIndex].puDbgBuf;
    MUINT32 dbgModuleSize = mDbgInfo[dbgModuleIndex].u4BufSize;
    //
    if ((dbgModuleSize >= 0) && (dbgModuleSize <= DEFAULT_SIZE)) {
      mpBaseExif->exifAppnMake(dbgAppn, *ppuAppnBuf, pDbgModuleInfo,
                               dbgModuleSize, &appnReturnSize, DEFAULT_SIZE);
      MY_LOGD_IF(mLogLevel > 1,
                 "addr: %p dbgCamSize: %d appnReturnSize: %d DEFAULT_SIZE: %d",
                 *ppuAppnBuf, dbgModuleSize, appnReturnSize, DEFAULT_SIZE);
      //
      *ppuAppnBuf += appnReturnSize;
    } else {
      MY_LOGW("dbgModuleSize(%d)", dbgModuleSize);
      ret = MFALSE;
    }
  } else {
    //        MY_LOGW("ID_%s(0x%04x) did not exist",
    //        mpDebugIdMap->stringFor(dbgModuleID).c_str(), dbgModuleID);
    MUINT8* pDbgModuleInfo = new MUINT8[DEFAULT_DATA];
    ::memset(pDbgModuleInfo, 1, sizeof(MUINT8) * DEFAULT_DATA);
    mpBaseExif->exifAppnMake(dbgAppn, *ppuAppnBuf, pDbgModuleInfo, DEFAULT_DATA,
                             &appnReturnSize, DEFAULT_SIZE);
    *ppuAppnBuf += appnReturnSize;
    delete[] pDbgModuleInfo;
    ret = MFALSE;
  }

  return ret;
}

/*****************************************************************************
 *
 ****************************************************************************/
MBOOL
StdExif::sendCommand(MINT32 cmd, MINT32 parg1, MUINTPTR parg2, MINT32 parg3) {
  MBOOL ret = MTRUE;

  // 1.Special command
  // MY_LOGI("[sendCommand] cmd: 0x%x \n", cmd);
  switch (cmd) {
    case CMD_REGISTER: {
      MUINT32 registerName = (MUINT32)parg1;
      //
      //  Is module ID (MID) ?
      auto mid = mapMID(registerName);
      if (ID_ERROR != mid) {
        *reinterpret_cast<MINT32*>(parg2) = mid;
        return MTRUE;
      }
      //
      //  Is key ID (KID) ?
      switch (registerName) {
        case DEBUG_EXIF_KEYID_ISP:
          *reinterpret_cast<MINT32*>(parg2) = ID_ISP;
          return MTRUE;
        case DEBUG_EXIF_KEYID_SHAD_TABLE:
          *reinterpret_cast<MINT32*>(parg2) = ID_SHAD_TABLE;
          return MTRUE;
        default:
          break;
      }
      switch (0xFFFFFF00 & registerName) {
        case (0xFFFFFF00 & DEBUG_EXIF_KEYID_AAA):
          *reinterpret_cast<MINT32*>(parg2) = ID_AAA;
          return MTRUE;
        default:
          break;
      }
      //
      *reinterpret_cast<MINT32*>(parg2) = ID_ERROR;
      MY_LOGE("registerID: 0x%x", registerName);
      ret = MFALSE;
    } break;
    case CMD_SET_DBG_EXIF: {
      MUINT32 u4ID = (MUINT32)parg1;
      MUINTPTR u4Addr = (MUINTPTR)parg2;
      MUINT32 u4Size = (MUINT32)parg3;
      //
      if (mi4DbgModuleType & u4ID) {
        MY_LOGW("ID_%s(0x%04x) already exists",
                mpDebugIdMap->stringFor(u4ID).c_str(), u4ID);
        ret = MFALSE;
      } else {
        if (u4ID != ID_ERROR) {
          //
          DbgInfo tmpDbgInfo;
          tmpDbgInfo.u4BufSize = u4Size;
          tmpDbgInfo.puDbgBuf = new MUINT8[u4Size];
          memset(tmpDbgInfo.puDbgBuf, 0, u4Size);
          memcpy(tmpDbgInfo.puDbgBuf, reinterpret_cast<void*>(u4Addr), u4Size);
          //
          mMapModuleID.emplace(u4ID, mDbgInfo.size());
          mDbgInfo.push_back(tmpDbgInfo);
          mi4DbgModuleType |= u4ID;
          //
          MY_LOGI("Set: ID_%s(0x%04x), Size(%d), Addr(%p)",
                  mpDebugIdMap->stringFor(u4ID).c_str(), u4ID,
                  tmpDbgInfo.u4BufSize, tmpDbgInfo.puDbgBuf);
        } else {
          MY_LOGE("ID_ERROR");
          ret = MFALSE;
        }
      }
    } break;
    default:
      MY_LOGE("unsupport cmd: 0x%x", cmd);
      ret = MFALSE;
      break;
  }
  //
  return ret;
}
