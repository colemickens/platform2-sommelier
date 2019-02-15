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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERS_H_
//
#include <list>
#include <memory>
#include <mutex>
#include <string>
//
#include <mtkcam/def/common.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include "StreamBufferPool.h"
#include "UsersManager.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of stream buffer.
 */
class VISIBILITY_PUBLIC StreamBufferImp : public virtual IStreamBuffer {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  typedef status_t status_t;
  typedef std::mutex Mutex;
  typedef pthread_rwlock_t RWLock;
  typedef std::string String8;

 protected:  ////                                Definitions.
  struct RWUser {
    std::string name;
    //
    explicit RWUser(char const* _name) : name(_name) {}
  };
  typedef std::list<RWUser> RWUserListT;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:  ////                                Data Members.
  std::shared_ptr<IStreamInfo> const mStreamInfo;

 protected:  ////                                Data Members.
  mutable std::timed_mutex mBufMutex;
  mutable std::timed_mutex mRepeatMutex;
  MUINT32 mBufStatus;
  std::shared_ptr<IUsersManager> mUsersManager;

 protected:  ////                                Data Members.
  mutable RWUserListT mWriters;
  mutable RWUserListT mReaders;
  MVOID* const mpBuffer;

 protected:  ////                                Operations.
  MVOID dumpLocked() const;
  virtual MVOID printLocked() const;

  virtual MVOID onUnlock(char const* szCallName, MVOID* const pBuffer);

  virtual MVOID* onTryReadLock(char const* szCallName);

  virtual MVOID* onTryWriteLock(char const* szCallName);

 public:  ////                                Operations.
  StreamBufferImp(std::shared_ptr<IStreamInfo> pStreamInfo,
                  MVOID* const pBuffer,
                  std::shared_ptr<IUsersManager> pUsersManager = 0);

  virtual MVOID setUsersManager(std::shared_ptr<IUsersManager> value);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IUsersManager Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Operations.
  virtual Subject_T getSubject() const;
  virtual char const* getSubjectName() const;
  virtual MVOID dumpState() const;

 public:  ////                                Operations.
  virtual std::shared_ptr<IUsersManager::IUserGraph> createGraph();

  virtual ssize_t enqueUserGraph(std::shared_ptr<IUserGraph> pUserGraph);

  virtual MERROR finishUserSetup();

  virtual MVOID reset();

 public:  ////                                Operations.
  virtual MUINT32 markUserStatus(UserId_T userId, MUINT32 const statusMask);

  virtual MUINT32 getUserStatus(UserId_T userId) const;

  virtual MUINT getUserCategory(UserId_T userId) const;

  virtual MERROR setUserReleaseFence(UserId_T userId, MINT releaseFence);

  virtual MUINT64 queryGroupUsage(UserId_T userId) const;

  virtual size_t getNumberOfProducers() const;
  virtual size_t getNumberOfConsumers() const;

 public:  ////                                Operations.
  virtual MINT createAcquireFence(UserId_T userId) const;

  virtual MINT createReleaseFence(UserId_T userId) const;

  virtual MINT createAcquireFence() const;

  virtual MINT createReleaseFence() const;

 public:  ////                                Operations.
  virtual MERROR haveAllUsersReleasedOrPreReleased(UserId_T userId) const;

  virtual MERROR haveAllUsersReleased() const;

  virtual MUINT32 getAllUsersStatus() const;

  virtual MERROR haveAllProducerUsersReleased() const;

  virtual MERROR haveAllProducerUsersUsed() const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamBuffer Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Attributes.
  virtual char const* getName() const { return mStreamInfo->getStreamName(); }

  virtual StreamId_T getStreamId() const { return mStreamInfo->getStreamId(); }

  virtual MUINT32 getStatus() const {
    std::lock_guard<std::timed_mutex> _l(mBufMutex);
    return mBufStatus;
  }

  virtual MBOOL hasStatus(MUINT32 mask) const {
    std::lock_guard<std::timed_mutex> _l(mBufMutex);
    return 0 != (mBufStatus & mask);
  }

  virtual MVOID markStatus(MUINT32 mask) {
    std::lock_guard<std::timed_mutex> _l(mBufMutex);
    mBufStatus |= mask;
  }

  virtual MVOID clearStatus() {
    std::lock_guard<std::timed_mutex> _l(mBufMutex);
    mBufStatus = 0;
  }
};

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of stream buffer template.
 */
template <class _StreamBufferT_, class _IStreamBufferT_>
class TStreamBuffer : public _IStreamBufferT_, public StreamBufferImp {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  typedef TStreamBuffer TStreamBufferT;
  typedef _StreamBufferT_ StreamBufferT;
  typedef _IStreamBufferT_ IStreamBufferT;
  typedef typename IStreamBufferT::IBufferT IBufferT;
  typedef typename IStreamBufferT::IStreamInfoT IStreamInfoT;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                                Data Members.
  std::shared_ptr<IStreamInfoT> const mStreamInfo;

 public:  ////                                Operations.
  TStreamBuffer(std::shared_ptr<IStreamInfoT> pStreamInfo,
                IBufferT* pIBuffer,
                std::shared_ptr<IUsersManager> pUsersManager = 0)
      : StreamBufferImp(pStreamInfo, pIBuffer, pUsersManager),
        mStreamInfo(pStreamInfo) {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamBuffer Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Attributes.
  virtual std::shared_ptr<IStreamInfoT> getStreamInfo() const {
    return mStreamInfo;
  }

 public:  ////                                Operations.
  virtual MVOID unlock(char const* szCallName, IBufferT* pBuffer) {
    onUnlock(szCallName, pBuffer);
  }

  virtual IBufferT* tryReadLock(char const* szCallName) {
    return reinterpret_cast<IBufferT*>(onTryReadLock(szCallName));
  }

  virtual IBufferT* tryWriteLock(char const* szCallName) {
    return reinterpret_cast<IBufferT*>(onTryWriteLock(szCallName));
  }
};

/******************************************************************************
 *
 ******************************************************************************/
/**
 * A template implementation of stream buffer with a pool.
 */
template <class _StreamBufferT_, class _IStreamBufferT_>
class TStreamBufferWithPool
    : public TStreamBuffer<_StreamBufferT_, _IStreamBufferT_>,
      public std::enable_shared_from_this<
          TStreamBufferWithPool<_StreamBufferT_, _IStreamBufferT_> > {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  typedef _StreamBufferT_ StreamBufferT;
  typedef _IStreamBufferT_ IStreamBufferT;
  typedef TStreamBuffer<StreamBufferT, IStreamBufferT> ParentT;
  typedef IStreamBufferPool<IStreamBufferT> IStreamBufferPoolT;
  typedef typename IStreamBufferT::IBufferT IBufferT;
  typedef typename IStreamBufferT::IStreamInfoT IStreamInfoT;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                                Data Members.
  mutable std::mutex mBufPoolMutex;
  std::weak_ptr<IStreamBufferPoolT> const mBufPool;

 public:  ////                                Operations.
  TStreamBufferWithPool(std::weak_ptr<IStreamBufferPoolT> pStreamBufPool,
                        std::shared_ptr<IStreamInfoT> pStreamInfo,
                        IBufferT* rIBuffer)
      : ParentT(pStreamInfo, rIBuffer),
        mBufPoolMutex(),
        mBufPool(pStreamBufPool) {}

 public:  ////                                Operations.
  virtual std::shared_ptr<IStreamBufferPoolT> tryGetBufferPool() const;
  virtual MVOID resetBuffer() = 0;
  virtual MVOID releaseBuffer();
};

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of hal image stream buffer.
 */
class VISIBILITY_PUBLIC HalImageStreamBuffer
    : public TStreamBufferWithPool<HalImageStreamBuffer, IImageStreamBuffer> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  struct Allocator {
   public:  ////                            Definitions.
    typedef StreamBufferPool<IStreamBufferT, StreamBufferT, Allocator>
        StreamBufferPoolT;

   public:  ////                            Data Members.
    std::shared_ptr<IStreamInfoT> mpStreamInfo;
    IGbmImageBufferHeap::AllocImgParam_t mAllocImgParam;

   public:  ////                            Operations.
    Allocator(std::shared_ptr<IStreamInfoT> pStreamInfo,
              IGbmImageBufferHeap::AllocImgParam_t const& rAllocImgParam);

   public:  ////                            Operator.
    std::shared_ptr<StreamBufferT> operator()(
        std::shared_ptr<IStreamBufferPoolT> pPool = nullptr);
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                                Data Members.
  std::shared_ptr<IImageBufferHeap> mImageBufferHeap;  // remove me

 public:  ////                                Operations.
  HalImageStreamBuffer(std::shared_ptr<IStreamInfoT> pStreamInfo,
                       std::weak_ptr<IStreamBufferPoolT> pStreamBufPool,
                       std::shared_ptr<IImageBufferHeap> pImageBufferHeap);

 public:  ////                                Operations.
  virtual MVOID resetBuffer();
  virtual MVOID releaseBuffer();

 public:  ////                                Attributes.
  virtual std::string toString() const;
};

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of hal metadata stream buffer.
 */
class VISIBILITY_PUBLIC HalMetaStreamBuffer
    : public TStreamBufferWithPool<HalMetaStreamBuffer, IMetaStreamBuffer> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
  struct Allocator {
   public:  ////                            Definitions.
    typedef StreamBufferPool<IStreamBufferT, StreamBufferT, Allocator>
        StreamBufferPoolT;

   public:  ////                            Data Members.
    std::shared_ptr<IStreamInfoT> mpStreamInfo;

   public:  ////                            Operations.
    explicit Allocator(std::shared_ptr<IStreamInfoT> pStreamInfo);

   public:  ////                            Operator.
    std::shared_ptr<StreamBufferT> operator()(
        std::shared_ptr<IStreamBufferPoolT> pPool = nullptr);
    std::shared_ptr<StreamBufferT> operator()(
        NSCam::IMetadata const& metadata,
        std::shared_ptr<IStreamBufferPoolT> pPool = nullptr);
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:                    ////                                Data Members.
  NSCam::IMetadata mMetadata;  // IBufferT-derived object.
  MBOOL mRepeating;

 public:  ////                                Operations.
  HalMetaStreamBuffer(std::shared_ptr<IStreamInfoT> pStreamInfo,
                      std::weak_ptr<IStreamBufferPoolT> pStreamBufPool);
  HalMetaStreamBuffer(NSCam::IMetadata const& metadata,
                      std::shared_ptr<IStreamInfoT> pStreamInfo,
                      std::weak_ptr<IStreamBufferPoolT> pStreamBufPool);

 public:  ////                                Operations.
  virtual MVOID resetBuffer();
  virtual MVOID releaseBuffer();

 public:  ////                                Attributes.
  virtual MVOID setRepeating(MBOOL const repeating);
  virtual MBOOL isRepeating() const;
  virtual std::string toString() const;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERS_H_
