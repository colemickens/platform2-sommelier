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
#include <cros-camera/camera_buffer_manager.h>
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
  struct MyBufInfo : public BufInfo {
    size_t u4Offset;
    MyBufInfo() : BufInfo(), u4Offset(0) {}
  };
  typedef std::vector<std::shared_ptr<MyBufInfo> > MyBufInfoVect_t;

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
  buffer_handle_t handle;
  uint32_t stride = 0;
  int err;

  MY_LOGD("[w,h]=[%d,%d],format=%x", mImgSize.w, mImgSize.h, mImgFormat);
  //  Allocate memory and setup mBufHeapInfo & rBufHeapInfo.
  mGbmBufferManager = cros::CameraBufferManager::GetInstance();
  if (mGbmBufferManager == nullptr) {
    MY_LOGE("GetInstance failed!");
  }

  mvHeapInfo.reserve(getPlaneCount());
  mvBufInfo.reserve(getPlaneCount());
  for (int i = 0; i < getPlaneCount(); i++) {
    std::shared_ptr<MyBufInfo> pBufInfo =
        std::shared_ptr<MyBufInfo>(new MyBufInfo);
    if (pBufInfo == nullptr) {
      MY_LOGE("create fail");
      return false;
    }
    mvBufInfo.push_back(pBufInfo);
    pBufInfo->stridesInBytes = mBufStridesInBytesToAlloc[i];
    pBufInfo->sizeInBytes =
        helpQueryBufSizeInBytes(i, mBufStridesInBytesToAlloc[i]);
    pBufInfo->u4Offset = pBufInfo->sizeInBytes;
    rvBufInfo[i]->stridesInBytes = pBufInfo->stridesInBytes;
    rvBufInfo[i]->sizeInBytes = pBufInfo->sizeInBytes;
    mBufsize += pBufInfo->sizeInBytes;
  }

  MY_LOGD("mBufsize = %d", mBufsize);
  size_t offset = 0;
  for (int i = 0; i < getPlaneCount(); i++) {
    mvBufInfo[i]->u4Offset = offset;
    offset += mvBufInfo[i]->sizeInBytes;
  }

  err = mGbmBufferManager->Allocate(mBufsize, 1, HAL_PIXEL_FORMAT_BLOB,
                                    GRALLOC_USAGE_HW_CAMERA_WRITE,
                                    cros::GRALLOC, &handle, &stride);
  if (err != 0) {
    MY_LOGE("Allocate handle failed! %d", ret);
    goto lbExit;
  }
  mpHwBuffer = handle;

  for (MUINT32 i = 0; i < getPlaneCount(); i++) {
    std::shared_ptr<HeapInfo> pHeapInfo = std::make_shared<HeapInfo>();
    if (pHeapInfo == nullptr) {
      MY_LOGE("create fail");
      return false;
    }
    mvHeapInfo.push_back(pHeapInfo);
    pHeapInfo->heapID = handle->data[0];
  }
  ret = MTRUE;
lbExit:
  if (!ret) {
    doDeallocGB();
    mvHeapInfo.clear();
    mvBufInfo.clear();
  }
  MY_LOGD_IF(getLogCond(), "- ret:%d\n", ret);
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
  MERROR status = OK;
  void* data = nullptr;

  int err = mGbmBufferManager->Lock(mpHwBuffer, 0, 0, 0, mBufsize, 1, &data);
  if (err) {
    MY_LOGE("@%s: call Lock fail, mHandle:%p", __FUNCTION__, mpHwBuffer);
  }

  uint32_t planeNum = getPlaneCount();

  if (planeNum == 1) {
    rvBufInfo[0]->va = (MINTPTR)data;
  } else if (planeNum == 2) {
    rvBufInfo[0]->va = (MINTPTR)data;
    rvBufInfo[1]->va = (MINTPTR)data + (MINTPTR)mvBufInfo[1]->u4Offset;
  } else if (planeNum == 3) {
    rvBufInfo[0]->va = (MINTPTR)data;
    rvBufInfo[1]->va = (MINTPTR)data + (MINTPTR)mvBufInfo[1]->u4Offset;
    rvBufInfo[2]->va = (MINTPTR)data + (MINTPTR)mvBufInfo[2]->u4Offset;
  } else {
    MY_LOGE("ERROR @%s: planeNum is 0", __FUNCTION__);
    return UNKNOWN_ERROR;
  }
  //

  if (status != OK) {
    impUnlockBuf(szCallerName, usage, rvBufInfo);
  }
  return status == OK;
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
