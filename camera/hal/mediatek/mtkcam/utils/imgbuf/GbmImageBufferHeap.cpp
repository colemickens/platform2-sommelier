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

#define LOG_TAG "MtkCam/GrallocHeap"
//
#include "BaseImageBufferHeap.h"
#include <camera_buffer_handle.h>
#include <cros-camera/camera_buffer_manager.h>
#include <linux/videodev2.h>
#include <memory>
#include <mtkcam/utils/imgbuf/IGbmImageBufferHeap.h>
#include <vector>

using NSCam::IGbmImageBufferHeap;
using NSCam::NSImageBufferHeap::BaseImageBufferHeap;
/******************************************************************************
 *  Image Buffer Heap (Gralloc).
 ******************************************************************************/
namespace {
class GbmImageBufferHeap
    : public IGbmImageBufferHeap,
      public NSCam::NSImageBufferHeap::BaseImageBufferHeap {
  friend class IGbmImageBufferHeap;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IGbmImageBufferHeap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Accessors.
  virtual void* getHWBuffer() { return reinterpret_cast<void*>(this); }
  virtual buffer_handle_t getBufferHandle() const { return mpHwBuffer; }
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
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  typedef std::vector<std::shared_ptr<BufInfo> > MyBufInfoVect_t;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  virtual MVOID doDeallocGB();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Destructor/Constructors.
  virtual ~GbmImageBufferHeap();
  GbmImageBufferHeap(char const* szCallerName,
                     AllocImgParam_t const& rImgParam);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Info to Allocate.
  size_t mImgFormat;  // image format.
  MSize mImgSize;     // image size.

 protected:                   ////                    Info of Allocated Result.
  HeapInfoVect_t mvHeapInfo;  //
  MyBufInfoVect_t mvBufInfo;  //
  buffer_handle_t mpHwBuffer;
  size_t mBufStridesInBytesToAlloc[3];   // buffer strides in bytes.
  size_t mBufBoundaryInBytesToAlloc[3];  // the address will be a multiple of
                                         // boundary in bytes, which must be a
                                         // power of two.
  size_t mBufsize;

 private:
  cros::CameraBufferManager* mGbmBufferManager;
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IGbmImageBufferHeap> IGbmImageBufferHeap::create(
    char const* szCallerName,
    AllocImgParam_t const& rImgParam,
    MBOOL const enableLog) {
  auto pHeap = std::make_shared<GbmImageBufferHeap>(szCallerName, rImgParam);
  if (!pHeap) {
    CAM_LOGE("Fail to new");
    return NULL;
  }
  //
  if (!pHeap->onCreate(std::dynamic_pointer_cast<BaseImageBufferHeap>(pHeap),
                       rImgParam.imgSize, rImgParam.imgFormat,
                       rImgParam.bufSize, enableLog)) {
    CAM_LOGE("onCreate fail");
    return nullptr;
  }
  //
  return pHeap;
}

/******************************************************************************
 *
 ******************************************************************************/
GbmImageBufferHeap::GbmImageBufferHeap(char const* szCallerName,
                                       AllocImgParam_t const& rImgParam)
    : BaseImageBufferHeap(szCallerName),
      mImgFormat(rImgParam.imgFormat),
      mImgSize(rImgParam.imgSize),
      mpHwBuffer(nullptr),
      mBufsize(0),
      mGbmBufferManager(nullptr) {
  ::memcpy(mBufStridesInBytesToAlloc, rImgParam.bufStridesInBytes,
           sizeof(mBufStridesInBytesToAlloc));
  ::memcpy(mBufBoundaryInBytesToAlloc, rImgParam.bufBoundaryInBytes,
           sizeof(mBufBoundaryInBytesToAlloc));
}

GbmImageBufferHeap::~GbmImageBufferHeap() {
  impUninit();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GbmImageBufferHeap::impInit(BufInfoVect_t const& rvBufInfo) {
  bool ret = MFALSE;
  uint32_t stride = 0;
  size_t allocateSize = 0;
  int err;

  MY_LOGD("[w,h]=[%d,%d],format=%x", mImgSize.w, mImgSize.h, mImgFormat);
  //  Allocate memory and setup mBufHeapInfo & rBufHeapInfo.
  mGbmBufferManager = cros::CameraBufferManager::GetInstance();
  if (mGbmBufferManager == nullptr) {
    MY_LOGE("GetInstance failed!");
    return false;
  }

  for (int i = 0; i < getPlaneCount(); i++) {
    allocateSize += helpQueryBufSizeInBytes(i, mBufStridesInBytesToAlloc[i]);
  }
  MY_LOGD("allocateSize = %zu", allocateSize);

  mvHeapInfo.reserve(getPlaneCount());
  mvBufInfo.reserve(getPlaneCount());

  for (int i = 0; i < getPlaneCount(); i++) {
    auto pHeapInfo = std::make_shared<HeapInfo>();
    mvHeapInfo.push_back(pHeapInfo);

    auto pBufInfo = std::make_shared<BufInfo>();
    mvBufInfo.push_back(pBufInfo);
  }

  if (mImgFormat == eImgFmt_NV12) {
    uint32_t halformat = HAL_PIXEL_FORMAT_YCbCr_420_888;
    err = mGbmBufferManager->Allocate(mImgSize.w, mImgSize.h, halformat,
                                      GRALLOC_USAGE_HW_CAMERA_WRITE,
                                      cros::GRALLOC, &mpHwBuffer, &stride);
    if (err != 0) {
      MY_LOGE("Allocate handle failed! %d", ret);
      return false;
    }

    for (int i = 0; i < getPlaneCount(); i++) {
      mvHeapInfo[i]->heapID = mpHwBuffer->data[i];
      mBufStridesInBytesToAlloc[i] =
          mGbmBufferManager->GetPlaneStride(mpHwBuffer, i);
      mvBufInfo[i]->stridesInBytes =
          mGbmBufferManager->GetPlaneStride(mpHwBuffer, i);
      mvBufInfo[i]->sizeInBytes =
          mGbmBufferManager->GetPlaneSize(mpHwBuffer, i);
      mvBufInfo[i]->offsetInBytes =
          mGbmBufferManager->GetPlaneOffset(mpHwBuffer, i);

      mBufsize += mvBufInfo[i]->sizeInBytes;
    }
  } else {
    err = mGbmBufferManager->Allocate(allocateSize, 1, HAL_PIXEL_FORMAT_BLOB,
                                      GRALLOC_USAGE_HW_CAMERA_WRITE,
                                      cros::GRALLOC, &mpHwBuffer, &stride);
    if (err != 0) {
      MY_LOGE("Allocate handle failed! %d", ret);
      return false;
    }

    off_t offset = 0;
    for (int i = 0; i < getPlaneCount(); i++) {
      mvHeapInfo[i]->heapID = mpHwBuffer->data[0];
      mvBufInfo[i]->stridesInBytes = mBufStridesInBytesToAlloc[i];
      mvBufInfo[i]->sizeInBytes =
          helpQueryBufSizeInBytes(i, mBufStridesInBytesToAlloc[i]);
      mvBufInfo[i]->offsetInBytes = offset;
      offset += mvBufInfo[i]->sizeInBytes;

      mBufsize += mvBufInfo[i]->sizeInBytes;
    }
  }

  for (int i = 0; i < getPlaneCount(); i++) {
    rvBufInfo[i]->stridesInBytes = mvBufInfo[i]->stridesInBytes;
    rvBufInfo[i]->sizeInBytes = mvBufInfo[i]->sizeInBytes;
    rvBufInfo[i]->offsetInBytes = mvBufInfo[i]->offsetInBytes;
  }

  MY_LOGD("mBufsize = %zu", mBufsize);
  ret = MTRUE;

  if (!ret) {
    doDeallocGB();
    mvHeapInfo.clear();
    mvBufInfo.clear();
  }
  MY_LOGD_IF(getLogCond(), "- ret:%d", ret);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GbmImageBufferHeap::impUninit() {
  doDeallocGB();
  mvHeapInfo.clear();
  mvBufInfo.clear();
  //
  MY_LOGD_IF(getLogCond(), "-");
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
GbmImageBufferHeap::doDeallocGB() {
  if (mpHwBuffer != nullptr) {
    int ret = mGbmBufferManager->Free(mpHwBuffer);
    if (ret) {
      MY_LOGE("@%s: call Deregister fail, mHandle:%p, ret:%d", __FUNCTION__,
              mpHwBuffer, ret);
    }
    mpHwBuffer = nullptr;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GbmImageBufferHeap::impLockBuf(char const* szCallerName,
                               MINT usage,
                               BufInfoVect_t const& rvBufInfo) {
  int v4l2Fmt = mGbmBufferManager->GetV4L2PixelFormat(mpHwBuffer);
  uint32_t planeNum = mGbmBufferManager->GetNumPlanes(mpHwBuffer);
  int ret = 0;

  if (planeNum == 1) {
    void* data = nullptr;
    ret = (mImgFormat == HAL_PIXEL_FORMAT_BLOB)
              ? mGbmBufferManager->Lock(mpHwBuffer, 0, 0, 0,
                                        mImgSize.w * mImgSize.h, 1, &data)
              : mGbmBufferManager->Lock(mpHwBuffer, 0, 0, 0, mImgSize.w,
                                        mImgSize.h, &data);
    if (ret) {
      MY_LOGE("@%s: call Lock fail, mHandle:%p", __FUNCTION__, mpHwBuffer);
      return MFALSE;
    }
    //
    MINTPTR va = reinterpret_cast<MINTPTR>(data);
    for (size_t i = 0; i < mvBufInfo.size(); i++) {
      rvBufInfo[i]->va = va;
      va += mvBufInfo[i]->sizeInBytes;
    }
  } else if (planeNum > 1) {
    struct android_ycbcr ycbrData;
    ret = mGbmBufferManager->LockYCbCr(mpHwBuffer, 0, 0, 0, mImgSize.w,
                                       mImgSize.h, &ycbrData);
    if (ret) {
      MY_LOGE("@%s: call LockYCbCr fail, mHandle:%p", __FUNCTION__, mpHwBuffer);
      return MFALSE;
    }
    rvBufInfo[0]->va = reinterpret_cast<MINTPTR>(ycbrData.y);
    if (planeNum == 2) {
      switch (v4l2Fmt) {
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12M:
          rvBufInfo[1]->va = reinterpret_cast<MINTPTR>(ycbrData.cb);
          break;
        default:
          MY_LOGE("Unsupported semi-planar format: %s",
                  FormatToString(v4l2Fmt).c_str());
      }
    } else {  // num_planes == 3
      switch (v4l2Fmt) {
        case V4L2_PIX_FMT_YVU420:
        case V4L2_PIX_FMT_YVU420M:
          rvBufInfo[1]->va = reinterpret_cast<MINTPTR>(ycbrData.cr);
          rvBufInfo[2]->va = reinterpret_cast<MINTPTR>(ycbrData.cb);
          break;
        default:
          MY_LOGE("Unsupported planar format: %s",
                  FormatToString(v4l2Fmt).c_str());
      }
    }
  } else {
    MY_LOGE("ERROR @%s: planeNum is 0", __FUNCTION__);
    return MFALSE;
  }

  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GbmImageBufferHeap::impUnlockBuf(char const* szCallerName,
                                 MINT usage,
                                 BufInfoVect_t const& rvBufInfo) {
  for (MUINT32 i = 0; i < rvBufInfo.size(); i++) {
    std::shared_ptr<BufInfo> pBufInfo = rvBufInfo[i];
    //  SW Access.

    if (0 != pBufInfo->va) {
      pBufInfo->va = 0;
    } else {
      MY_LOGD("%s@ skip VA=0 at %d-th plane", szCallerName, i);
    }
  }

  int ret = mGbmBufferManager->Unlock(mpHwBuffer);
  if (ret) {
    MY_LOGE("@%s: call Unlock fail, mHandle:%p, ret:%d", __FUNCTION__,
            mpHwBuffer, ret);
    return false;
  }

  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
