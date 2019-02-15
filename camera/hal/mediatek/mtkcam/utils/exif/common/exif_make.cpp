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
//
#include <exif/common/Exif.h>
#include <exif/common/exif_errcode.h>
#include <exif/common/exif_log.h>
#include <exif/common/exif_type.h>
#include <exif/common/IBaseExif.h>
//
#include <property_service/property.h>
#include <property_service/property_lib.h>
#include <stdlib.h>
#include <string.h>
//

/*****************************************************************************
 *
 ******************************************************************************/
ExifUtils::ExifUtils()
    : IBaseExif(),
      mpzeroList(NULL),
      mpexifList(NULL),
      mpgpsList(NULL),
      mpfirstList(NULL),
      mpitopList(NULL),
      exifGpsEnFlag(0),
      mpexifHdrTmplBuf(NULL),
      miLogLevel(1) {
  char cLogLevel[PROPERTY_VALUE_MAX] = {'\0'};
  property_get("vendor.debug.camera.exif.loglevel", cLogLevel, "1");
  miLogLevel = ::atoi(cLogLevel);
  MEXIF_LOGD("- this:%p, debug.camera.exif.loglevel=%s", this, cLogLevel);
}

/*****************************************************************************
 *
 ******************************************************************************/
ExifUtils::~ExifUtils() {
  MEXIF_LOGD("");
}

/*****************************************************************************
 *
 ******************************************************************************/
bool ExifUtils::init(unsigned int const gpsEnFlag) {
  MEXIF_LOGD("gpsEnFlag(%d)", gpsEnFlag);
  unsigned int err = EXIF_NO_ERROR;

  exifGpsEnFlag = gpsEnFlag;

  ifdListInit();
  err = ifdValueInit();
  if (err != EXIF_NO_ERROR) {
    MEXIF_LOGE("ifdValueInit FAIL(%x)", err);
    return err;
  }

  return true;
}

/*****************************************************************************
 *
 ******************************************************************************/
bool ExifUtils::uninit() {
  MEXIF_LOGD_IF((2 <= miLogLevel), "");
  ifdListUninit();

  return true;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifTagUpdate(exifImageInfo_t* pexifImgInfo,
                                      exifAPP1Info_t* pexifAPP1Info) {
  unsigned int err = EXIF_NO_ERROR;
  unsigned char buf[64];

  unsigned int w = pexifImgInfo->mainWidth;
  unsigned int h = pexifImgInfo->mainHeight;

  memcpy(buf, (unsigned char*)&w, 4);
  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_PEXELXDIM, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  memcpy(buf, (unsigned char*)&h, 4);
  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_PEXELYDIM, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  // The exifApp1Sizeof excludes thumbnail size
  *(unsigned int*)buf = exifApp1Sizeof() - 0x0a;
  err = ifdListNodeModify(IFD_TYPE_FIRSTIFD, IFD1_TAG_JPG_INTERCHGFMT, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  *(unsigned int*)buf = pexifImgInfo->thumbSize;
  err = ifdListNodeModify(IFD_TYPE_FIRSTIFD, IFD1_TAG_JPG_INTERCHGFMTLEN, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_IMAGE_WIDTH,
                          &pexifImgInfo->mainWidth);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_IMAGE_LENGTH,
                          &pexifImgInfo->mainHeight);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  /* make the string tag compatible with EXIF2.2 */
  memset(buf, 0, 32);
  memcpy(buf, (const char*)pexifAPP1Info->strImageDescription,
         strlen((const char*)pexifAPP1Info->strImageDescription));
  buf[31] = 0;
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_IMGDESC, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  /* make the string tag compatible with EXIF2.2 */
  memset(buf, 0, 32);
  memcpy(buf, (const char*)pexifAPP1Info->strMake,
         strlen((const char*)pexifAPP1Info->strMake));
  buf[31] = 0;
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_MAKE, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  /* make the string tag compatible with EXIF2.2 */
  memset(buf, 0, 32);
  memcpy(buf, (const char*)pexifAPP1Info->strModel,
         strlen((const char*)pexifAPP1Info->strModel));
  buf[31] = 0;
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_MODEL, buf);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_SOFTWARE,
                          pexifAPP1Info->strSoftware);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_DATETIME,
                          pexifAPP1Info->strDateTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_ORIENT,
                          &pexifAPP1Info->orientation);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  // mtk definition: the index of continuous shot image. (1~n)
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_MTK_IMGINDEX,
                          &pexifAPP1Info->imgIndex);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  // mtk definition: group ID for continuous shot.
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_MTK_GROUPID,
                          &pexifAPP1Info->groupID);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  // mtk definition: focus value(H) for best shot.
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_MTK_BESTFOCUSH,
                          &pexifAPP1Info->bestFocusH);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  // mtk definition: focus value(L) for best shot.
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_MTK_BESTFOCUSL,
                          &pexifAPP1Info->bestFocusL);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  // mtk definition: main sensor position (L or R) for image refocus.
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_MTK_REFOCUSPOS,
                          &pexifAPP1Info->refocusPos);
  if (err != EXIF_NO_ERROR) {
    return err;
  }
  // mtk definition: JPS file name for image refocus.
  err = ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_MTK_REFOCUSJPS,
                          pexifAPP1Info->strJpsFileName);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_FLASH,
                          &pexifAPP1Info->flash);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_DATETIMEORIG,
                          pexifAPP1Info->strDateTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_DATETIMEDITI,
                          pexifAPP1Info->strDateTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_SUBSECTIME,
                          pexifAPP1Info->strSubSecTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_SUBSECTIMEORIG,
                          pexifAPP1Info->strSubSecTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_SUBSECTIMEDIGI,
                          pexifAPP1Info->strSubSecTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_EXPPROG,
                          &pexifAPP1Info->exposureProgram);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_EXPTIME,
                          pexifAPP1Info->exposureTime);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_EXPBIAS,
                          pexifAPP1Info->exposureBiasValue);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_FNUM,
                          pexifAPP1Info->fnumber);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_FOCALLEN,
                          pexifAPP1Info->focalLength);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_FOCALLEN35MM,
                          &pexifAPP1Info->focalLength35mm);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_ISOSPEEDRATE,
                          &pexifAPP1Info->isoSpeedRatings);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_METERMODE,
                          &pexifAPP1Info->meteringMode);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_DIGITALZOOMRATIO,
                          pexifAPP1Info->digitalZoomRatio);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_SCENECAPTURETYPE,
                          &pexifAPP1Info->sceneCaptureType);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_LIGHTSOURCE,
                          &pexifAPP1Info->lightSource);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_FLRESHPIXVER,
                          pexifAPP1Info->strFlashPixVer);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_EXPOSUREMODE,
                          &pexifAPP1Info->exposureMode);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  err = ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_WHITEBALANCEMODE,
                          &pexifAPP1Info->whiteBalanceMode);
  if (err != EXIF_NO_ERROR) {
    return err;
  }

  if (exifIsGpsOnFlag()) {
    // Update GPS info
    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_ALTITUDE,
                            &pexifAPP1Info->gpsAltitude[0]);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_LATITUDEREF,
                            pexifAPP1Info->gpsLatitudeRef);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_LATITUDE,
                            &pexifAPP1Info->gpsLatitude[0]);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_LONGITUDEREF,
                            pexifAPP1Info->gpsLongitudeRef);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_LONGITUDE,
                            &pexifAPP1Info->gpsLongitude[0]);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_TIMESTAMP,
                            &pexifAPP1Info->gpsTimeStamp[0]);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_PROCESSINGMETHOD,
                            pexifAPP1Info->gpsProcessingMethod);
    if (err != EXIF_NO_ERROR) {
      return err;
    }

    err = ifdListNodeModify(IFD_TYPE_GPSIFD, GPS_TAG_DATESTAMP,
                            pexifAPP1Info->gpsDateStamp);
    if (err != EXIF_NO_ERROR) {
      return err;
    }
    //
  }

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifApp1Make(exifImageInfo_t* pexifImgInfo,
                                     exifAPP1Info_t* pexifAPP1Info,
                                     unsigned int* pretSize) {
  unsigned int err = EXIF_NO_ERROR;
  unsigned int size = 0;
  unsigned int app1Size = 0;
  unsigned char* pdata;

  MEXIF_LOGD_IF((2 <= miLogLevel), "+");

  exifHdrTmplAddrSet((unsigned char*)pexifImgInfo->bufAddr);

  exifGpsEnFlag = pexifAPP1Info->gpsIsOn;

  pdata = (unsigned char*)pexifImgInfo->bufAddr;
  // Start of Image
  exifSOIWrite(pdata, &size);
  pdata += size;

  /* For EXIF APP1 */
  if ((err = exifAPP1Write(pdata, &size)) != 0) {
    MEXIF_LOGE("exifAPP1Write FAIL(%x)", err);
    return err;
  }
  // Fill the app1 size
  app1Size = exifApp1Sizeof() + pexifImgInfo->thumbSize;
  write16(pdata + 2, mySwap16(app1Size - 2));

  if ((err = exifTagUpdate(pexifImgInfo, pexifAPP1Info)) != 0) {
    MEXIF_LOGE("exifTagUpdate FAIL(%x)", err);
    return err;
  }

  // Return the exif App1 size without thumbnail
  *pretSize = exifApp1Sizeof() + 2;

  //    ifdListUninit();

  MEXIF_LOGD_IF((2 <= miLogLevel), "-");
  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifAppnMake(unsigned int appn,
                                     unsigned char* paddr,
                                     unsigned char* pdata,
                                     unsigned int dataSize,
                                     unsigned int* pretSize,
                                     unsigned int defaultSize) {
  if ((defaultSize > 0) && (defaultSize < dataSize)) {
    MEXIF_LOGE("dataSize(%d) > defaultSize(%d)", dataSize, defaultSize);
    return EXIF_UNKNOWN_ERROR;
  }

  unsigned int appnSize = defaultSize > 0 ? defaultSize : dataSize + 0x02;

  // write Appn marker
  *paddr++ = 0xFF;
  *paddr++ = APP0_MARKER + appn;
  write16(paddr, mySwap16(appnSize));
  paddr += 2;

  memcpy(paddr, pdata, dataSize);

  *pretSize = appnSize + 0x02;

  return EXIF_NO_ERROR;
}
