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
#include "IBaseExif.h"
#include "./exif_errcode.h"
#include "./exif_type.h"
//

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListSizeof() const {
  unsigned int size = 0;
  zeroIFDList_t* pzeroList;
  exifIFDList_t* pexifList;
  gpsIFDList_t* pgpsList;
  firstIFDList_t* pfirstList;
  itopIFDList_t* pitopList;

  pzeroList = ifdZeroListGet();
  pexifList = ifdExifListGet();
  pgpsList = ifdGpsListGet();
  pfirstList = ifdFirstListGet();
  pitopList = ifdItopListGet();

  size += (pzeroList->nodeCnt * sizeof(IFD_t) + pzeroList->valBufPos);
  size += (pexifList->nodeCnt * sizeof(IFD_t) + pexifList->valBufPos);
  size += (pgpsList->nodeCnt * sizeof(IFD_t) + pgpsList->valBufPos);
  size += (pfirstList->nodeCnt * sizeof(IFD_t) + pfirstList->valBufPos);
  size += (pitopList->nodeCnt * sizeof(IFD_t) + pitopList->valBufPos);

  return size;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned char* ExifUtils::ifdListValBufGet(unsigned int ifdType) {
  unsigned int err = EXIF_NO_ERROR;
  void* plist;
  unsigned char* pdata = 0;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      plist = ifdZeroListGet();
      pdata = (reinterpret_cast<zeroIFDList_t*>(plist))->valBuf;
      break;
    case IFD_TYPE_EXIFIFD:
      plist = ifdExifListGet();
      pdata = (reinterpret_cast<exifIFDList_t*>(plist))->valBuf;
      break;
    case IFD_TYPE_GPSIFD:
      plist = ifdGpsListGet();
      pdata = (reinterpret_cast<gpsIFDList_t*>(plist))->valBuf;
      break;
    case IFD_TYPE_FIRSTIFD:
      plist = ifdFirstListGet();
      pdata = (reinterpret_cast<firstIFDList_t*>(plist))->valBuf;
      break;
    case IFD_TYPE_ITOPIFD:
      plist = ifdItopListGet();
      pdata = (reinterpret_cast<itopIFDList_t*>(plist))->valBuf;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  return pdata;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListValBufSizeof(unsigned int ifdType) {
  unsigned int err = EXIF_NO_ERROR;
  unsigned int cnt = 0;
  void* plist;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      plist = ifdZeroListGet();
      cnt = (reinterpret_cast<zeroIFDList_t*>(plist))->valBufPos;
      break;
    case IFD_TYPE_EXIFIFD:
      plist = ifdExifListGet();
      cnt = (reinterpret_cast<exifIFDList_t*>(plist))->valBufPos;
      break;
    case IFD_TYPE_GPSIFD:
      plist = ifdGpsListGet();
      cnt = (reinterpret_cast<gpsIFDList_t*>(plist))->valBufPos;
      break;
    case IFD_TYPE_FIRSTIFD:
      plist = ifdFirstListGet();
      cnt = (reinterpret_cast<firstIFDList_t*>(plist))->valBufPos;
      break;
    case IFD_TYPE_ITOPIFD:
      plist = ifdItopListGet();
      cnt = (reinterpret_cast<itopIFDList_t*>(plist))->valBufPos;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }
  return cnt;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListNodeCntGet(unsigned int ifdType) {
  unsigned int err = EXIF_NO_ERROR;
  unsigned int cnt = 0;
  void* plist = 0;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      plist = ifdZeroListGet();
      cnt = (reinterpret_cast<zeroIFDList_t*>(plist))->nodeCnt;
      break;
    case IFD_TYPE_EXIFIFD:
      plist = ifdExifListGet();
      cnt = (reinterpret_cast<exifIFDList_t*>(plist))->nodeCnt;
      break;
    case IFD_TYPE_GPSIFD:
      plist = ifdGpsListGet();
      cnt = (reinterpret_cast<gpsIFDList_t*>(plist))->nodeCnt;
      break;
    case IFD_TYPE_FIRSTIFD:
      plist = ifdFirstListGet();
      cnt = (reinterpret_cast<firstIFDList_t*>(plist))->nodeCnt;
      break;
    case IFD_TYPE_ITOPIFD:
      plist = ifdItopListGet();
      cnt = (reinterpret_cast<itopIFDList_t*>(plist))->nodeCnt;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  return cnt;
}
