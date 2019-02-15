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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPOOL_H_
//
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "IStreamBufferPool.h"
#include "StreamBufferPoolImpl.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * @class StreamBufferPoolImp
 *
 * @param <_IBufferT_> the type of buffer interface.
 *  This type must have operations of incStrong and decStrong.
 *
 * @param <_BufferT_> the type of buffer which is a subclass of _IBufferT_.
 *
 * @param <_AllocatorT_> the type of allocator.
 *  This type must have a call operator as below:
 *      _BufferT_* _AllocatorT_::operator()(IStreamBufferPool*);
 */
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
class StreamBufferPoolImp : public virtual IStreamBufferPool<_IBufferT_>,
                            private StreamBufferPoolImpl {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  typedef std::string String8;

  typedef IStreamBufferPool<_IBufferT_> IPoolT;
  typedef typename IPoolT::IBufferT IBufferT;
  typedef typename IPoolT::SP_IBufferT SP_IBufferT;
  typedef _BufferT_ BufferT;
  typedef std::shared_ptr<BufferT> SP_BufferT;
  typedef _AllocatorT_ AllocatorT;

 private:
  typedef struct {
    SP_BufferT mBuf;
  } MyBufferT;
  typedef std::vector<MyBufferT> BufferList_t;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamBufferPool Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Operations.
  virtual char const* poolName() const { return mPoolName.c_str(); }

  virtual MVOID dumpPool() const;

  virtual MERROR initPool(char const* szCallerName,
                          size_t maxNumberOfBuffers,
                          size_t minNumberOfInitialCommittedBuffers);

  virtual MVOID uninitPool(char const* szCallerName);

  virtual MERROR commitPool(char const* szCallerName);

  virtual MERROR releaseToPool(char const* szCallerName, SP_IBufferT pBuffer);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  virtual MERROR do_construct(MUINT32* returnIndex);
  MERROR do_acquireFromPool(char const* szCallerName,
                            SP_BufferT* rpBuffer,
                            int64_t nsTimeout);
  MVOID get_itemLocation(SP_BufferT* rpBuffer, MUINT32 index);
  ////
 public:  ////                        Instantiation.
  /**
   * Constructor.
   *
   * @param[in] szPoolName: a null-terminated string for a pool name.
   *
   * @param[in] rAllocator: a function object for a buffer allocator.
   *
   */
  StreamBufferPoolImp(char const* szPoolName, AllocatorT const& rAllocator);

  virtual ~StreamBufferPoolImp();

 protected:  ////                        Data Members.
  mutable std::mutex mLock;
  std::string mPoolName;
  AllocatorT mAllocator;

  BufferList_t mStorage;
};

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::StreamBufferPoolImp(
    char const* szPoolName, AllocatorT const& rAllocator)
    : StreamBufferPoolImpl(), mPoolName(szPoolName), mAllocator(rAllocator) {}

template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::
    ~StreamBufferPoolImp() {}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MVOID StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::dumpPool()
    const {
  dumpPoolImpl();
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MERROR StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::initPool(
    char const* szCallerName,
    size_t maxNumberOfBuffers,
    size_t minNumberOfInitialCommittedBuffers) {
  mStorage.reserve(maxNumberOfBuffers);
  //
  return initPoolImpl(szCallerName, maxNumberOfBuffers,
                      minNumberOfInitialCommittedBuffers);
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MVOID StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::uninitPool(
    char const* szCallerName) {
  uninitPoolImpl(szCallerName);
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MERROR StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::commitPool(
    char const* szCallerName) {
  return commitPoolImpl(szCallerName);
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MERROR StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::releaseToPool(
    char const* szCallerName, SP_IBufferT pBuffer) {
  std::lock_guard<std::mutex> _l(mLock);
  //
  MUINT32 counter = 0;
  typename std::vector<MyBufferT>::iterator iter = mStorage.begin();
  for (; iter != mStorage.end(); iter++) {
    if (iter->mBuf.get() == pBuffer.get()) {
      return releaseToPoolImpl(szCallerName, counter);
    }
    counter++;
  }

  return -ENOSYS;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MERROR StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::do_construct(
    MUINT32* returnIndex) {
  SP_BufferT pBuffer = mAllocator(this->shared_from_this());
  if (pBuffer == nullptr) {
    return -ENOMEM;
  }

  MyBufferT buf;
  buf.mBuf = pBuffer;

  {
    std::lock_guard<std::mutex> _l(mLock);
    mStorage.push_back(buf);
    *returnIndex = mStorage.size() - 1;
  }
  return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MERROR
StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::do_acquireFromPool(
    char const* szCallerName, SP_BufferT* rpBuffer, int64_t nsTimeout) {
  MUINT32 returnBufIndex = 0;
  bool getBuf = acquireFromPoolImpl(szCallerName, &returnBufIndex, nsTimeout);

  if (getBuf != 0) {
    return -ENOMEM;
  }

  get_itemLocation(rpBuffer, returnBufIndex);
  //
  return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
MVOID StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_>::
    get_itemLocation(SP_BufferT* rpBuffer, MUINT32 index) {
  std::lock_guard<std::mutex> _l(mLock);
  if (index < mStorage.size()) {
    *rpBuffer = mStorage[index].mBuf;
  } else {
    *rpBuffer = NULL;
  }
}

/**
 * @class StreamBufferPool
 *
 * @param <_IBufferT_> the type of buffer interface.
 *  This type must have operations of incStrong and decStrong.
 *
 * @param <_BufferT_> the type of buffer which is a subclass of _IBufferT_.
 *
 * @param <_AllocatorT_> the type of allocator.
 *  This type must have a call operator as below:
 *      _BufferT_* _AllocatorT_::operator()(IStreamBufferPool*);
 */
template <class _IBufferT_, class _BufferT_, class _AllocatorT_>
class StreamBufferPool
    : public StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_> {
 public:  ////        Definitions.
  typedef StreamBufferPoolImp<_IBufferT_, _BufferT_, _AllocatorT_> ParentT;
  typedef typename ParentT::AllocatorT AllocatorT;
  typedef typename ParentT::IBufferT IBufferT;
  typedef typename ParentT::BufferT BufferT;
  typedef typename ParentT::SP_IBufferT SP_IBufferT;
  typedef typename ParentT::SP_BufferT SP_BufferT;

 public:  ////        Operations.
  /**
   * Constructor.
   *
   * @param[in] szPoolName: a null-terminated string for a pool name.
   *
   * @param[in] rAllocator: a function object for a buffer allocator.
   *
   */
  StreamBufferPool(char const* szPoolName, AllocatorT const& rAllocator)
      : ParentT(szPoolName, rAllocator) {}

  virtual MERROR acquireFromPool(char const* szCallerName,
                                 SP_IBufferT* rpBuffer,
                                 int64_t nsTimeout) {
    SP_BufferT pBuffer;
    MERROR err = ParentT::do_acquireFromPool(szCallerName, &pBuffer, nsTimeout);
    if (0 == err && pBuffer != 0) {
      *rpBuffer = pBuffer;
    }
    return err;
  }

  virtual MERROR acquireFromPool(char const* szCallerName,
                                 SP_BufferT* rpBuffer,
                                 int64_t nsTimeout) {
    return ParentT::do_acquireFromPool(szCallerName, rpBuffer, nsTimeout);
  }
};

/**
 * @class StreamBufferPool
 *
 * @param <_BufferT_> the type of buffer.
 *  This type must have operations of incStrong and decStrong.
 *
 * @param <_AllocatorT_> the type of allocator.
 *  This type must have a call operator as below:
 *      _BufferT_* _AllocatorT_::operator()(IStreamBufferPool*);
 *
 * @remark This is a version of partial template specialization.
 */
template <class _BufferT_, class _AllocatorT_>
class StreamBufferPool<_BufferT_, _BufferT_, _AllocatorT_>
    : public StreamBufferPoolImp<_BufferT_, _BufferT_, _AllocatorT_> {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////        Definitions.
  typedef StreamBufferPoolImp<_BufferT_, _BufferT_, _AllocatorT_> ParentT;
  typedef typename ParentT::AllocatorT AllocatorT;

 public:  ////        Operations.
  /**
   * Constructor.
   *
   * @param[in] szPoolName: a null-terminated string for a pool name.
   *
   * @param[in] rAllocator: a function object for a buffer allocator.
   *
   */
  StreamBufferPool(char const* szPoolName, AllocatorT const& rAllocator)
      : ParentT(szPoolName, rAllocator) {}
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace Utils
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPOOL_H_
