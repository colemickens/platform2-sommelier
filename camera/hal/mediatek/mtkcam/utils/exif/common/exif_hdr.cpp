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

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifSOIWrite(unsigned char* pdata,
                                     unsigned int* pretSize) {
  unsigned int err = EXIF_NO_ERROR;

  *pdata++ = 0xFF;
  *pdata++ = SOI_MARKER;

  *pretSize = 0x02;

  return err;
}

/*****************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::exifAPP1Write(unsigned char* pdata,
                                      unsigned int* pretSize) {
  unsigned int err = EXIF_NO_ERROR;
  unsigned int tagIdx;
  unsigned int ifdValOffset = 0;
  unsigned int ifdSize, tiffHdrSize, size;
  unsigned int nextIFD0Offest;
  unsigned int nodeCnt;
  ifdNode_t* pnode;
  unsigned char* pstart;
  unsigned int value32 = 0;
  unsigned int exifNextPtr = 0;
  unsigned char buf[12];
  // unsigned short value16;
  TiffHeader_t tiff;

  *pretSize = 0;
  pstart = pdata;
  ifdSize = sizeof(IFD_t);
  tiffHdrSize = sizeof(TiffHeader_t);

  *pdata++ = 0xFF;
  *pdata++ = APP1_MARKER;
  *pretSize += 0x02;
  // write16(pdata, value16);  Fill the size later

  pdata += 2; /* App1 len */
  *pretSize += 2;

  strncpy(reinterpret_cast<char*>(pdata), "Exif", 5); /* "Exif"00 */
  pdata += 5;
  *pdata++ = 0x00; /* pad */
  *pretSize += 6;

  /* TIFF header */
  tiff.byteOrder = 0x4949;
  tiff.fixed = 0x002A;
  tiff.ifdOffset = tiffHdrSize;
  memcpy(pdata, &tiff, tiffHdrSize);

  pdata += tiffHdrSize;
  *pretSize += tiffHdrSize;
  ifdValOffset = tiffHdrSize; /* offset from the start of the TIFF header */

  /* find next IFD0 offset */
  nextIFD0Offest = tiffHdrSize;
  for (tagIdx = IFD_TYPE_ZEROIFD; tagIdx <= IFD_TYPE_GPSIFD; tagIdx++) {
    if ((tagIdx == IFD_TYPE_GPSIFD) && (!exifIsGpsOnFlag())) {
      continue;
    }
    nextIFD0Offest += (2 + ifdListNodeCntGet(tagIdx) * ifdSize +
                       ifdListValBufSizeof(tagIdx) + 4);
  }

  /* parsing all IFDs from 3 pre-defined IDF arrays */
  for (tagIdx = IFD_TYPE_ZEROIFD; tagIdx <= IFD_TYPE_ITOPIFD; tagIdx++) {
    nodeCnt = ifdListNodeCntGet(tagIdx);
    if (tagIdx == IFD_TYPE_EXIFIFD) {
      exifNextPtr = ifdValOffset;
    }

    if (tagIdx == IFD_TYPE_GPSIFD) {
      if (!exifIsGpsOnFlag()) {
        continue;
      }
      exifNextPtr = ifdValOffset;
    }

    if (tagIdx == IFD_TYPE_ITOPIFD) {
      exifNextPtr = ifdValOffset;
    }

    write16(pdata, nodeCnt); /* numnber of IFD interoperability */
    pdata += 2;
    *pretSize += 2;
    ifdValOffset += (nodeCnt * ifdSize + 2 + 4);
    /* fill IFD nodes to template header and save each entry's offset for quick
     * access */
    pnode = idfListHeadNodeGet(tagIdx);

    while (pnode) {
      // Special case for GPS
      memcpy(pdata, (unsigned char*)&pnode->ifd, ifdSize);

      /**((IFD_t*)pdata) = (IFD_t)pnode->ifd;*/
      if (exifIFDValueSizeof(pnode->ifd.type, pnode->ifd.count) >
          4) { /* record data to somewhere */
        write32((unsigned char*)&((reinterpret_cast<IFD_t*>(pdata))->valoff),
                pnode->ifd.valoff + ifdValOffset);
        value32 = read32(
            (unsigned char*)&((reinterpret_cast<IFD_t*>(pdata))->valoff));
        write32((unsigned char*)&pnode->ifd.valoff, value32 + 0x0c);
      } else {
        write32((unsigned char*)&pnode->ifd.valoff, *pretSize + ifdSize - 2);
      }

      pdata += ifdSize;
      *pretSize += ifdSize;
      pnode = pnode->next; /* pointer to next IFD */
    }

    if (tagIdx == IFD_TYPE_ZEROIFD) {
      // this address will be filled with next
      // ifd0 offset
      write32(pdata, nextIFD0Offest);
    } else {
      write32(pdata, 0);
    }

    pdata += 4; /* offset */
    *pretSize += 4;

    /* copy value buffer */
    size = ifdListValBufSizeof(tagIdx);
    if (size) {
      memcpy(pdata, ifdListValBufGet(tagIdx), size);
      pdata += size; /* offset */
      ifdValOffset += size;
      *pretSize += size;
    }

    if (tagIdx == IFD_TYPE_EXIFIFD) {
      write32(buf, exifNextPtr);
      ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_EXIFPTR, buf);
    }

    if (tagIdx == IFD_TYPE_GPSIFD) {
      write32(buf, exifNextPtr);
      ifdListNodeModify(IFD_TYPE_ZEROIFD, IFD0_TAG_GPSINFO, buf);
    }

    if (tagIdx == IFD_TYPE_ITOPIFD) { /* update */
      write32(buf, exifNextPtr);
      ifdListNodeModify(IFD_TYPE_EXIFIFD, EXIF_TAG_ITOPIFDPTR, buf);
    }
  }

  exifErrPrint((unsigned char*)"exifAPP1Write", err);

  return err;
}
