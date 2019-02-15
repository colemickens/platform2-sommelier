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

#define LOG_TAG "MtkCam/DummyHeap"

#include "BaseImageBufferHeap.h"
#include <atomic>
#include <memory>
#include <mtkcam/utils/imgbuf/IDummyImageBufferHeap.h>

using NSCam::IDummyImageBufferHeap;
using NSCam::PortBufInfo_dummy;
using NSCam::NSImageBufferHeap::BaseImageBufferHeap;
/******************************************************************************
 *  Image Buffer Heap (Dummy).
 ******************************************************************************/
namespace {
class DummyImageBufferHeap
    : public IDummyImageBufferHeap,
      public NSCam::NSImageBufferHeap::BaseImageBufferHeap {
  friend class IDummyImageBufferHeap;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IImageBufferHeap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MBOOL syncCache(NSCam::eCacheCtrl const ctrl) override;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  BaseImageBufferHeap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  char const* impGetMagicName() const override { return magicName(); }

  void* impGetMagicInstance() { return reinterpret_cast<void*>(this); }

  HeapInfoVect_t const& impGetHeapInfo() const override { return mvHeapInfo; }

  MBOOL impInit(BufInfoVect_t const& rvBufInfo) override;
  MBOOL impUninit() override;
  MBOOL impReconfig(BufInfoVect_t const& rvBufInfo) override { return MFALSE; }

 public:  ////
  MBOOL impLockBuf(char const* szCallerName,
                   MINT usage,
                   BufInfoVect_t const& rvBufInfo) override;
  MBOOL impUnlockBuf(char const* szCallerName,
                     MINT usage,
                     BufInfoVect_t const& rvBufInfo) override;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  virtual ~DummyImageBufferHeap() {}
  DummyImageBufferHeap(char const* szCallerName,
                       ImgParam_t const& rImgParam,
                       PortBufInfo_dummy const& rPortBufInfo);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Info to Allocate.
  size_t mBufStridesInBytesToAlloc[3];  // buffer strides in bytes.

 protected:  ////                    Info of Allocated Result.
  PortBufInfo_dummy mPortBufInfo;  //
  HeapInfoVect_t mvHeapInfo;       //
  BufInfoVect_t mvBufInfo;         //
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
#define GET_BUF_VA(plane, va, index) (plane >= (index + 1)) ? va : 0
#define GET_BUF_ID(plane, memID, index) (plane >= (index + 1)) ? memID : 0

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IDummyImageBufferHeap> IDummyImageBufferHeap::create(
    char const* szCallerName,
    ImgParam_t const& rImgParam,
    PortBufInfo_dummy const& rPortBufInfo,
    MBOOL const enableLog) {
  std::shared_ptr<DummyImageBufferHeap> pHeap(
      new DummyImageBufferHeap(szCallerName, rImgParam, rPortBufInfo));

  if (!pHeap) {
    CAM_LOGE("Fail to new");
    return nullptr;
  }
  //
  if (!pHeap->onCreate(rImgParam.imgSize, rImgParam.imgFormat,
                       rImgParam.bufSize, enableLog)) {
    CAM_LOGE("onCreate");
    return nullptr;
  }
  //
  return pHeap;
}

/******************************************************************************
 *
 ******************************************************************************/
DummyImageBufferHeap::DummyImageBufferHeap(
    char const* szCallerName,
    ImgParam_t const& rImgParam,
    PortBufInfo_dummy const& rPortBufInfo)
    : BaseImageBufferHeap(szCallerName), mPortBufInfo(rPortBufInfo) {
  ::memcpy(mBufStridesInBytesToAlloc, rImgParam.bufStridesInBytes,
           sizeof(mBufStridesInBytesToAlloc));
}

MBOOL
DummyImageBufferHeap::syncCache(NSCam::eCacheCtrl const /* ctrl */) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
DummyImageBufferHeap::impInit(BufInfoVect_t const& rvBufInfo) {
  MBOOL ret = MFALSE;
  MUINT32 planesSizeInBytes = 0;  // for calculating n-plane va
  //
  CAM_LOGD_IF(getLogCond(),
              "plane(%zu), memID(0x%x), va(0x%" PRIxPTR "/0x%" PRIxPTR
              "/0x%" PRIxPTR ")",
              getPlaneCount(), mPortBufInfo.memID,
              GET_BUF_VA(getPlaneCount(), mPortBufInfo.virtAddr[0], 0),
              GET_BUF_VA(getPlaneCount(), mPortBufInfo.virtAddr[1], 1),
              GET_BUF_VA(getPlaneCount(), mPortBufInfo.virtAddr[2], 2));
  mvHeapInfo.reserve(getPlaneCount());
  mvBufInfo.reserve(getPlaneCount());
  for (MUINT32 i = 0; i < getPlaneCount(); i++) {
    if (!helpCheckBufStrides(i, mBufStridesInBytesToAlloc[i])) {
      CAM_LOGW("stride(size=%zu) of plane(%zu) is invalid.",
               mBufStridesInBytesToAlloc[i], i);

      goto lbExit;
    }
    //
    {
      std::shared_ptr<HeapInfo> pHeapInfo(new HeapInfo);
      mvHeapInfo.push_back(pHeapInfo);
      pHeapInfo->heapID = mPortBufInfo.memID;
      //
      std::shared_ptr<BufInfo> pBufInfo(new BufInfo);
      mvBufInfo.push_back(pBufInfo);
      pBufInfo->stridesInBytes = mBufStridesInBytesToAlloc[i];
      pBufInfo->sizeInBytes =
          helpQueryBufSizeInBytes(i, mBufStridesInBytesToAlloc[i]);
      pBufInfo->va = mPortBufInfo.virtAddr[i];
      pBufInfo->pa = mPortBufInfo.phyAddr[i];
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
DummyImageBufferHeap::impUninit() {
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
DummyImageBufferHeap::impLockBuf(char const* /*szCallerName*/,
                                 MINT usage,
                                 BufInfoVect_t const& rvBufInfo) {
  //
  for (MUINT32 i = 0; i < rvBufInfo.size(); i++) {
    std::shared_ptr<BufInfo> pBufInfo = rvBufInfo[i];
    //
    //  SW Access.
    pBufInfo->va =
        (0 != (usage & NSCam::eBUFFER_USAGE_SW_MASK)) ? mvBufInfo[i]->va : 0;
    //
    //  HW Access.
    pBufInfo->pa =
        (0 != (usage & NSCam::eBUFFER_USAGE_HW_MASK)) ? mvBufInfo[i]->pa : 0;
  }

  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
DummyImageBufferHeap::impUnlockBuf(char const* szCallerName,
                                   MINT usage,
                                   BufInfoVect_t const& rvBufInfo) {
  //
  for (MUINT32 i = 0; i < rvBufInfo.size(); i++) {
    std::shared_ptr<BufInfo> pBufInfo = rvBufInfo[i];
    //
    //  HW Access.
    if (0 != (usage & NSCam::eBUFFER_USAGE_HW_MASK)) {
      if (0 != pBufInfo->pa) {
        pBufInfo->pa = 0;
      } else {
        CAM_LOGI("%s@ skip PA=0 at %d-th plane", szCallerName, i);
      }
    }
    //
    //  SW Access.
    if (0 != (usage & NSCam::eBUFFER_USAGE_SW_MASK)) {
      if (0 != pBufInfo->va) {
        pBufInfo->va = 0;
      } else {
        CAM_LOGI("%s@ skip VA=0 at %d-th plane", szCallerName, i);
      }
    }
  }
  //
  return MTRUE;
}
