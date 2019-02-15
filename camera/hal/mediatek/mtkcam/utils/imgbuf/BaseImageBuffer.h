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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_IMGBUF_BASEIMAGEBUFFER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_IMGBUF_BASEIMAGEBUFFER_H_
//
#include "BaseImageBufferHeap.h"
#include <memory>
#include <vector>

using NSCam::NSImageBufferHeap::BaseImageBufferHeap;

namespace NSCam {
namespace NSImageBuffer {

/******************************************************************************
 *  Image Buffer (Base).
 ******************************************************************************/
class BaseImageBuffer : public IImageBuffer,
                        public std::enable_shared_from_this<BaseImageBuffer> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IImageBuffer Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Reference Counting.
  virtual MVOID incStrong(MVOID const* id) const {}
  virtual MVOID decStrong(MVOID const* id) const {}
  virtual MINT32 getStrongCount() const { return 0; }

 public:  ////                    Image Attributes.
  virtual MINT getImgFormat() const { return mImgFormat; }
  virtual MSize const& getImgSize() const { return mImgSize; }
  virtual size_t getImgBitsPerPixel() const;
  virtual size_t getPlaneBitsPerPixel(size_t index) const;
  virtual size_t getPlaneCount() const { return mPlaneCount; }
  virtual size_t getBitstreamSize() const { return mBitstreamSize; }
  virtual MBOOL setBitstreamSize(size_t const bitstreamsize);
  virtual void setColorArrangement(MINT32 const colorArrangement);
  virtual MINT32 getColorArrangement() const {
    return mspImgBufHeap->getColorArrangement();
  }
  virtual MBOOL setImgDesc(ImageDescId id, MINT64 value, MBOOL overwrite);
  virtual MBOOL getImgDesc(ImageDescId id, MINT64* value) const;
  virtual std::shared_ptr<IImageBufferHeap> getImageBufferHeap() const {
    return mspImgBufHeap;
  }
  virtual MBOOL setExtParam(MSize const& imgSize, size_t offsetInBytes);
  virtual size_t getExtOffsetInBytes(size_t index) const;
  virtual MVOID setColorProfile(eColorProfile profile) {
    mColorProfile = profile;
  }
  virtual eColorProfile getColorProfile() const { return mColorProfile; }

 public:  ////                    Buffer Attributes.
  virtual char const* getMagicName() const {
    return mspImgBufHeap->getMagicName();
  }
  virtual MINT32 getFD(size_t index = 0) const {
    return mspImgBufHeap->getHeapID(index);
  }
  virtual size_t getFDCount() const { return mspImgBufHeap->getHeapIDCount(); }
  virtual size_t getBufOffsetInBytes(size_t index) const;
  virtual MINTPTR getBufPA(size_t index) const;
  virtual MINTPTR getBufVA(size_t index) const;
  virtual size_t getBufSizeInBytes(size_t index) const;
  virtual size_t getBufStridesInBytes(size_t index) const;
  virtual size_t getBufStridesInPixel(size_t index) const;
  virtual size_t getBufScanlines(size_t index) const;

 public:  ////                    Buffer Operations.
  virtual MBOOL lockBuf(char const* szCallerName, MINT usage);
  virtual MBOOL unlockBuf(char const* szCallerName);
  virtual MBOOL syncCache(eCacheCtrl const ctrl) {
    return mspImgBufHeap->syncCache(ctrl);
  }

 public:  ////                    File Operations.
  virtual MBOOL saveToFile(char const* filepath);
  virtual MBOOL loadFromFile(char const* filepath);

 public:  ////                    Timestamp Accesssors.
  virtual MINT64 getTimestamp() const { return mTimestamp; }
  virtual MVOID setTimestamp(MINT64 const timestamp) { mTimestamp = timestamp; }

 public:  ////                    Fence Operations.
  virtual MINT getAcquireFence() const { return mAcquireFence; }
  virtual MVOID setAcquireFence(MINT fence) { mAcquireFence = fence; }
  virtual MINT getReleaseFence() const { return mReleaseFence; }
  virtual MVOID setReleaseFence(MINT fence) { mReleaseFence = fence; }
  virtual MBOOL updateInfo(MSize const& imgSize);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:  ////                    Called inside lock.
  virtual MBOOL lockBufLocked(char const* szCallerName, MINT usage);
  virtual MBOOL unlockBufLocked(char const* szCallerName);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                       Destructor/Constructors.
  virtual ~BaseImageBuffer();
  BaseImageBuffer(std::shared_ptr<BaseImageBufferHeap> _spImgBufHeap,
                  MSize _imgSize,
                  MINT _imgFormat,
                  size_t _bufSize,
                  size_t const _strides[3],
                  size_t _offset = 0)
      : IImageBuffer(),
        mspImgBufHeap(_spImgBufHeap),
        mLockCount(0),
        mLockUsage(0),
        mImgSize(_imgSize),
        mBufHeight(_imgSize.h),
        mImgFormat(_imgFormat),
        mPlaneCount(NSCam::Utils::Format::queryPlaneCount(_imgFormat)),
        mBitstreamSize(_bufSize),
        mColorArrangement(-1),
        mOffset(_offset),
        mTimestamp(0),
        mAcquireFence(0),
        mReleaseFence(0),
        mColorProfile(eCOLORPROFILE_UNKNOWN) {
    for (size_t i = 0; i < 3; ++i) {
      mStrides[i] = _strides[i];
    }
  }

 public:  ////                       Callback (Create)
  virtual MBOOL onCreate();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Buffer Info.
  struct ImgBufInfo : public BaseImageBufferHeap::BufInfo {
    size_t offsetInBytes;     // (plane) offset in bytes
    size_t extOffsetInBytes;  // (plane) offset in bytes for valid image region.
    //
    explicit ImgBufInfo(size_t _offsetInBytes = 0, size_t _extOffsetInBytes = 0)
        : BufInfo(),
          offsetInBytes(_offsetInBytes),
          extOffsetInBytes(_extOffsetInBytes) {}
  };
  typedef std::vector<std::shared_ptr<ImgBufInfo> > ImgBufInfoVect_t;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:  ////                    Heap Info.
  std::shared_ptr<BaseImageBufferHeap> mspImgBufHeap;
  mutable std::mutex mLockMtx;
  MINT32 volatile mLockCount;
  MINT32 mLockUsage;
  ImgBufInfoVect_t mvImgBufInfo;  // from image buffer.
  BaseImageBufferHeap::BufInfoVect_t
      mvBufHeapInfo;  // from heap. use to lock/unlock buffer.

 private:             ////                    Image Attributes.
  MSize mImgSize;     // in pixels
  MINT32 mBufHeight;  // in pixels
  MINT mImgFormat;
  size_t mPlaneCount;
  size_t mBitstreamSize;  // in bytes
  MINT32 mColorArrangement;
  size_t mStrides[3];  // in bytes
  size_t mOffset;      // in bytes
  MINT64 mTimestamp;
  MINT mAcquireFence;
  MINT mReleaseFence;

  static MUINT32 mfgFileCacheEn;
  eColorProfile mColorProfile;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSImageBuffer
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_IMGBUF_BASEIMAGEBUFFER_H_
