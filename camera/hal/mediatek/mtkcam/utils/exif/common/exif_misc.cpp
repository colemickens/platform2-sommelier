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
#include "./exif_log.h"
#include "./exif_type.h"
//

/*****************************************************************************
 *
 ******************************************************************************/
uint16_t ExifUtils::mySwap16(uint16_t x) {
  x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
  return x;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::mySwap32(unsigned int x) {
  x = (((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) |
       ((x & 0x0000ff00) << 8) | ((x & 0x000000ff) << 24));
  return x;
}

/*****************************************************************************
 *
 ******************************************************************************/
uint16_t ExifUtils::mySwap16ByOrder(uint16_t order, uint16_t x) {
  if (order == 0x4D4D) {
    x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
  }

  return x;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::mySwap32ByOrder(uint16_t order, unsigned int x) {
  if (order == 0x4D4D) {
    x = (((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) |
         ((x & 0x0000ff00) << 8) | ((x & 0x000000ff) << 24));
  }

  return x;
}

/*****************************************************************************
 *
 ******************************************************************************/
uint16_t ExifUtils::read16(void* psrc) {
  unsigned char* pdata;
  uint16_t ret;

  pdata = (unsigned char*)psrc;
  ret = (*(pdata + 1) << 8) + (*pdata);

  return ret;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::read32(void* psrc) {
  unsigned char* pdata;
  unsigned int ret;

  pdata = (unsigned char*)psrc;
  ret = (*(pdata + 3) << 24) + (*(pdata + 2) << 16) + (*(pdata + 1) << 8) +
        (*(pdata));

  return ret;
}

/*****************************************************************************
 *
 ******************************************************************************/
void ExifUtils::write16(void* pdst, uint16_t src) {
  unsigned char byte0, byte1;
  unsigned char* pdata;

  pdata = (unsigned char*)pdst;
  byte0 = (unsigned char)((src)&0x00ff);
  byte1 = (unsigned char)((src >> 8) & 0x00ff);
  *pdata = byte0;
  pdata++;
  *pdata = byte1;
}

/*****************************************************************************
 *
 ******************************************************************************/
void ExifUtils::write32(void* pdst, unsigned int src) {
  unsigned char byte0, byte1, byte2, byte3;
  unsigned char* pdata;

  pdata = (unsigned char*)pdst;
  byte0 = (unsigned char)((src)&0x000000ff);
  byte1 = (unsigned char)((src >> 8) & 0x000000ff);
  byte2 = (unsigned char)((src >> 16) & 0x000000ff);
  byte3 = (unsigned char)((src >> 24) & 0x000000ff);
  *pdata = byte0;
  pdata++;
  *pdata = byte1;
  pdata++;
  *pdata = byte2;
  pdata++;
  *pdata = byte3;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifMemcmp(unsigned char* pdst,
                                   unsigned char* psrc,
                                   unsigned int size) {
  while (size > 0) {
    if (*pdst != *psrc) {
      break;
    }
    pdst++;
    psrc++;
    size--;
  }

  return size;
}

/*****************************************************************************
 * without thumbnail
 ******************************************************************************/
size_t ExifUtils::exifApp1SizeGet() const {
  size_t size = (size_t)exifApp1Sizeof() + 2;  // 0xFFD8
  MEXIF_LOGI("app1 size(%zu)", size);
  return size;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifApp1Sizeof() const {
  unsigned int size =
      0x0a + sizeof(TiffHeader_t) + ifdListSizeof() + IFD_TYPE_ITOPIFD * 6;
  unsigned int reminder = (size + 2) % 128;

  if (reminder != 0) {
    size += 128 - reminder;
  }

  // This size excludes thumbnail size

  return size;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifIFDValueSizeof(uint16_t type, unsigned int count) {
  unsigned int size = 0;

  switch (type) {
    case IFD_DATATYPE_BYTE:
    case IFD_DATATYPE_UNDEFINED:
    case IFD_DATATYPE_ASCII:
      size = count;
      break;
    case IFD_DATATYPE_SHORT:
      size = count << 1;
      break;
    case IFD_DATATYPE_SLONG:
    case IFD_DATATYPE_LONG:
      size = count << 2;
      break;
    case IFD_DATATYPE_RATIONAL:
    case IFD_DATATYPE_SRATIONAL:
      size = count << 3;
      break;
    default:
      MEXIF_LOGE("Unsupport tag, type(%d), err = %x\n", type,
                 LIBEXIF_IFD_ERR0002);
      break;
  }

  return size;
}

/*****************************************************************************
 *
 ******************************************************************************/
void ExifUtils::exifErrPrint(unsigned char* pname, unsigned int err) {
  switch (err) {
    case EXIF_NO_ERROR:
      break;
    case LIBEXIF_FILE_ERR0001:
      MEXIF_LOGE("Error in %s() call, Unsupport file format, err  = %x\n",
                 pname, err);
      break;

    case LIBEXIF_APP1_ERR0001:
      MEXIF_LOGE("Error in %s() call, THumbnail not found, err = %x\n", pname,
                 err);
      break;
    case LIBEXIF_APP1_ERR0002:
      MEXIF_LOGE("Error in %s() call, TIFF header error, err  =%x\n", pname,
                 err);
      break;
    case LIBEXIF_DQT_ERR0001:
      MEXIF_LOGE("Error in %s() call, Too many DQT found, err  =%x\n", pname,
                 err);
      break;
#ifdef EXIF_WARNING_DEBUG
    case LIBEXIF_SOI_ERR0001:
      MEXIF_LOGE("Error in %s() call, SOI not found, err =%x\n", pname, err);
      break;
    case LIBEXIF_EOI_ERR0001:
      MEXIF_LOGE("Error in %s() call, EOI not found, err = %x\n", pname, err);
      break;
#endif
    case LIBEXIF_DQT_ERR0002:
      MEXIF_LOGE("Error in %s() call, DQT not found!, err = %x\n", pname, err);
      break;
    case LIBEXIF_DQT_ERR0003:
    case LIBEXIF_DHT_ERR0002:
    case LIBEXIF_DHT_ERR0004:
    case LIBEXIF_DHT_ERR0003:
    case LIBEXIF_DHT_ERR0005:
    case LIBEXIF_DHT_ERR0006:
      MEXIF_LOGE("Error in %s() call, Unsupport DHT found, err = %x\n", pname,
                 err);
      break;
    case LIBEXIF_SOF_ERR0001:
      MEXIF_LOGE("Error in %s() call, SOF not found, err = %x\n", pname, err);
      break;
    case LIBEXIF_SOF_ERR0002:
      MEXIF_LOGE("Error in %s() call, Support SOF length, err = %x\n", pname,
                 err);
      break;
    case LIBEXIF_SOF_ERR0003:
      MEXIF_LOGE("Error in %s() call, Unsupport data format, err = %x\n", pname,
                 err);
      break;
    case LIBEXIF_SOS_ERR0001:
      MEXIF_LOGE("Error in %s() call, SOS not found, err = %x\n", pname, err);
      break;
    case LIBEXIF_SOS_ERR0002:
      MEXIF_LOGE("Error in %s() call, Support SOS length, err = %x\n", pname,
                 err);
      break;
    case LIBEXIF_MISC_ERR0001:
      MEXIF_LOGE("Error in %s() call, Unknow Maker!, err = %x\n", pname, err);
      break;
    case LIBEXIF_MISC_ERR0002:
      MEXIF_LOGE("Error in %s() call, file size overflow!, err = %x\n", pname,
                 err);
      break;
    case LIBEXIF_IFD_ERR0001:
      MEXIF_LOGE(" Error in %s() call, not support IFD list!, err = %x\n",
                 pname, err);
      break;
    case LIBEXIF_IFD_ERR0002:
      MEXIF_LOGE("Error in %s() call, Unsupport tag!, err = %x\n", pname, err);
      break;
    case LIBEXIF_IFD_ERR0005:
      break;
    default:
      MEXIF_LOGE("Error in %s() call, Unknow err code!, err = %x\n", pname,
                 err);
  }
}
