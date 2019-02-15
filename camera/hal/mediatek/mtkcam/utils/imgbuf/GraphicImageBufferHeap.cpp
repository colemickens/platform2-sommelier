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

#define LOG_TAG "MtkCam/GraphicImageBufferHeap"
//
#include "BaseImageBufferHeap.h"
#include <cros-camera/camera_buffer_manager.h>
#include <memory>
#include <mtkcam/utils/gralloc/IGrallocHelper.h>
#include <mtkcam/utils/imgbuf/IGraphicImageBufferHeap.h>
#include <vector>

using NSCam::GrallocStaticInfo;
using NSCam::IGrallocHelper;
using NSCam::IGraphicImageBufferHeap;
using NSCam::NSImageBufferHeap::BaseImageBufferHeap;

/******************************************************************************
 *  Image Buffer Heap.
 ******************************************************************************/
namespace {
class GraphicImageBufferHeap
    : public IGraphicImageBufferHeap,
      public NSCam::NSImageBufferHeap::BaseImageBufferHeap {
  friend class IGraphicImageBufferHeap;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IGraphicImageBufferHeap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Accessors.
  virtual buffer_handle_t getBufferHandle() const { return *mpBufferHandle; }
  virtual buffer_handle_t* getBufferHandlePtr() const { return mpBufferHandle; }
  virtual MINT getAcquireFence() const { return mAcquireFence; }
  virtual MVOID setAcquireFence(MINT fence) { mAcquireFence = fence; }
  virtual MINT getReleaseFence() const { return mReleaseFence; }
  virtual MVOID setReleaseFence(MINT fence) { mReleaseFence = fence; }

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
 protected:  ////                    Heap Info.
  typedef HeapInfo MyHeapInfo;
  typedef std::vector<std::shared_ptr<MyHeapInfo> > MyHeapInfoVect_t;

 protected:  ////                    Buffer Info.
  typedef std::vector<std::shared_ptr<BufInfo> > MyBufInfoVect_t;
  struct BufferParams {
    int width = 0;
    int height = 0;
    int stride = 0;
    int format = 0;
    int usage = 0;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected
     :  ////
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        //  Instantiation.
        //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Destructor/Constructors.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~GraphicImageBufferHeap();
  GraphicImageBufferHeap(char const* szCallerName,
                         buffer_handle_t* pBufferHandle,
                         const BufferParams& rBufParams,
                         MINT const acquire_fence,
                         MINT const release_fence);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  MyHeapInfoVect_t mvHeapInfo;
  MyBufInfoVect_t mvBufInfo;

 protected:  ////
  buffer_handle_t* mpBufferHandle;
  BufferParams mBufferParams;
  int mAcquireFence;
  int mReleaseFence;
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
static bool validate_camera3_stream_buffer(
    camera3_stream_buffer const* stream_buffer) {
  if (!stream_buffer) {
    CAM_LOGE("camera3_stream_buffer: NULL");
    return false;
  }
  //
  if (!stream_buffer->stream) {
    CAM_LOGE("camera3_stream_buffer: NULL stream");
    return false;
  }
  //
  if (!stream_buffer->buffer) {
    CAM_LOGE("camera3_stream_buffer: NULL buffer");
    return false;
  }
  //
  if (!*stream_buffer->buffer) {
    CAM_LOGE("camera3_stream_buffer: NULL *buffer");
    return false;
  }
  ///
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IGraphicImageBufferHeap> IGraphicImageBufferHeap::create(
    char const* szCallerName, camera3_stream_buffer const* stream_buffer) {
  if (!validate_camera3_stream_buffer(stream_buffer)) {
    return nullptr;
  }
  //
  GrallocStaticInfo staticInfo;
  MERROR status = IGrallocHelper::singleton()->query(
      *stream_buffer->buffer, stream_buffer->stream->usage, &staticInfo);
  if (OK != status) {
    CAM_LOGE(
        "cannot query the real format from buffer_handle_t - status:%d(%s)",
        status, ::strerror(-status));
    return nullptr;
  }
  //
  GraphicImageBufferHeap::BufferParams rBufParams = {
      .width = static_cast<int>(stream_buffer->stream->width),
      .height = static_cast<int>(stream_buffer->stream->height),
      .stride = 0,
      .format = static_cast<int>(staticInfo.format),
      .usage = static_cast<int>(stream_buffer->stream->usage),
  };
  //
  std::shared_ptr<GraphicImageBufferHeap> pHeap = nullptr;
  pHeap = std::make_shared<GraphicImageBufferHeap>(
      szCallerName, stream_buffer->buffer, rBufParams,
      stream_buffer->acquire_fence, stream_buffer->release_fence);
  if (!pHeap) {
    CAM_LOGE("Fail to new a heap");
    return nullptr;
  }

  MSize const imgSize(rBufParams.width, rBufParams.height);
  int format = staticInfo.format;

  if (!pHeap->onCreate(std::dynamic_pointer_cast<BaseImageBufferHeap>(pHeap),
                       imgSize, format, 0, /*enableLog*/ false)) {
    CAM_LOGE("onCreate fail");
    return nullptr;
  }
  //
  return pHeap;
}

/******************************************************************************
 *
 ******************************************************************************/
GraphicImageBufferHeap::GraphicImageBufferHeap(char const* szCallerName,
                                               buffer_handle_t* pBufferHandle,
                                               const BufferParams& rBufParams,
                                               int const acquire_fence,
                                               int const release_fence)
    : BaseImageBufferHeap(szCallerName),
      mpBufferHandle(pBufferHandle),
      mBufferParams(rBufParams),
      mAcquireFence(acquire_fence),
      mReleaseFence(release_fence) {}

GraphicImageBufferHeap::~GraphicImageBufferHeap() {
  impUninit();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GraphicImageBufferHeap::impInit(BufInfoVect_t const& rvBufInfo) {
  GrallocStaticInfo staticInfo;
  IGrallocHelper* const pGrallocHelper = IGrallocHelper::singleton();
  //
  MERROR status = OK;
  //
  status = pGrallocHelper->query(getBufferHandle(), mBufferParams.usage,
                                 &staticInfo);
  if (OK != status) {
    MY_LOGE("cannot query the real format from buffer_handle_t - status:%d[%s]",
            status, ::strerror(status));
    return false;
  }
  //
  mvHeapInfo.reserve(getPlaneCount());
  mvBufInfo.reserve(getPlaneCount());
  //

  for (int i = 0; i < getPlaneCount(); i++) {
    std::shared_ptr<MyHeapInfo> pHeapInfo = std::make_shared<MyHeapInfo>();
    mvHeapInfo.push_back(pHeapInfo);
    pHeapInfo->heapID = getBufferHandle()->data[0];
    //
    std::shared_ptr<BufInfo> pBufInfo = std::make_shared<BufInfo>();
    mvBufInfo.push_back(pBufInfo);
    pBufInfo->stridesInBytes = staticInfo.planes[i].rowStrideInBytes;
    pBufInfo->sizeInBytes = staticInfo.planes[i].sizeInBytes;
    //
    rvBufInfo[i]->stridesInBytes = pBufInfo->stridesInBytes;
    rvBufInfo[i]->sizeInBytes = pBufInfo->sizeInBytes;
  }
  //
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GraphicImageBufferHeap::impUninit() {
  mvBufInfo.clear();
  mvHeapInfo.clear();
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GraphicImageBufferHeap::impLockBuf(char const* szCallerName,
                                   int usage,
                                   BufInfoVect_t const& rvBufInfo) {
  void* vaddr = NULL;
  cros::CameraBufferManager* bufManager =
      cros::CameraBufferManager::GetInstance();
  uint32_t planeNum = getPlaneCount();
  int ret = 0;

  if (planeNum == 1) {
    void* data = nullptr;
    ret =
        (mBufferParams.format == HAL_PIXEL_FORMAT_BLOB)
            ? bufManager->Lock(getBufferHandle(), 0, 0, 0,
                               mBufferParams.width * mBufferParams.height, 1,
                               &data)
            : bufManager->Lock(getBufferHandle(), 0, 0, 0, mBufferParams.width,
                               mBufferParams.height, &data);
    if (ret) {
      MY_LOGE("@%s: call Lock fail, mHandle:%p", __FUNCTION__, mpBufferHandle);
      return MFALSE;
    }
    vaddr = data;
  } else if (planeNum > 1) {
    struct android_ycbcr ycbrData;
    ret = bufManager->LockYCbCr(getBufferHandle(), 0, 0, 0, mBufferParams.width,
                                mBufferParams.height, &ycbrData);
    if (ret) {
      MY_LOGE("@%s: call LockYCbCr fail, mHandle:%p", __FUNCTION__,
              getBufferHandle());
      return MFALSE;
    }
    vaddr = ycbrData.y;
  } else {
    MY_LOGE("ERROR @%s: planeNum is 0", __FUNCTION__);
    return MFALSE;
  }

  MINTPTR va = reinterpret_cast<MINTPTR>(vaddr);
  for (size_t i = 0; i < planeNum; i++) {
    rvBufInfo[i]->va = va;
    va += mvBufInfo[i]->sizeInBytes;
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
GraphicImageBufferHeap::impUnlockBuf(char const* szCallerName,
                                     MINT usage,
                                     BufInfoVect_t const& rvBufInfo) {
  for (size_t i = 0; i < getPlaneCount(); i++) {
    rvBufInfo[i]->va = 0;
    rvBufInfo[i]->pa = 0;
  }
  //
  cros::CameraBufferManager* bufManager =
      cros::CameraBufferManager::GetInstance();
  bufManager->Unlock(getBufferHandle());
  //
  return MTRUE;
}
