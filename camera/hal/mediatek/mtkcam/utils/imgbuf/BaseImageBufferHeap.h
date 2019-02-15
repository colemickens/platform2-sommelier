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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_IMGBUF_BASEIMAGEBUFFERHEAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_IMGBUF_BASEIMAGEBUFFERHEAP_H_
//
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
//
#include <list>
#include <map>
#include <memory>
#include <mtkcam/utils/debug/debug.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/LogTool.h>
#include <mtkcam/utils/std/Trace.h>
#include <mutex>
#include <string>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSImageBufferHeap {
/******************************************************************************
 *  Image Buffer Heap (Base).
 ******************************************************************************/
class BaseImageBufferHeap
    : public virtual IImageBufferHeap,
      public std::enable_shared_from_this<BaseImageBufferHeap> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IImageBufferHeap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Reference Counting.
  virtual MVOID incStrong(MVOID const* id) const {}
  virtual MVOID decStrong(MVOID const* id) const {}
  virtual MINT32 getStrongCount() const {
    return 0; /*return RefBase::getStrongCount(); */
  }

 public:  ////                    Image Attributes.
  virtual MINT getImgFormat() const { return mImgFormat; }
  virtual MSize const& getImgSize() const { return mImgSize; }
  virtual size_t getImgBitsPerPixel() const;
  virtual size_t getPlaneBitsPerPixel(size_t index) const;
  virtual size_t getPlaneCount() const { return mPlaneCount; }
  virtual size_t getBitstreamSize() const { return mBitstreamSize; }
  virtual MBOOL setBitstreamSize(size_t const bitstreamsize);
  virtual void setColorArrangement(MINT32 const colorArrangement);
  virtual MINT32 getColorArrangement() const { return mColorArrangement; }
  virtual MBOOL setImgDesc(ImageDescId id, MINT64 value, MBOOL overwrite);
  virtual MBOOL getImgDesc(ImageDescId id, MINT64* value) const;
  virtual MBOOL updateImgInfo(MSize const& imgSize,
                              MINT const imgFormat,
                              size_t const sizeInBytes[3],
                              size_t const rowStrideInBytes[3],
                              size_t const bufPlaneSize);

 public:  ////                    Buffer Attributes.
  virtual MBOOL getLogCond() const { return mEnableLog; }
  virtual char const* getMagicName() const { return impGetMagicName(); }

  virtual MINT32 getHeapID(size_t index) const;
  virtual size_t getHeapIDCount() const;
  virtual MINTPTR getBufPA(size_t index) const;
  virtual MINTPTR getBufVA(size_t index) const;
  virtual size_t getBufSizeInBytes(size_t index) const;
  virtual size_t getBufStridesInBytes(size_t index) const;

  /**
   * Buffer customized size, which means caller specified the buffer size he
   * wants of the given plane. Caller usually gives this value because he wants
   * vertical padding of image. This method returns 0 if caller didn't specify
   * the customized buffer size of the given plane.
   */
  virtual size_t getBufCustomSizeInBytes(size_t /*index*/) const { return 0; }
  virtual void* getHWBuffer() { return NULL; }

 public:  ////                    Buffer Operations.
  virtual MBOOL lockBuf(char const* szCallerName, MINT usage);
  virtual MBOOL unlockBuf(char const* szCallerName);
  virtual MBOOL syncCache(eCacheCtrl const ctrl);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IImageBuffer Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  /**
   * Create an IImageBuffer instance with its ROI equal to the image full
   * resolution of this heap.
   */
  virtual std::shared_ptr<IImageBuffer> createImageBuffer(
      ImgBufCreator* pCreator = NULL);

  /**
   * This call is legal only if the heap format is blob.
   *
   * From the given blob heap, create an IImageBuffer instance with a specified
   * offset and size, and its format equal to blob.
   */
  virtual std::shared_ptr<IImageBuffer> createImageBuffer_FromBlobHeap(
      size_t offsetInBytes, size_t sizeInBytes);

  /**
   * This call is legal only if the heap format is blob.
   *
   * From the given blob heap, create an IImageBuffer instance with a specified
   * offset, image format, image size in pixels, and buffer strides in pixels.
   */
  virtual std::shared_ptr<IImageBuffer> createImageBuffer_FromBlobHeap(
      size_t offsetInBytes,
      MINT32 imgFormat,
      MSize const& imgSize,
      size_t const bufStridesInBytes[3]);

  /**
   * Create an IImageBuffer instance indicating the left-side or right-side
   * buffer within a side-by-side image.
   *
   * Left side if isRightSide = 0; otherwise right side.
   */
  virtual std::shared_ptr<IImageBuffer> createImageBuffer_SideBySide(
      MBOOL isRightSide);

  virtual std::vector<std::shared_ptr<IImageBuffer> >
  createImageBuffers_FromBlobHeap(const ImageBufferInfo& info,
                                  const char* callerName);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Heap Info.
  struct HeapInfo {
    MINT32 heapID;  // heap ID
    HeapInfo() : heapID(-1) {}
  };
  typedef std::vector<std::shared_ptr<HeapInfo> > HeapInfoVect_t;

 public:  ////                    Buffer Info.
  struct BufInfo {
    MINTPTR pa;             // (plane) physical address
    MINTPTR va;             // (plane) virtual address
    size_t stridesInBytes;  // (plane) strides in bytes
    size_t sizeInBytes;     // (plane) size in bytes
    BufInfo(MINTPTR _pa = 0,
            MINTPTR _va = 0,
            size_t _stridesInBytes = 0,
            size_t _sizeInBytes = 0)
        : pa(_pa),
          va(_va),
          stridesInBytes(_stridesInBytes),
          sizeInBytes(_sizeInBytes) {}
  };
  typedef std::vector<std::shared_ptr<BufInfo> > BufInfoVect_t;

 public:  ////                    Buffer Lock Info.
  struct BufLockInfo {
    std::string mUser;
    pid_t mTid;
    struct timespec mTimestamp;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Template-Method Pattern. Subclass must implement them.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  /**
   * Return a pointer to a null-terminated string to indicate a magic name of
   * buffer type.
   */
  virtual char const* impGetMagicName() const = 0;

  /**
   * This call is valid after calling impLockBuf();
   * invalid after impUnlockBuf().
   */
  virtual HeapInfoVect_t const& impGetHeapInfo() const = 0;

  /**
   * onCreate() must be invoked by a subclass when its instance is created to
   * inform this base class of a creating event.
   * The call impInit(), implemented by a subclass, will be invoked by this
   * base class during onCreate() for initialization.
   * As to buffer information (i.e. BufInfoVect_t), buffer strides in pixels
   * and buffer size in bytes of each plane as well as the vector size MUST be
   * legal, at least, after impInit() return success.
   *
   * onLastStrongRef() will be invoked to indicate the last one reference to
   * this instance before it is freed.
   * The call impUninit(), implemented by a subclass, will be invoked by this
   * base class during onLastStrongRef() for uninitialization.
   */
  virtual MBOOL impInit(BufInfoVect_t const& rvBufInfo) = 0;
  virtual MBOOL impUninit() = 0;
  virtual MBOOL impReconfig(BufInfoVect_t const& rvBufInfo) = 0;

 public:  ////
  /**
   * As to buffer information (i.e. BufInfoVect_t), buffer strides in bytes
   * and buffer size in bytes of each plane as well as the vector size MUST be
   * always legal.
   *
   * After calling impLockBuf() successfully, the heap information from
   * impGetHeapInfo() must be legal; virtual address and physical address of
   * each plane must be legal if any SW usage and any HW usage are specified,
   * respectively.
   */
  virtual MBOOL impLockBuf(char const* szCallerName,
                           MINT usage,
                           BufInfoVect_t const& rvBufInfo) = 0;
  virtual MBOOL impUnlockBuf(char const* szCallerName,
                             MINT usage,
                             BufInfoVect_t const& rvBufInfo) = 0;

 protected:  ////
  /**
   * The call impPrintLocked(), implemented by a subclass, will be invoked by
   * this base class during printLocked().
   */
  virtual std::string impPrintLocked() const { return std::string(""); }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Helper Functions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

 protected:  ////                    Helper Functions.
  virtual MBOOL helpCheckBufStrides(size_t const planeIndex,
                                    size_t const planeBufStridesInBytes) const;

  virtual size_t helpQueryBufSizeInBytes(
      size_t const planeIndex, size_t const planeStridesInBytes) const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:  ////                    Called inside lock.
  virtual MBOOL initLocked();
  virtual MBOOL uninitLocked();
  virtual MBOOL lockBufLocked(char const* szCallerName, MINT usage);
  virtual MBOOL unlockBufLocked(char const* szCallerName);
  virtual void dummy(std::shared_ptr<BaseImageBufferHeap> p);
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Destructor/Constructors.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~BaseImageBufferHeap();
  explicit BaseImageBufferHeap(char const* szCallerName);

 protected:  ////                    Callback (LastStrongRef@RefBase)
  virtual void onFirstRef();
  virtual void onLastStrongRef(const void* id);

 protected:  ////                    Callback (Create)
  MBOOL onCreate(std::shared_ptr<BaseImageBufferHeap> heap,
                 MSize const& imgSize,
                 MINT const imgFormat = eImgFmt_BLOB,
                 size_t const bitstreamSize = 0,
                 MBOOL const enableLog = MTRUE);

  virtual MBOOL onCreate(MSize const& imgSize,
                         MINT const imgFormat = eImgFmt_BLOB,
                         size_t const bitstreamSize = 0,
                         MBOOL const enableLog = MTRUE);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:  ////                    Heap Info.
  std::map<MINT32, MINTPTR> mBufMap;
  mutable std::mutex mInitMtx;
  mutable std::mutex mLockMtx;
  MINT32 volatile mLockCount;
  MINT32 mLockUsage;
  std::list<BufLockInfo> mLockInfoList;
  BufInfoVect_t mvBufInfo;

 private:  ////                    Image Attributes.
  MSize mImgSize;
  MINT mImgFormat;
  size_t mPlaneCount;
  size_t mBitstreamSize;  // in bytes
  MINT32 mColorArrangement;
  MBOOL mEnableLog;

 protected:
  struct timespec mCreationTimestamp;
  size_t mCreationTimeCost = 0;
  std::string mCallerName;

 private:
  ImgBufCreator* mCreator;

 private:
  struct ImageDescItem {
    MBOOL isValid;
    MINT64 value;

    ImageDescItem() : isValid(MFALSE), value(0) {}
  };

  // Because we only have few IDs currently. Use a simple array will be more
  // economical and efficient than a complicated data structure, e.g.
  // KeyedVector<> or std::map<>
  ImageDescItem mImageDesc[kImageDescIdMax];
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSImageBufferHeap
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_IMGBUF_BASEIMAGEBUFFERHEAP_H_
