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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_TYPE_H_

#include <stdlib.h>
#include <string.h>

/*****************************************************************************
 *
 *****************************************************************************/
#define IFD_MAX_ZEROIFD_CNT 0x20
#define IFD_MAX_EXIFIFD_CNT 0x30
#define IFD_MAX_GPSIFD_CNT 0x10
#define IFD_MAX_FIRSTIFD_CNT 0x10
#define IFD_MAX_ITOPIFD_CNT 0x08

enum ifdDataType {
  IFD_DATATYPE_BYTE = 1,
  IFD_DATATYPE_ASCII,
  IFD_DATATYPE_SHORT,
  IFD_DATATYPE_LONG,
  IFD_DATATYPE_RATIONAL,
  IFD_DATATYPE_UNDEFINED = 7,
  IFD_DATATYPE_SLONG = 9,
  IFD_DATATYPE_SRATIONAL = 10
};

enum idfTypeID {
  IFD_TYPE_ZEROIFD = 1,
  IFD_TYPE_EXIFIFD,
  IFD_TYPE_GPSIFD,
  IFD_TYPE_FIRSTIFD,
  IFD_TYPE_ITOPIFD
};

enum markerID {
  SOF_MARKER = 0xc0,
  SOF2_MARKER = 0xc2,
  DHT_MARKER = 0xc4,

  SOI_MARKER = 0xd8,
  EOI_MARKER = 0xd9,
  SOS_MARKER = 0xda,
  DQT_MARKER = 0xdb,
  DRI_MARKER = 0xdd,

  APP0_MARKER = 0xe0,
  APP1_MARKER = 0xe1,
  APP2_MARKER = 0xe2,
  APP3_MARKER = 0xe3,
  APP4_MARKER = 0xe4,
  APP5_MARKER = 0xe5,
  APP6_MARKER = 0xe6,
  APP7_MARKER = 0xe7,
  APP8_MARKER = 0xe8,
  APP9_MARKER = 0xe9,

  COM_MARKER = 0xfe,
  PREFIX_MARKER = 0xff
};

/*****************************************************************************
 *
 ******************************************************************************/
#define IFD0_TAG_IMAGE_WIDTH 0x0100
#define IFD0_TAG_IMAGE_LENGTH 0x0101
#define IFD0_TAG_IMGDESC 0x010E
#define IFD0_TAG_MAKE 0x010F
#define IFD0_TAG_MODEL 0x0110
#define IFD0_TAG_ORIENT 0x0112
#define IFD0_TAG_XRES 0x011A
#define IFD0_TAG_YRES 0x011B
#define IFD0_TAG_RESUNIT 0x0128
#define IFD0_TAG_SOFTWARE 0x0131
#define IFD0_TAG_DATETIME 0x0132
#define IFD0_TAG_YCBCRPOS 0x0213
#define IFD0_MTK_IMGINDEX \
  0x0220  // mtk definition: the index of continuous shot image.
#define IFD0_MTK_GROUPID \
  0x0221  // mtk definition: group ID for continuous
          // shot.
#define IFD0_MTK_BESTFOCUSH \
  0x0222  // mtk definition: focus value (H) for best shot.
#define IFD0_MTK_BESTFOCUSL \
  0x0223  // mtk definition: focus value (L) for best shot.
#define IFD0_MTK_REFOCUSPOS \
  0x0224  // mtk definition: sensor position (L or R) for image refocus..
#define IFD0_MTK_REFOCUSJPS 0x0225  // mtk definition: JPS file name.
#define IFD0_TAG_EXIFPTR 0x8769
#define IFD0_TAG_GPSINFO 0x8825
#define IFD0_TAG_IPM2 0xC4A5

#define EXIF_TAG_EXPTIME 0x829A
#define EXIF_TAG_FNUM 0x829D
#define EXIF_TAG_EXPPROG 0x8822
#define EXIF_TAG_ISOSPEEDRATE 0x8827
#define EXIF_TAG_EXIFVER 0x9000
#define EXIF_TAG_DATETIMEORIG 0x9003
#define EXIF_TAG_DATETIMEDITI 0x9004
#define EXIF_TAG_COMPCONFIGURE 0x9101
#define EXIF_TAG_COMPRESSBPP 0x9102
#define EXIF_TAG_EXPBIAS 0x9204
#define EXIF_TAG_MAXAPTURE 0x9205
#define EXIF_TAG_METERMODE 0x9207
#define EXIF_TAG_LIGHTSOURCE 0x9208
#define EXIF_TAG_FLASH 0x9209
#define EXIF_TAG_FOCALLEN 0x920A
#define EXIF_TAG_MAKERNOTE 0x927C
#define EXIF_TAG_USRCOMMENT 0x9286
#define EXIF_TAG_SUBSECTIME 0x9290
#define EXIF_TAG_SUBSECTIMEORIG 0x9291
#define EXIF_TAG_SUBSECTIMEDIGI 0x9292

#define EXIF_TAG_FLRESHPIXVER 0xA000
#define EXIF_TAG_COLORSPACE 0xA001
#define EXIF_TAG_PEXELXDIM 0xA002
#define EXIF_TAG_PEXELYDIM 0xA003
#define EXIF_TAG_AUDIOFILE 0xA004
#define EXIF_TAG_ITOPIFDPTR 0xA005
#define EXIF_TAG_FILESOURCE 0xA300
#define EXIF_TAG_SENCETYPE 0xA301
#define EXIF_TAG_EXPOSUREMODE 0xA402
#define EXIF_TAG_WHITEBALANCEMODE 0xA403
#define EXIF_TAG_DIGITALZOOMRATIO 0xA404
#define EXIF_TAG_FOCALLEN35MM 0xA405
#define EXIF_TAG_SCENECAPTURETYPE 0xA406

#define GPS_TAG_VERSIONID 0x0000
#define GPS_TAG_LATITUDEREF 0x0001
#define GPS_TAG_LATITUDE 0x0002
#define GPS_TAG_LONGITUDEREF 0x0003
#define GPS_TAG_LONGITUDE 0x0004
#define GPS_TAG_ALTITUDEREF 0x0005
#define GPS_TAG_ALTITUDE 0x0006
#define GPS_TAG_TIMESTAMP 0x0007
#define GPS_TAG_PROCESSINGMETHOD 0x001B
#define GPS_TAG_DATESTAMP 0x001D

#define IFD1_TAG_COMPRESS 0x0103
#define IFD1_TAG_ORIENT 0x0112
#define IFD1_TAG_XRES 0x011A
#define IFD1_TAG_YRES 0x011B
#define IFD1_TAG_RESUINT 0x0128
#define IFD1_TAG_JPG_INTERCHGFMT 0x0201
#define IFD1_TAG_JPG_INTERCHGFMTLEN 0x0202
#define IFD1_TAG_YCBCRPOS 0x0213

#define ITOP_TAG_ITOPINDEX 0x0001
#define ITOP_TAG_ITOPVERSION 0x0002

#define INVALID_TAG 0xFFFF

/*****************************************************************************
 *
 ******************************************************************************/

static unsigned char exifVersion[] = {'0', '2', '2', '0'};

static unsigned char gpsVersion[] = {2, 2, 0, 0};

static uint16_t zeroTagID[] = {
    IFD0_TAG_IMAGE_WIDTH, IFD0_TAG_IMAGE_LENGTH, IFD0_TAG_IMGDESC,
    IFD0_TAG_MAKE,        IFD0_TAG_MODEL,        IFD0_TAG_ORIENT,
    IFD0_TAG_XRES,        IFD0_TAG_YRES,         IFD0_TAG_RESUNIT,
    IFD0_TAG_SOFTWARE,    IFD0_TAG_DATETIME,     IFD0_TAG_YCBCRPOS,
    IFD0_MTK_IMGINDEX,    // mtk definition
    IFD0_MTK_GROUPID,     // mtk definition
    IFD0_MTK_BESTFOCUSH,  // mtk definition
    IFD0_MTK_BESTFOCUSL,  // mtk definition
    IFD0_MTK_REFOCUSPOS,  // mtk definition
    IFD0_MTK_REFOCUSJPS,  // mtk definition
    IFD0_TAG_EXIFPTR,     IFD0_TAG_GPSINFO};

static uint16_t exifTagID[] = {
    EXIF_TAG_EXPTIME, EXIF_TAG_FNUM, EXIF_TAG_EXPPROG, EXIF_TAG_ISOSPEEDRATE,
    EXIF_TAG_EXIFVER, EXIF_TAG_DATETIMEORIG, EXIF_TAG_DATETIMEDITI,
    EXIF_TAG_COMPCONFIGURE,
    /*EXIF_TAG_COMPRESSBPP,*/
    EXIF_TAG_EXPBIAS,
    /*EXIF_TAG_MAXAPTURE,*/
    EXIF_TAG_METERMODE, EXIF_TAG_LIGHTSOURCE, EXIF_TAG_FLASH, EXIF_TAG_FOCALLEN,
    EXIF_TAG_FOCALLEN35MM,
    /*EXIF_TAG_USRCOMMENT,*/
    EXIF_TAG_SUBSECTIME, EXIF_TAG_SUBSECTIMEORIG, EXIF_TAG_SUBSECTIMEDIGI,
    EXIF_TAG_FLRESHPIXVER, EXIF_TAG_COLORSPACE, EXIF_TAG_PEXELXDIM,
    EXIF_TAG_PEXELYDIM,
    /*EXIF_TAG_AUDIOFILE,*/
    EXIF_TAG_ITOPIFDPTR,
    /*EXIF_TAG_FILESOURCE,*/
    /*EXIF_TAG_SENCETYPE, */
    EXIF_TAG_DIGITALZOOMRATIO, EXIF_TAG_SCENECAPTURETYPE, EXIF_TAG_EXPOSUREMODE,
    EXIF_TAG_WHITEBALANCEMODE};

static uint16_t gpsTagID[] = {GPS_TAG_VERSIONID,        GPS_TAG_LATITUDEREF,
                              GPS_TAG_LATITUDE,         GPS_TAG_LONGITUDEREF,
                              GPS_TAG_LONGITUDE,        GPS_TAG_ALTITUDEREF,
                              GPS_TAG_ALTITUDE,         GPS_TAG_TIMESTAMP,
                              GPS_TAG_PROCESSINGMETHOD, GPS_TAG_DATESTAMP};

static uint16_t firstTagID[] = {IFD1_TAG_COMPRESS,
                                IFD1_TAG_ORIENT,
                                IFD1_TAG_XRES,
                                IFD1_TAG_YRES,
                                IFD1_TAG_RESUINT,
                                IFD1_TAG_JPG_INTERCHGFMT,
                                IFD1_TAG_JPG_INTERCHGFMTLEN,
                                IFD1_TAG_YCBCRPOS};

static uint16_t itopTagID[] = {ITOP_TAG_ITOPINDEX, ITOP_TAG_ITOPVERSION};

/*****************************************************************************
 *
 ******************************************************************************/
struct IFD_t {
 public:  ////    Data Members.
  uint16_t tag;
  uint16_t type;
  unsigned int count;
  unsigned int valoff;

 public:  ////    Operations.
  IFD_t() : tag(INVALID_TAG), type(0), count(0), valoff(0) {}
};

struct ifdNode_t {
 public:  ////    Data Members.
  IFD_t ifd;
  struct ifdNode_t* next;

 public:  ////    Operations.
  ifdNode_t() : ifd(), next(NULL) {}
};

struct zeroIFDList_t {
 public:  ////    Data Members.
  ifdNode_t* pheadNode;
  unsigned int nodeCnt;
  ifdNode_t ifdNodePool[IFD_MAX_ZEROIFD_CNT];
  unsigned char valBuf[0x20 * IFD_MAX_ZEROIFD_CNT];
  unsigned int valBufPos;

 public:  ////    Operations.
  zeroIFDList_t() { ::memset(this, 0, sizeof(zeroIFDList_t)); }
};

struct exifIFDList_t {
  ifdNode_t* pheadNode;
  unsigned int nodeCnt;
  ifdNode_t ifdNodePool[IFD_MAX_EXIFIFD_CNT];
  unsigned char* pvalBuf;
  unsigned char valBuf[0x40 * IFD_MAX_EXIFIFD_CNT];
  unsigned int valBufPos;

 public:  ////    Operations.
  exifIFDList_t() { ::memset(this, 0, sizeof(exifIFDList_t)); }
};

struct gpsIFDList_t {
  ifdNode_t* pheadNode;
  unsigned int nodeCnt;
  ifdNode_t ifdNodePool[IFD_MAX_GPSIFD_CNT];
  unsigned char* pvalBuf;
  unsigned char valBuf[0x20 * IFD_MAX_GPSIFD_CNT];
  unsigned int valBufPos;

 public:  ////    Operations.
  gpsIFDList_t() { ::memset(this, 0, sizeof(gpsIFDList_t)); }
};

struct firstIFDList_t {
  ifdNode_t* pheadNode;
  unsigned int nodeCnt;
  ifdNode_t ifdNodePool[IFD_MAX_FIRSTIFD_CNT];
  unsigned char* pvalBuf;
  unsigned char valBuf[0x20 * IFD_MAX_FIRSTIFD_CNT];
  unsigned int valBufPos;

 public:  ////    Operations.
  firstIFDList_t() { ::memset(this, 0, sizeof(firstIFDList_t)); }
};

struct itopIFDList_t {
 public:  ////    Data Members.
  ifdNode_t* pheadNode;
  unsigned int nodeCnt;
  ifdNode_t ifdNodePool[IFD_MAX_ITOPIFD_CNT];
  unsigned char* pvalBuf;
  unsigned char valBuf[0x20 * IFD_MAX_ITOPIFD_CNT];
  unsigned int valBufPos;

 public:  ////    Operations.
  itopIFDList_t() { ::memset(this, 0, sizeof(itopIFDList_t)); }
};

struct TiffHeader_t {
 public:  ////    Data Members.
  uint16_t byteOrder;
  uint16_t fixed;
  unsigned int ifdOffset;

 public:  ////    Operations.
  TiffHeader_t() { ::memset(this, 0, sizeof(TiffHeader_t)); }
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_TYPE_H_
