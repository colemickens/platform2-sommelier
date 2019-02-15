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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERSETCONTROLIMP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERSETCONTROLIMP_H_
//
#include <list>
#include <memory>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferSetControl.h>
#include <unordered_map>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {
namespace Imp {

/**
 * An Implementation of Stream Buffer Set Control.
 */
class StreamBufferSetControlImp : public IStreamBufferSetControl {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations: Buffer Map Template
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  enum {
    eBUF_STATUS_RELEASE = 0,
    eBUF_STATUS_RETURN,
  };

  template <class StreamBufferT>
  struct THolder {
   public:  ////                    Data Members.
    std::shared_ptr<StreamBufferT> mBuffer;
    std::bitset<32> mBitStatus;

   public:  ////                    Operations.
    explicit THolder(std::shared_ptr<StreamBufferT> buffer)
        : mBuffer(buffer), mBitStatus(0) {}

    std::shared_ptr<IUsersManager> getUsersManager() const { return mBuffer; }
  };

  template <class _StreamBufferT_,
            class _IStreamBufferT_ = typename _StreamBufferT_::IStreamBufferT>
  struct TBufferMap
      : public std::unordered_map<StreamId_T,
                                  std::shared_ptr<THolder<_StreamBufferT_> > > {
   public:  ////                    Definitions.
    typedef _StreamBufferT_ StreamBufferT;
    typedef _IStreamBufferT_ IStreamBufferT;

   public:  ////                    Data Members.
    ssize_t mNumberOfNonNullBuffers;

   public:  ////                    Operations.
    TBufferMap() : mNumberOfNonNullBuffers(0) {}
  };

  template <class _StreamBufferMapT_,
            class _StreamBufferT_ = typename _StreamBufferMapT_::StreamBufferT>
  class MyMap : public IMap<_StreamBufferT_> {
   public:  ////                    Definitions.
    typedef _StreamBufferMapT_ StreamBufferMapT;
    typedef _StreamBufferT_ StreamBufferT;

   protected:  ////                    Data Members.
    StreamBufferMapT& mrBufMap;

   public:  ////                    Operations.
    explicit MyMap(StreamBufferMapT& rBufMap) : mrBufMap(rBufMap) {}
    virtual ~MyMap() {}
    virtual ssize_t add(std::shared_ptr<StreamBufferT> pBuffer) {
      if (pBuffer == nullptr) {
        return -EINVAL;
      }
      //
      StreamId_T const streamId = pBuffer->getStreamInfo()->getStreamId();
      //
      mrBufMap.mNumberOfNonNullBuffers++;
      mrBufMap.emplace(streamId,
                       std::make_shared<THolder<StreamBufferT> >(pBuffer));
      //
      return mrBufMap.mNumberOfNonNullBuffers;
    }

    virtual ssize_t setCapacity(size_t size) {
      mrBufMap.reserve(size);
      return mrBufMap.size();
    }

    virtual bool isEmpty() const { return mrBufMap.empty(); }

    virtual size_t size() const { return mrBufMap.size(); }

    virtual ssize_t indexOfKey(StreamId_T const key) const {
      ssize_t index = 0;
      for (auto iter = mrBufMap.begin(); iter != mrBufMap.end();
           iter++, index++)
        if (iter->first == key) {
          return index;
        }
      return -1;
    }

    virtual StreamId_T keyAt(size_t index) const {
      auto iter = mrBufMap.begin();
      std::advance(iter, index);
      return iter->first;
    }

    virtual std::shared_ptr<StreamBufferT> const& valueAt(size_t index) const {
      auto iter = mrBufMap.begin();
      std::advance(iter, index);
      return (iter->second)->mBuffer;
    }

    virtual std::shared_ptr<StreamBufferT> const& valueFor(
        StreamId_T const key) const {
      return mrBufMap.at(key)->mBuffer;
    }
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations: Buffer Map
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Definitions.
  typedef TBufferMap<IImageStreamBuffer, IImageStreamBuffer>
      BufferMap_AppImageT;
  typedef TBufferMap<IMetaStreamBuffer, IMetaStreamBuffer> BufferMap_AppMetaT;
  typedef TBufferMap<HalImageStreamBuffer> BufferMap_HalImageT;
  typedef TBufferMap<HalMetaStreamBuffer> BufferMap_HalMetaT;
  struct TBufMapReleaser_Hal;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                            Data Members.
  mutable std::mutex mBufMapLock;
  BufferMap_AppImageT mBufMap_AppImage;
  BufferMap_AppMetaT mBufMap_AppMeta;
  BufferMap_HalImageT mBufMap_HalImage;
  BufferMap_HalMetaT mBufMap_HalMeta;
  MUINT32 mFrameNo;
  std::weak_ptr<IAppCallback> const mpAppCallback;

 protected:  ////                            Data Members.
  struct MyListener {
    std::weak_ptr<IListener> mpListener;
    MVOID* mpCookie;
    //
    explicit MyListener(std::weak_ptr<IListener> listener,
                        MVOID* const cookie = nullptr)
        : mpListener(listener), mpCookie(cookie) {}
  };
  std::list<MyListener> mListeners;

 public:  ////                            Operations.
  StreamBufferSetControlImp(MUINT32 frameNo,
                            std::weak_ptr<IAppCallback> const& pAppCallback);
  virtual ~StreamBufferSetControlImp() {}

 protected:  ////                            Operations.
  std::shared_ptr<IUsersManager> findSubjectUsersLocked(
      StreamId_T streamId) const;

  template <class StreamBufferMapT>
  std::shared_ptr<typename StreamBufferMapT::IStreamBufferT> getBufferLocked(
      StreamId_T streamId,
      UserId_T userId,
      StreamBufferMapT const& rBufMap) const;

  size_t getMetaBufferSizeLocked() const;
  size_t getImageBufferSizeLocked() const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamBufferSetControl Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
  virtual MERROR attachListener(std::weak_ptr<IListener> const& pListener,
                                MVOID* pCookie);

  virtual MUINT32 getFrameNo() const { return mFrameNo; }

 public:  ////                            Operations.
  virtual std::shared_ptr<IMap<HalImageStreamBuffer> > editMap_HalImage() {
    auto ptr = std::make_shared<MyMap<BufferMap_HalImageT> >(mBufMap_HalImage);
    return ptr;
  }

  virtual std::shared_ptr<IMap<HalMetaStreamBuffer> > editMap_HalMeta() {
    auto ptr = std::make_shared<MyMap<BufferMap_HalMetaT> >(mBufMap_HalMeta);
    return ptr;
  }

  virtual std::shared_ptr<IMap<IImageStreamBuffer> > editMap_AppImage() {
    auto ptr = std::make_shared<MyMap<BufferMap_AppImageT> >(mBufMap_AppImage);
    return ptr;
  }

  virtual std::shared_ptr<IMap<IMetaStreamBuffer> > editMap_AppMeta() {
    auto ptr = std::make_shared<MyMap<BufferMap_AppMetaT> >(mBufMap_AppMeta);
    return ptr;
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamBufferSet Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
  virtual MVOID applyPreRelease(UserId_T userId);

  virtual MVOID applyRelease(UserId_T userId);

 public:  ////                            Operations.
  virtual std::shared_ptr<IMetaStreamBuffer> getMetaBuffer(
      StreamId_T streamId, UserId_T userId) const;

  virtual std::shared_ptr<IImageStreamBuffer> getImageBuffer(
      StreamId_T streamId, UserId_T userId) const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IUsersManager Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
  virtual MUINT32 markUserStatus(StreamId_T const streamId,
                                 UserId_T userId,
                                 MUINT32 eStatus);

  virtual MERROR setUserReleaseFence(StreamId_T const streamId,
                                     UserId_T userId,
                                     MINT releaseFence);

  virtual MUINT64 queryGroupUsage(StreamId_T const streamId,
                                  UserId_T userId) const;

 public:  ////                            Operations.
  virtual MINT createAcquireFence(StreamId_T const streamId,
                                  UserId_T userId) const;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Imp
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERSETCONTROLIMP_H_
