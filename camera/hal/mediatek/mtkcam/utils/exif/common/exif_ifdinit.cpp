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
#include <string.h>
//
#include "Exif.h"
#include "./exif_errcode.h"
#include "./exif_log.h"
#include "./exif_type.h"
#include "IBaseExif.h"

//

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdValueInit() {
  unsigned int err = 0;
  unsigned int idx;
  ifdNode_t* pnode = NULL;

  for (idx = 0; idx < (sizeof(zeroTagID) >> 1); idx++) {
    if ((zeroTagID[idx] == IFD0_TAG_GPSINFO) && (exifIsGpsOnFlag() == 0)) {
      continue;
    }

    pnode = ifdListNodeAlloc(IFD_TYPE_ZEROIFD);
    if (!pnode) {
      MEXIF_LOGE("ifdListNodeAlloc FAIL(ZEROIFD)");
      return LIBEXIF_IFD_ERR0004;
    }

    pnode->ifd.tag = zeroTagID[idx];
    if ((err = ifdZeroIFDValInit(pnode, ifdZeroListGet())) == 0) {
      ifdListNodeInsert(IFD_TYPE_ZEROIFD, pnode, 0);
    }
  }

  for (idx = 0; idx < (sizeof(exifTagID) >> 1); idx++) {
    pnode = ifdListNodeAlloc(IFD_TYPE_EXIFIFD);
    if (!pnode) {
      MEXIF_LOGE("ifdListNodeAlloc FAIL(EXIFIFD)");
      return LIBEXIF_IFD_ERR0004;
    }

    pnode->ifd.tag = exifTagID[idx];
    if ((err = ifdExifIFDValInit(pnode, ifdExifListGet())) == 0) {
      ifdListNodeInsert(IFD_TYPE_EXIFIFD, pnode, 0);
    }
  }

  for (idx = 0; idx < (sizeof(gpsTagID) >> 1); idx++) {
    pnode = ifdListNodeAlloc(IFD_TYPE_GPSIFD);
    if (!pnode) {
      MEXIF_LOGE("ifdListNodeAlloc FAIL(GPSIFD)");
      return LIBEXIF_IFD_ERR0004;
    }

    pnode->ifd.tag = gpsTagID[idx];
    if ((err = ifdGpsIFDValInit(pnode, ifdGpsListGet())) == 0) {
      ifdListNodeInsert(IFD_TYPE_GPSIFD, pnode, 0);
    }
  }

  for (idx = 0; idx < (sizeof(firstTagID) >> 1); idx++) {
    pnode = ifdListNodeAlloc(IFD_TYPE_FIRSTIFD);
    if (!pnode) {
      MEXIF_LOGE("ifdListNodeAlloc FAIL(FIRSTIFD)");
      return LIBEXIF_IFD_ERR0004;
    }

    pnode->ifd.tag = firstTagID[idx];
    if ((err = ifdFirstIFDValInit(pnode, ifdFirstListGet())) == 0) {
      ifdListNodeInsert(IFD_TYPE_FIRSTIFD, pnode, 0);
    }
  }

  for (idx = 0; idx < (sizeof(itopTagID) >> 1); idx++) {
    pnode = ifdListNodeAlloc(IFD_TYPE_ITOPIFD);
    if (!pnode) {
      MEXIF_LOGE("ifdListNodeAlloc FAIL(ITOPIFD)");
      return LIBEXIF_IFD_ERR0004;
    }

    pnode->ifd.tag = itopTagID[idx];
    if ((err = ifdItopIFDValInit(pnode, ifdItopListGet())) == 0) {
      ifdListNodeInsert(IFD_TYPE_ITOPIFD, pnode, 0);
    }
  }

  exifErrPrint((unsigned char*)"ifdValueInit", err);

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdZeroIFDValInit(ifdNode_t* pnode,
                                          struct zeroIFDList_t* plist) {
  unsigned int err = 0;
  unsigned char* pdata;
  IFD_t* pifd;
  unsigned int idx = 0;

  pdata = plist->valBuf + plist->valBufPos;
  pifd = &pnode->ifd;

  while (idx < plist->nodeCnt) {
    if (plist->ifdNodePool[idx].ifd.tag == pifd->tag) {
      MEXIF_LOGE("IFD duplicated! tag(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0005;
      return err;
    }
    idx++;
  }

  switch (pifd->tag) {
    case IFD0_TAG_IMAGE_WIDTH:
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      write32((unsigned char*)&pifd->valoff, 0x00000000);
      break;
    case IFD0_TAG_IMAGE_LENGTH:
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      write32((unsigned char*)&pifd->valoff, 0x00000000);
      break;
    case IFD0_TAG_IMGDESC:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->valoff = plist->valBufPos;
      pifd->count = 0x20;
      strncpy(reinterpret_cast<char*>(pdata), "Unknown Image Title            ",
              pifd->count);
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case IFD0_TAG_MAKE:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->valoff = plist->valBufPos;
      pifd->count = 0x20;
      strncpy(reinterpret_cast<char*>(pdata), "Unknown Manufacturer Name",
              pifd->count);
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case IFD0_TAG_MODEL:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->valoff = plist->valBufPos;
      pifd->count = 0x20;
      strncpy(reinterpret_cast<char*>(pdata), "Unknown Model Name ",
              pifd->count);
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case IFD0_TAG_ORIENT:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 1); /* no rotatation */
      break;
    case IFD0_TAG_XRES:
    case IFD0_TAG_YRES:
      pifd->type = IFD_DATATYPE_RATIONAL;
      pifd->count = 1;
      pifd->valoff = plist->valBufPos;
      write32(pdata, 72);
      pdata += sizeof(unsigned int);
      write32(pdata, 1);
      pdata += sizeof(unsigned int);
      plist->valBufPos += (sizeof(unsigned int) << 1);
      break;
    case IFD0_TAG_RESUNIT:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2); /* inches */
      break;
    case IFD0_MTK_REFOCUSJPS:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 32;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case IFD0_TAG_SOFTWARE:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 32;
      pifd->valoff = plist->valBufPos;
      strncpy(reinterpret_cast<char*>(pdata), "MediaTek Camera Application",
              pifd->count);
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case IFD0_TAG_DATETIME:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 20;
      pifd->valoff = plist->valBufPos;
      strncpy(reinterpret_cast<char*>(pdata), "2002:01:24 17:35:30",
              pifd->count); /* get date/time from RTC */
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case IFD0_TAG_YCBCRPOS:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2); /* cosite */
      break;
    case IFD0_MTK_IMGINDEX:    // mtk def: the index of continuous shot
                               // image. (1~n)
    case IFD0_MTK_GROUPID:     // mtk def: group ID for continuous shot.
    case IFD0_MTK_BESTFOCUSH:  // mtk def: focus value (H) for best shot.
    case IFD0_MTK_BESTFOCUSL:  // mtk def: focus value (L) for best shot.
    case IFD0_MTK_REFOCUSPOS:  // mtk def: sensor position(L/R) for image
                               // refocus.
    case IFD0_TAG_EXIFPTR:
    case IFD0_TAG_GPSINFO:
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      break;
    default:
      MEXIF_LOGE("Unsupport tag!(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0002;
      break;
  }

  exifErrPrint((unsigned char*)"ifdZeroIFDValInit", err);

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdExifIFDValInit(ifdNode_t* pnode,
                                          struct exifIFDList_t* plist) {
  unsigned int err = 0;
  unsigned char* pdata;
  IFD_t* pifd;
  unsigned int idx = 0;
  /*unsigned char timeBuf[20];*/

  pdata = plist->valBuf + plist->valBufPos;
  pifd = &pnode->ifd;

  while (idx < plist->nodeCnt) {
    if (plist->ifdNodePool[idx].ifd.tag == pifd->tag) {
      MEXIF_LOGE("IFD duplicated! tag(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0005;
      return err;
    }
    idx++;
  }

  switch (pifd->tag) {
    case EXIF_TAG_EXPTIME:
    case EXIF_TAG_FNUM:
    case EXIF_TAG_COMPRESSBPP:
      /*case EXIF_TAG_EXPBIAS: */
    case EXIF_TAG_FOCALLEN:
    case EXIF_TAG_MAXAPTURE:
      pifd->type = IFD_DATATYPE_RATIONAL;
      pifd->count = 1;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += (sizeof(unsigned int) << 1);
      pdata += (sizeof(unsigned int) << 1);
      break;
    case EXIF_TAG_EXPBIAS:
      pifd->type = IFD_DATATYPE_SRATIONAL;
      pifd->count = 1;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += (sizeof(signed int) << 1);
      pdata += (sizeof(signed int) << 1);
      break;
    case EXIF_TAG_USRCOMMENT:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 256;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += 256;
      pdata += 256;
      break;
    case EXIF_TAG_EXPPROG:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2); /* normal mode */
      break;
    case EXIF_TAG_ISOSPEEDRATE:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 0x64);
      break;
    case EXIF_TAG_EXIFVER:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 4;
      memcpy(&pifd->valoff, exifVersion, 4); /* No null for termination */
      break;
    case EXIF_TAG_DATETIMEORIG:
    case EXIF_TAG_DATETIMEDITI:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 20;
      pifd->valoff = plist->valBufPos;
      strncpy(reinterpret_cast<char*>(pdata), "2002:01:24 17:35:30",
              pifd->count);
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    case EXIF_TAG_SUBSECTIME:
    case EXIF_TAG_SUBSECTIMEORIG:
    case EXIF_TAG_SUBSECTIMEDIGI:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 2;
      memcpy(&pifd->valoff, "1", 2);  // Give default value
      break;
    case EXIF_TAG_COMPCONFIGURE:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 4;
      write32((unsigned char*)&pifd->valoff, 0x00030201);
      break;
    case EXIF_TAG_METERMODE:
    case EXIF_TAG_FOCALLEN35MM:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2); /* CenterWeightedAverage */
      break;
#if 0
    case EXIF_TAG_AUDIOFILE:
        pifd->type = IFD_DATATYPE_ASCII;
        pifd->count = 13;
        pifd->valoff = plist->valBufPos;
        /*      strcpy(pdata, "DSC_0047.WAV");*/
        plist->valBufPos += pifd->count + 1;
        pdata += pifd->count;
        break;
#endif
    case EXIF_TAG_ITOPIFDPTR:
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      write32((unsigned char*)&pifd->valoff, 0x00000000);
      break;
    case EXIF_TAG_LIGHTSOURCE:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2);
      break;
    case EXIF_TAG_FLASH:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff,
              0); /* strobe return light detected */
      break;
    case EXIF_TAG_FLRESHPIXVER:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 4;
      memcpy((unsigned char*)&pifd->valoff, "0100",
             4); /* No null for termination */
      break;
    case EXIF_TAG_COLORSPACE:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 1); /*srgb */
      break;
    case EXIF_TAG_PEXELXDIM:
    case EXIF_TAG_PEXELYDIM:
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      write32((unsigned char*)&pifd->valoff, 1024); /*srgb */
      break;
      /*case IDF_EXIF_INTEROPIFDPTR:
          break;*/
    case EXIF_TAG_FILESOURCE:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 3); /* DSC */
      break;
    case EXIF_TAG_SENCETYPE:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 1); /* directly photographed */
      break;
    case EXIF_TAG_DIGITALZOOMRATIO:
      pifd->type = IFD_DATATYPE_RATIONAL;
      pifd->count = 1;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += (sizeof(unsigned int) << 1);
      pdata += (sizeof(unsigned int) << 1);
      break;
    case EXIF_TAG_SCENECAPTURETYPE:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 0); /*scenecapturetype*/
      break;
    case EXIF_TAG_EXPOSUREMODE:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 0); /*exposureMode*/
      break;
    case EXIF_TAG_WHITEBALANCEMODE:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 0); /*whiteBalanceMode*/
      break;
    default:
      MEXIF_LOGE("Unsupport tag!(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0002;
      break;
  }

  exifErrPrint((unsigned char*)"ifdExifIFDValInit", err);

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdGpsIFDValInit(ifdNode_t* pnode,
                                         struct gpsIFDList_t* plist) {
  unsigned int err = 0;
  unsigned char* pdata;
  IFD_t* pifd;
  unsigned int idx = 0;
  /*unsigned char timeBuf[20];*/

  pdata = plist->valBuf + plist->valBufPos;
  pifd = &pnode->ifd;

  while (idx < plist->nodeCnt) {
    if (plist->ifdNodePool[idx].ifd.tag == pifd->tag) {
      MEXIF_LOGE("IFD duplicated! tag(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0005;
      return err;
    }
    idx++;
  }

  switch (pifd->tag) {
    case GPS_TAG_VERSIONID:
      pifd->type = IFD_DATATYPE_BYTE;
      pifd->count = 4;
      memcpy(&pifd->valoff, gpsVersion, 4); /* No null for termination */
      break;
    case GPS_TAG_ALTITUDEREF:
      pifd->type = IFD_DATATYPE_BYTE;
      pifd->count = 1;
      pifd->valoff = 0;
      break;
    case GPS_TAG_LATITUDEREF:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 2;
      memcpy(&pifd->valoff, "N", 2);  // Give default value
      break;
    case GPS_TAG_LONGITUDEREF:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 2;
      memcpy(&pifd->valoff, "E", 2);  // Give default value
      break;
    case GPS_TAG_LATITUDE:
    case GPS_TAG_LONGITUDE:
    case GPS_TAG_TIMESTAMP:
      pifd->type = IFD_DATATYPE_RATIONAL;
      pifd->count = 3;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += (sizeof(unsigned int) << 1) * 3;
      pdata += (sizeof(unsigned int) << 1) * 3;
      break;
    case GPS_TAG_ALTITUDE:
      pifd->type = IFD_DATATYPE_RATIONAL;
      pifd->count = 1;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += (sizeof(unsigned int) << 1) * 1;
      pdata += (sizeof(unsigned int) << 1) * 1;
      break;
    case GPS_TAG_PROCESSINGMETHOD:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 64;
      pifd->valoff = plist->valBufPos;
      plist->valBufPos += 64;
      pdata += 64;
      break;
    case GPS_TAG_DATESTAMP:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->valoff = plist->valBufPos;
      pifd->count = 11;
      plist->valBufPos += pifd->count;
      pdata += pifd->count;
      break;
    default:
      MEXIF_LOGE("Unsupport tag!(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0002;
      break;
  }

  exifErrPrint((unsigned char*)"ifdGpsIFDValInit", err);

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdFirstIFDValInit(ifdNode_t* pnode,
                                           struct firstIFDList_t* plist) {
  unsigned int err = 0;
  unsigned char* pdata;
  IFD_t* pifd;
  unsigned int idx = 0;

  pdata = plist->valBuf + plist->valBufPos;
  pifd = &pnode->ifd;

  while (idx < plist->nodeCnt) {
    if (plist->ifdNodePool[idx].ifd.tag == pifd->tag) {
      MEXIF_LOGE("IFD duplicated! tag(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0005;
      return err;
    }
    idx++;
  }

  switch (pifd->tag) {
    case IFD1_TAG_COMPRESS:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 6); /* JPEG thumbnail compress */
      break;
    case IFD1_TAG_ORIENT:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 1); /* no rotatation */
      break;
    case IFD1_TAG_XRES:
    case IFD1_TAG_YRES:
      pifd->type = IFD_DATATYPE_RATIONAL;
      pifd->count = 1;
      pifd->valoff = plist->valBufPos;
      write32(pdata, 0x48);
      pdata += 4;
      write32(pdata, 0x01);
      pdata += 4;
      plist->valBufPos += (sizeof(unsigned int) << 1);
      break;
    case IFD1_TAG_RESUINT:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2); /* inches */
      break;
    case IFD1_TAG_JPG_INTERCHGFMT: /*thumbnail offset from TIFF header */
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      break;
    case IFD1_TAG_JPG_INTERCHGFMTLEN: /*thumbnail length (from SOI to EOI) */
      pifd->type = IFD_DATATYPE_LONG;
      pifd->count = 1;
      break;
    case IFD1_TAG_YCBCRPOS:
      pifd->type = IFD_DATATYPE_SHORT;
      pifd->count = 1;
      write16((unsigned char*)&pifd->valoff, 2); /* cosite */
      break;
    default:
      MEXIF_LOGE("Unsupport tag!(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0002;
      break;
  }

  exifErrPrint((unsigned char*)"ifdFirstIFDValInit", err);

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdItopIFDValInit(ifdNode_t* pnode,
                                          struct itopIFDList_t* plist) {
  unsigned int err = 0;
  unsigned char* pdata;
  IFD_t* pifd;
  unsigned int idx = 0;

  pdata = plist->valBuf + plist->valBufPos;
  pifd = &pnode->ifd;

  while (idx < plist->nodeCnt) {
    if (plist->ifdNodePool[idx].ifd.tag == pifd->tag) {
      MEXIF_LOGE("IFD duplicated! tag(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0005;
      return err;
    }
    idx++;
  }

  switch (pifd->tag) {
    case ITOP_TAG_ITOPINDEX:
      pifd->type = IFD_DATATYPE_ASCII;
      pifd->count = 4;
      strncpy(reinterpret_cast<char*>(&pifd->valoff), "R98",
              pifd->count); /* JPEG thumbnail compress */
      break;
    case ITOP_TAG_ITOPVERSION:
      pifd->type = IFD_DATATYPE_UNDEFINED;
      pifd->count = 4;
      memcpy(reinterpret_cast<char*>(&pifd->valoff), "0100",
             4); /* No null for termination */
      break;
    default:
      MEXIF_LOGE("Unsupport tag!(0x%x)", pifd->tag);
      err = LIBEXIF_IFD_ERR0002;
      break;
  }

  exifErrPrint((unsigned char*)"ifditopIFDValInit", err);

  return err;
}
