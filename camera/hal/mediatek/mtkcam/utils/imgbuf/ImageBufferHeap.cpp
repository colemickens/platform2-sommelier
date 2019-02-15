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

#define LOG_TAG "MtkCam/Cam1Heap"
//
#include "BaseImageBufferHeap.h"
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <memory>
#include <vector>
#include <inttypes.h>
//
using NSCam::NSImageBufferHeap::BaseImageBufferHeap;
//

/******************************************************************************
 *
 ******************************************************************************/
#define GET_BUF_VA(plane, va, index) (plane >= (index + 1)) ? va : 0
#define GET_BUF_ID(plane, memID, index) (plane >= (index + 1)) ? memID : 0

/******************************************************************************
 *  Image Buffer Heap (Camera1).
 ******************************************************************************/
class ImageBufferHeapImpl
    : public NSCam::ImageBufferHeap,
      public NSCam::NSImageBufferHeap::BaseImageBufferHeap {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Creation.
  static std::shared_ptr<ImageBufferHeapImpl> create(
      char const* szCallerName,
      ImageBufferHeapImpl::ImgParam_t const& rImgParam,
      NSCam::PortBufInfo_v1 const& rPortBufInfo,
      MBOOL const enableLog = MTRUE);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  BaseImageBufferHeap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  virtual char const* impGetMagicName() const { return magicName(); }
  virtual HeapInfoVect_t const& impGetHeapInfo() const { return mvHeapInfo; }

  virtual MBOOL impInit(BufInfoVect_t const& rvBufInfo);
  virtual MBOOL impUninit();
  virtual MBOOL impReconfig(BufInfoVect_t const& rvBufInfo) { return MFALSE; }

 public:  ////
  virtual MBOOL impLockBuf(char const* szCallerName,
                           MINT usage,
                           BufInfoVect_t const& rvBufInfo);
  virtual MBOOL impUnlockBuf(char const* szCallerName,
                             MINT usage,
                             BufInfoVect_t const& rvBufInfo);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  virtual MBOOL doMapPhyAddr(char const* szCallerName,
                             HeapInfo const& rHeapInfo,
                             BufInfo const& rBufInfo);
  virtual MBOOL doUnmapPhyAddr(char const* szCallerName,
                               HeapInfo const& rHeapInfo,
                               BufInfo const& rBufInfo);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Destructor/Constructors.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~ImageBufferHeapImpl();
  ImageBufferHeapImpl(char const* szCallerName,
                      ImageBufferHeapImpl::ImgParam_t const& rImgParam,
                      NSCam::PortBufInfo_v1 const& rPortBufInfo);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Info to Allocate.
  size_t mBufStridesInBytesToAlloc[3];  // buffer strides in bytes.

 protected:  ////                    Info of Allocated Result.
  NSCam::PortBufInfo_v1 mPortBufInfo;  //
  HeapInfoVect_t mvHeapInfo;           //
  BufInfoVect_t mvBufInfo;             //
};

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::ImageBufferHeap> NSCam::ImageBufferHeap::create(
    char const* szCallerName,
    ImageBufferHeapImpl::ImgParam_t const& rImgParam,
    NSCam::PortBufInfo_v1 const& rPortBufInfo,
    MBOOL const enableLog) {
  return ImageBufferHeapImpl::create(szCallerName, rImgParam, rPortBufInfo,
                                     enableLog);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<ImageBufferHeapImpl> ImageBufferHeapImpl::create(
    char const* szCallerName,
    ImageBufferHeapImpl::ImgParam_t const& rImgParam,
    NSCam::PortBufInfo_v1 const& rPortBufInfo,
    MBOOL const enableLog) {
  MUINT const planeCount =
      NSCam::Utils::Format::queryPlaneCount(rImgParam.imgFormat);
  CAM_LOGD("format %#x, planeCount %d", rImgParam.imgFormat, planeCount);
  //
  std::shared_ptr<ImageBufferHeapImpl> pHeap =
      std::make_shared<ImageBufferHeapImpl>(szCallerName, rImgParam,
                                            rPortBufInfo);
  if (!pHeap) {
    CAM_LOGE("Fail to new");
    return NULL;
  }
  //
  if (!pHeap->onCreate(std::dynamic_pointer_cast<BaseImageBufferHeap>(pHeap),
                       rImgParam.imgSize, rImgParam.imgFormat,
                       rImgParam.bufSize, enableLog)) {
    CAM_LOGE("onCreate");
    // delete pHeap;
    return NULL;
  }
  //
  return pHeap;
}

/******************************************************************************
 *
 ******************************************************************************/
ImageBufferHeapImpl::ImageBufferHeapImpl(
    char const* szCallerName,
    ImageBufferHeapImpl::ImgParam_t const& rImgParam,
    NSCam::PortBufInfo_v1 const& rPortBufInfo)
    : BaseImageBufferHeap(szCallerName), mPortBufInfo(rPortBufInfo) {
  ::memcpy(mBufStridesInBytesToAlloc, rImgParam.bufStridesInBytes,
           sizeof(mBufStridesInBytesToAlloc));
}

ImageBufferHeapImpl::~ImageBufferHeapImpl() {
  impUninit();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageBufferHeapImpl::impInit(BufInfoVect_t const& rvBufInfo) {
  MBOOL ret = MFALSE;
  MUINT32 planesSizeInBytes = 0;  // for calculating n-plane va
  //
  MY_LOGD_IF(getLogCond(),
             "continuos(%d) plane(%zu), memID(0x%x/0x%x/0x%x), va(0x%" PRIxPTR
             "/0x%" PRIxPTR "/0x%" PRIxPTR ")",
             mPortBufInfo.continuos, getPlaneCount(),
             GET_BUF_ID(getPlaneCount(), mPortBufInfo.memID[0], 0),
             (mPortBufInfo.continuos)
                 ? 0
                 : GET_BUF_ID(getPlaneCount(), mPortBufInfo.memID[1], 1),
             (mPortBufInfo.continuos)
                 ? 0
                 : GET_BUF_ID(getPlaneCount(), mPortBufInfo.memID[2], 2),
             GET_BUF_VA(getPlaneCount(), mPortBufInfo.virtAddr[0], 0),
             (mPortBufInfo.continuos)
                 ? 0
                 : GET_BUF_VA(getPlaneCount(), mPortBufInfo.virtAddr[1], 1),
             (mPortBufInfo.continuos)
                 ? 0
                 : GET_BUF_VA(getPlaneCount(), mPortBufInfo.virtAddr[2], 2));
  mvHeapInfo.reserve(getPlaneCount());
  mvBufInfo.reserve(getPlaneCount());
  for (MUINT32 i = 0; i < getPlaneCount(); i++) {
    if (!helpCheckBufStrides(i, mBufStridesInBytesToAlloc[i])) {
      goto lbExit;
    }
    //
    {
      std::shared_ptr<HeapInfo> pHeapInfo = std::make_shared<HeapInfo>();
      mvHeapInfo.push_back(pHeapInfo);
      pHeapInfo->heapID = (MTRUE == mPortBufInfo.continuos)
                              ? mPortBufInfo.memID[0]
                              : mPortBufInfo.memID[i];
      //
      std::shared_ptr<BufInfo> pBufInfo = std::make_shared<BufInfo>();
      mvBufInfo.push_back(pBufInfo);
      pBufInfo->stridesInBytes = mBufStridesInBytesToAlloc[i];
      pBufInfo->sizeInBytes =
          helpQueryBufSizeInBytes(i, mBufStridesInBytesToAlloc[i]);
      pBufInfo->va = (MTRUE == mPortBufInfo.continuos)
                         ? mPortBufInfo.virtAddr[0] + planesSizeInBytes
                         : mPortBufInfo.virtAddr[i];
      //
      planesSizeInBytes += pBufInfo->sizeInBytes;
      //
      rvBufInfo[i]->stridesInBytes = pBufInfo->stridesInBytes;
      rvBufInfo[i]->sizeInBytes = pBufInfo->sizeInBytes;
    }
  }
  //
  ret = MTRUE;
lbExit:
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageBufferHeapImpl::impUninit() {
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageBufferHeapImpl::impLockBuf(char const* szCallerName,
                                MINT usage,
                                BufInfoVect_t const& rvBufInfo) {
  MBOOL ret = MFALSE;
  //
  for (MUINT32 i = 0; i < rvBufInfo.size(); i++) {
    std::shared_ptr<HeapInfo> pHeapInfo = mvHeapInfo[i];
    std::shared_ptr<BufInfo> pBufInfo = rvBufInfo[i];
    //
    //  SW Access.
    pBufInfo->va =
        (0 != (usage & NSCam::eBUFFER_USAGE_SW_MASK)) ? mvBufInfo[i]->va : 0;
  }
  //
  ret = MTRUE;

  if (!ret) {
    impUnlockBuf(szCallerName, usage, rvBufInfo);
  }

  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageBufferHeapImpl::impUnlockBuf(char const* szCallerName,
                                  MINT usage,
                                  BufInfoVect_t const& rvBufInfo) {
  //
  for (MUINT32 i = 0; i < rvBufInfo.size(); i++) {
    std::shared_ptr<HeapInfo> pHeapInfo = mvHeapInfo[i];
    std::shared_ptr<BufInfo> pBufInfo = rvBufInfo[i];
    //
    //  HW Access.
    if (0 != (usage & NSCam::eBUFFER_USAGE_HW_MASK)) {
      if (0 != pBufInfo->pa) {
        doUnmapPhyAddr(szCallerName, *pHeapInfo, *pBufInfo);
        pBufInfo->pa = 0;
      } else {
        MY_LOGD("%s@ skip PA=0 at %d-th plane", szCallerName, i);
      }
    }
    //
    //  SW Access.
    if (0 != (usage & NSCam::eBUFFER_USAGE_SW_MASK)) {
      if (0 != pBufInfo->va) {
        pBufInfo->va = 0;
      } else {
        MY_LOGD("%s@ skip VA=0 at %d-th plane", szCallerName, i);
      }
    }
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageBufferHeapImpl::doMapPhyAddr(char const* szCallerName,
                                  HeapInfo const& rHeapInfo,
                                  BufInfo const& rBufInfo) {
  return MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ImageBufferHeapImpl::doUnmapPhyAddr(char const* szCallerName,
                                    HeapInfo const& rHeapInfo,
                                    BufInfo const& rBufInfo) {
  return MFALSE;
}
