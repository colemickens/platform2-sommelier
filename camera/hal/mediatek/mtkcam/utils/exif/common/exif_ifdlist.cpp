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
#include <mtkcam/utils/exif/common/exif_errcode.h>
#include <mtkcam/utils/exif/common/exif_log.h>
#include <mtkcam/utils/exif/common/exif_type.h>
//
#include "Exif.h"
#include "IBaseExif.h"

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListInit() {
  unsigned int err = EXIF_NO_ERROR;
  unsigned int i = 0;

  if (mpzeroList == NULL) {
    mpzeroList = new zeroIFDList_t();
    for (i = 0; i < IFD_MAX_ZEROIFD_CNT; ++i) {
      mpzeroList->ifdNodePool[i].ifd.tag = INVALID_TAG;
    }
  }

  if (mpexifList == NULL) {
    mpexifList = new exifIFDList_t();
    for (i = 0; i < IFD_MAX_EXIFIFD_CNT; ++i) {
      mpexifList->ifdNodePool[i].ifd.tag = INVALID_TAG;
    }
  }

  if (mpgpsList == NULL) {
    mpgpsList = new gpsIFDList_t();
    for (i = 0; i < IFD_MAX_GPSIFD_CNT; ++i) {
      mpgpsList->ifdNodePool[i].ifd.tag = INVALID_TAG;
    }
  }

  if (mpfirstList == NULL) {
    mpfirstList = new firstIFDList_t();
    for (i = 0; i < IFD_MAX_FIRSTIFD_CNT; ++i) {
      mpfirstList->ifdNodePool[i].ifd.tag = INVALID_TAG;
    }
  }

  if (mpitopList == NULL) {
    mpitopList = new itopIFDList_t();
    for (i = 0; i < IFD_MAX_ITOPIFD_CNT; ++i) {
      mpitopList->ifdNodePool[i].ifd.tag = INVALID_TAG;
    }
  }

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListUninit() {
  unsigned int err = EXIF_NO_ERROR;

  if (mpzeroList != NULL) {
    delete mpzeroList;
    mpzeroList = NULL;
  }

  if (mpexifList != NULL) {
    delete mpexifList;
    mpexifList = NULL;
  }

  if (mpgpsList != NULL) {
    delete mpgpsList;
    mpgpsList = NULL;
  }

  if (mpfirstList != NULL) {
    delete mpfirstList;
    mpfirstList = NULL;
  }

  if (mpitopList != NULL) {
    delete mpitopList;
    mpitopList = NULL;
  }

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/
ifdNode_t* ExifUtils::ifdListNodeAlloc(unsigned int ifdType) {
  unsigned int err = EXIF_NO_ERROR;

  ifdNode_t* pnode = NULL;
  unsigned int maxCnt = 0, idx;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      pnode = mpzeroList->ifdNodePool;
      maxCnt = IFD_MAX_ZEROIFD_CNT;
      break;
    case IFD_TYPE_EXIFIFD:
      pnode = mpexifList->ifdNodePool;
      maxCnt = IFD_MAX_EXIFIFD_CNT;
      break;
    case IFD_TYPE_GPSIFD:
      pnode = mpgpsList->ifdNodePool;
      maxCnt = IFD_MAX_GPSIFD_CNT;
      break;
    case IFD_TYPE_FIRSTIFD:
      pnode = mpfirstList->ifdNodePool;
      maxCnt = IFD_MAX_FIRSTIFD_CNT;
      break;
    case IFD_TYPE_ITOPIFD:
      pnode = mpitopList->ifdNodePool;
      maxCnt = IFD_MAX_ITOPIFD_CNT;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  /* find a empty node */
  if (!err) {
    idx = 0;
    while (pnode->ifd.tag != INVALID_TAG && idx < maxCnt) {
      idx++;
      pnode++;
    }
  }

  exifErrPrint((unsigned char*)"ifdListNodeAlloc", err);

  return pnode;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListNodeInsert(unsigned int ifdType,
                                          ifdNode_t* pnode,
                                          void* pdata) {
  if (pnode == NULL) {
    MEXIF_LOGE("pnode is NULL");
    return LIBEXIF_IFD_ERR0001;
  }

  unsigned int err = EXIF_NO_ERROR;
  ifdNode_t* pheadNode = 0;
  ifdNode_t* pcurNode;
  ifdNode_t* pprevNode;

  unsigned char* pvalbuf = 0;
  unsigned int size;
  unsigned int* pbufPos = 0;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      pheadNode = mpzeroList->pheadNode;
      mpzeroList->nodeCnt++;
      pvalbuf = mpzeroList->valBuf;
      pbufPos = &mpzeroList->valBufPos;
      break;
    case IFD_TYPE_EXIFIFD:
      pheadNode = mpexifList->pheadNode;
      mpexifList->nodeCnt++;
      pvalbuf = mpexifList->valBuf;
      pbufPos = &mpexifList->valBufPos;
      break;
    case IFD_TYPE_GPSIFD:
      pheadNode = mpgpsList->pheadNode;
      mpgpsList->nodeCnt++;
      pvalbuf = mpgpsList->valBuf;
      pbufPos = &mpgpsList->valBufPos;
      break;
    case IFD_TYPE_FIRSTIFD:
      pheadNode = mpfirstList->pheadNode;
      mpfirstList->nodeCnt++;
      pvalbuf = mpfirstList->valBuf;
      pbufPos = &mpfirstList->valBufPos;
      break;
    case IFD_TYPE_ITOPIFD:
      pheadNode = mpitopList->pheadNode;
      mpitopList->nodeCnt++;
      pvalbuf = mpitopList->valBuf;
      pbufPos = &mpitopList->valBufPos;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  if (!err) {
    pcurNode = pheadNode;

    if (pcurNode == NULL) { /* the only node */
      pheadNode = pnode;
      pnode->next = NULL;
    } else {
      pprevNode = NULL;

      while (pcurNode && (pcurNode->ifd.tag <
                          pnode->ifd.tag)) { /* what if tag no are equal */
        pprevNode = pcurNode;
        pcurNode = pcurNode->next;
      }

      if (pcurNode == NULL) { /* this is biggest tag number, append to tail */
        pprevNode->next = pnode;
        pnode->next = NULL;
      } else {                       /* insert the node, sort by number */
        if (pcurNode == pheadNode) { /* this is smallest tag number */
          pnode->next = pheadNode;
          pheadNode = pnode;
        } else { /* insert to middle */
          pprevNode->next = pnode;
          pnode->next = pcurNode;
        }
      }
    }

    if (pnode && pdata) {
      size = exifIFDValueSizeof(pnode->ifd.type, pnode->ifd.count);
      if (size <= 4) {
        memcpy(&pnode->ifd.valoff, pdata, size);
      } else {
        memcpy(pvalbuf + *pbufPos, pdata, size);
        pnode->ifd.valoff = *pbufPos;
        *pbufPos += size;
      }
    }

    err = ifdListHeadNodeSet(ifdType, pheadNode);
  }

  exifErrPrint((unsigned char*)"ifdListNodeInsert", err);

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListNodeModify(uint16_t ifdType,
                                          uint16_t tagId,
                                          void* pdata) {
  unsigned int err = EXIF_NO_ERROR;

  unsigned int size = 0;
  MUINTPTR bufAddr = 0;
  ifdNode_t* pnode = 0;

  err = ifdListNodeInfoGet(ifdType, tagId, &pnode, &bufAddr);
  if (err != EXIF_NO_ERROR) {
    MEXIF_LOGE("err(0x%x)  ifdType/tagId: 0x%x/0x%x", err, ifdType, tagId);
  }
  if ((!err) && (pnode->ifd.tag != INVALID_TAG)) {
    size = exifIFDValueSizeof(pnode->ifd.type, pnode->ifd.count);
    memcpy((unsigned char*)bufAddr, (unsigned char*)pdata, size);
  }

  exifErrPrint((unsigned char*)"ifdListNodeModify", err);

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListNodeDelete(unsigned int ifdType,
                                          uint16_t tagId) {
  unsigned int err = EXIF_NO_ERROR;
  ifdNode_t* pheadNode = 0;
  ifdNode_t* pcurNode;
  ifdNode_t* pprevNode = 0;

  pheadNode = idfListHeadNodeGet(ifdType);

  if (pheadNode) {
    pcurNode = pheadNode;
    pprevNode = NULL;

    while (pcurNode && (pcurNode->ifd.tag != tagId)) {
      pprevNode = pcurNode; /* what if tag no are equal */
      pcurNode = pcurNode->next;
    }

    if (pcurNode != NULL) { /* node found and delete it */
      if (pprevNode) {      /* not head node */
        pprevNode->next = pcurNode->next;
        pcurNode->next = 0;
        switch (ifdType) {
          case IFD_TYPE_ZEROIFD:
            mpzeroList->nodeCnt--;
            break;
          case IFD_TYPE_EXIFIFD:
            mpexifList->nodeCnt--;
            break;
          case IFD_TYPE_GPSIFD:
            mpgpsList->nodeCnt--;
            break;
          case IFD_TYPE_FIRSTIFD:
            mpfirstList->nodeCnt--;
            break;
          case IFD_TYPE_ITOPIFD:
            mpitopList->nodeCnt--;
            break;
        }
      } else { /* head node */
        pheadNode = pcurNode->next;
        err = ifdListHeadNodeSet(ifdType, pheadNode);
      }
      memset(pcurNode, 0x00, sizeof(ifdNode_t)); /* clear node content */
    }
  }

  exifErrPrint((unsigned char*)"ifdListNodeDelete", err);

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListNodeInfoGet(uint16_t ifdType,
                                           uint16_t tagId,
                                           ifdNode_t** pnode,
                                           MUINTPTR* pbufAddr) {
  unsigned int err = EXIF_NO_ERROR;
  ifdNode_t* pcurNode = 0;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      pcurNode = mpzeroList->pheadNode;
      break;
    case IFD_TYPE_EXIFIFD:
      pcurNode = mpexifList->pheadNode;
      break;
    case IFD_TYPE_GPSIFD:
      pcurNode = mpgpsList->pheadNode;
      break;
    case IFD_TYPE_FIRSTIFD:
      pcurNode = mpfirstList->pheadNode;
      break;
    case IFD_TYPE_ITOPIFD:
      pcurNode = mpitopList->pheadNode;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  if (!err) {
    while (pcurNode != NULL && (pcurNode->ifd.tag != tagId)) {
      pcurNode = pcurNode->next;
    }

    if (pcurNode) {
      *pbufAddr = (MUINTPTR)(exifHdrTmplAddrGet() + pcurNode->ifd.valoff);
      *pnode = pcurNode;
    } else {
      err = LIBEXIF_IFD_ERR0003;
    }
  }

  exifErrPrint((unsigned char*)"ifdListNodeInfoGet", err);

  return err;
}

/*******************************************************************************
 *
 ******************************************************************************/
ifdNode_t* ExifUtils::idfListHeadNodeGet(unsigned int ifdType) {
  unsigned int err = EXIF_NO_ERROR;
  ifdNode_t* pnode = 0;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      pnode = mpzeroList->pheadNode;
      break;
    case IFD_TYPE_EXIFIFD:
      pnode = mpexifList->pheadNode;
      break;
    case IFD_TYPE_GPSIFD:
      pnode = mpgpsList->pheadNode;
      break;
    case IFD_TYPE_FIRSTIFD:
      pnode = mpfirstList->pheadNode;
      break;
    case IFD_TYPE_ITOPIFD:
      pnode = mpitopList->pheadNode;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  exifErrPrint((unsigned char*)"idfListHeadNodeGet", err);

  return pnode;
}

/*******************************************************************************
 *
 ******************************************************************************/
unsigned int ExifUtils::ifdListHeadNodeSet(unsigned int ifdType,
                                           ifdNode_t* pheadNode) {
  unsigned int err = EXIF_NO_ERROR;

  switch (ifdType) {
    case IFD_TYPE_ZEROIFD:
      mpzeroList->pheadNode = pheadNode;
      break;
    case IFD_TYPE_EXIFIFD:
      mpexifList->pheadNode = pheadNode;
      break;
    case IFD_TYPE_GPSIFD:
      mpgpsList->pheadNode = pheadNode;
      break;
    case IFD_TYPE_FIRSTIFD:
      mpfirstList->pheadNode = pheadNode;
      break;
    case IFD_TYPE_ITOPIFD:
      mpitopList->pheadNode = pheadNode;
      break;
    default:
      err = LIBEXIF_IFD_ERR0001;
      break;
  }

  exifErrPrint((unsigned char*)"ifdListHeadNodeSet", err);

  return err;
}
