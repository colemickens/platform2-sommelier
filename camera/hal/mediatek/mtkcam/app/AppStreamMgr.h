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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_APP_APPSTREAMMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_APP_APPSTREAMMGR_H_
//
#include <bitset>
#include <mtkcam/pipeline/stream/StreamId.h>

#include <mtkcam/app/IAppStreamManager.h>
//
#include <mtkcam/utils/metadata/IMetadataConverter.h>
//
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "IErrorCallback.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Imp {

/**
 * An implementation of App stream manager.
 */
class AppStreamMgr : public IAppStreamManager, public IErrorCallback {
  friend class IAppStreamManager;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Callback (camera3_capture_result & camera3_notify_msg)
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct CallbackParcel {
    struct ImageItem {
      std::shared_ptr<AppImageStreamBuffer> buffer;
      std::shared_ptr<AppImageStreamInfo> stream;
    };

    struct MetaItem {
      std::shared_ptr<IMetaStreamBuffer> buffer;
      MUINT32 bufferNo;  // partial_result
    };

    struct Error {
      std::shared_ptr<AppImageStreamInfo> stream;
      MINT errorCode;
    };

    struct Shutter {
      MUINT64 timestamp;
    };

    std::vector<ImageItem> vInputImageItem;
    std::vector<ImageItem> vOutputImageItem;
    std::vector<MetaItem> vOutputMetaItem;
    std::vector<Error> vError;
    std::shared_ptr<Shutter> shutter;
    MUINT64 timestampShutter;
    MUINT32 frameNo;
    MBOOL valid;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Result Queue
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  struct ResultItem {
    bool lastPartial;  // partial metadata
    MUINT32 frameNo;
    std::vector<std::shared_ptr<IMetaStreamBuffer>> buffer;
  };

  typedef enum {
    TYPE_NONE = 0,
    TYPE_YUV = 1,
    TYPE_IMPLEMENTATION_DEFINED = 2,
  } stream_input_type;

 public:  ////
  typedef std::map<MUINT32, std::shared_ptr<ResultItem>> ResultQueueT;

  bool exitPending() const { return mExitPending; }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                Data Members.
  MINT32 const mOpenId;
  camera3_callback_ops const* mpCallbackOps;

 protected:  ////                Data Members.
  std::shared_ptr<IMetadataProvider> mpMetadataProvider;
  size_t mAtMostMetaStreamCount;
  std::shared_ptr<IMetadataConverter> mpMetadataConverter;
  camera_metadata_t* mMetadata;
  std::thread mThread;
  bool mExitPending;

 protected:  ////                Data Members (RESULT)
  mutable std::mutex mResultQueueLock;
  std::condition_variable mResultQueueCond;
  ResultQueueT mResultQueue;

 protected:  ////                Data Members (CONFIG/FRAME)
  class FrameHandler;
  std::shared_ptr<FrameHandler> mFrameHandler;
  mutable std::mutex mFrameHandlerLock;
  std::condition_variable mFrameHandlerCond;
  StreamId_T mStreamIdToConfig;
  IMetadata mLatestSettings;

 protected:  ////                LOGD & LOGI on / off
  MINT32 mLogLevel;

 protected:  ////                fps / duration information for debug
  MUINT64 mAvgTimestampDuration;
  MUINT64 mAvgCallbackDuration;
  MUINT64 mAvgTimestampFps;
  MUINT64 mAvgCallbackFps;
  MUINT32 mFrameCounter;
  MUINT32 mMaxFrameCount;
  //                          latest timestamp / callback time
  MUINT64 mTimestamp;
  MUINT64 mCallbackTime;
  std::bitset<8> mInputType;
  MBOOL mHasImplemt;
  MBOOL mHasVideoEnc;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  AppStreamMgr(MINT32 openId,
               camera3_callback_ops const* callback_ops,
               std::shared_ptr<IMetadataProvider> pMetadataProvider,
               MBOOL isExternal);
  virtual ~AppStreamMgr() {}

 protected:  ////                Operations.
  MERROR checkStream(camera3_stream* stream) const;
  MERROR checkStreams(camera3_stream_configuration_t* stream_list) const;

  std::shared_ptr<AppImageStreamInfo> createImageStreamInfo(
      StreamId_T suggestedStreamId, camera3_stream* stream);

  std::shared_ptr<AppMetaStreamInfo> createMetaStreamInfo(
      StreamId_T suggestedStreamId) const;

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 protected:  ////                Operations (Request Handler)
  MERROR checkRequestLocked(camera3_capture_request_t const* request) const;
  std::shared_ptr<AppImageStreamBuffer> createImageStreamBuffer(
      camera3_stream_buffer const* buffer) const;
  std::shared_ptr<AppMetaStreamBuffer> createMetaStreamBuffer(
      std::shared_ptr<IMetaStreamInfo> pStreamInfo,
      IMetadata const& rSettings,
      MBOOL const repeating) const;

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 protected:  ////                Operations (Result Handler)
  // MERROR                      enqueResult(MUINT32 const frameNo, MINTPTR
  // const userId);
  MERROR enqueResult(MUINT32 const frameNo,
                     MINTPTR const userId,
                     std::vector<std::shared_ptr<IMetaStreamBuffer>> resultMeta,
                     bool hasLastPartial);
  MERROR dequeResult(ResultQueueT* rvResult);
  MVOID handleResult(ResultQueueT const& rvResult);
  MVOID performCallback(CallbackParcel const& cbParcel);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Thread Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  // Ask this object's thread to exit. This function is asynchronous, when the
  // function returns the thread might still be running. Of course, this
  // function can be called from a different thread.
  virtual void requestExit();

  // Good place to do one-time initializations
  virtual MERROR readyToRun();

 private:
  // Derived class must implement threadLoop(). The thread starts its life
  // here. There are two ways of using the Thread object:
  // 1) loop: if threadLoop() returns true, it will be called again if
  //          requestExit() wasn't called.
  // 2) once: if threadLoop() returns false, the thread will exit upon return.
  bool threadLoop();
  bool _threadLoop();

  /* IErrorCallback interface */
 public:
  virtual status_t deviceError(void);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IAppStreamManager Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  virtual MVOID destroy();

  virtual MERROR configureStreams(camera3_stream_configuration_t* stream_list);

  virtual MERROR queryConfiguredStreams(ConfigAppStreams* rStreams) const;

  virtual MERROR createRequest(camera3_capture_request_t* request,
                               Request* rRequest);

  virtual MERROR registerRequest(Request const& rRequest);
  virtual MVOID updateResult(
      MUINT32 const frameNo,
      MINTPTR const userId,
      std::vector<std::shared_ptr<IMetaStreamBuffer>> resultMeta,
      bool hasLastPartial);

  virtual MERROR waitUntilDrained(int64_t timeout);

  virtual MERROR queryOldestRequestNumber(MUINT32* ReqNo);
};

/**
 * Frame Handler
 */
class AppStreamMgr::FrameHandler {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions:
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct FrameParcel;
  struct MetaItem;
  struct MetaItemSet;
  struct ImageItem;
  struct ImageItemSet;

  /**
   * IN_FLIGHT    -> PRE_RELEASE
   * IN_FLIGHT    -> VALID
   * IN_FLIGHT    -> ERROR
   * PRE_RELEASE  -> VALID
   * PRE_RELEASE  -> ERROR
   */
  struct State {
    enum T {
      IN_FLIGHT,
      PRE_RELEASE,
      VALID,
      ERROR,
    };
  };

  struct HistoryBit {
    enum {
      RETURNED,
      ERROR_SENT_FRAME,
      ERROR_SENT_META,
      ERROR_SENT_IMAGE,
    };
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Meta Stream
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct MetaItem {
    std::weak_ptr<FrameParcel> pFrame;
    MetaItemSet* pItemSet;
    State::T state;
    std::bitset<32> history;  // HistoryBit::xxx
    std::shared_ptr<IMetaStreamBuffer> buffer;
    MUINT32 bufferNo;  // partial_result
    //
    MetaItem()
        : pItemSet(nullptr), state(State::IN_FLIGHT), history(0), bufferNo(0) {}
  };

  struct MetaItemSet : public std::map<StreamId_T, std::shared_ptr<MetaItem>> {
    MBOOL asInput;
    size_t numReturnedStreams;
    size_t numValidStreams;
    size_t numErrorStreams;
    bool hasLastPartial;
    //
    explicit MetaItemSet(MBOOL _asInput)
        : asInput(_asInput),
          numReturnedStreams(0),
          numValidStreams(0),
          numErrorStreams(0),
          hasLastPartial(false) {}
  };

  struct MetaConfigItem {
    std::shared_ptr<AppMetaStreamInfo> pStreamInfo;
    //
    MetaConfigItem() {}
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Image Stream
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct ImageItemFrameQueue : public std::list<std::shared_ptr<ImageItem>> {
    ImageItemFrameQueue() {}
  };

  struct ImageItem {
    std::weak_ptr<FrameParcel> pFrame;
    ImageItemSet* pItemSet;
    State::T state;
    std::bitset<32> history;  // HistoryBit::xxx
    std::shared_ptr<AppImageStreamBuffer> buffer;
    ImageItemFrameQueue::iterator iter;
    //
    ImageItem() : pItemSet(nullptr), state(State::IN_FLIGHT), history(0) {}
  };

  struct ImageItemSet
      : public std::vector<std::pair<StreamId_T, std::shared_ptr<ImageItem>>> {
    MBOOL asInput;
    size_t numReturnedStreams;
    size_t numValidStreams;
    size_t numErrorStreams;
    //
    explicit ImageItemSet(MBOOL _asInput)
        : asInput(_asInput),
          numReturnedStreams(0),
          numValidStreams(0),
          numErrorStreams(0) {}
  };

  struct ImageConfigItem {
    std::shared_ptr<AppImageStreamInfo> pStreamInfo;
    ImageItemFrameQueue vItemFrameQueue;
    //
    ImageConfigItem() {}
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Frame Parcel & Queue
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct FrameParcel {
    ImageItemSet vOutputImageItem;
    ImageItemSet vInputImageItem;
    MetaItemSet vOutputMetaItem;
    MetaItemSet vInputMetaItem;
    MUINT32 frameNo;
    MUINT64 timestampShutter;
    bool bShutterCallbacked;
    std::bitset<32> errors;  // HistoryBit::ERROR_SENT_xxx
    //
    FrameParcel()
        //
        : vOutputImageItem(MFALSE),
          vInputImageItem(MTRUE),
          vOutputMetaItem(MFALSE),
          vInputMetaItem(MTRUE)
          //
          ,
          frameNo(0),
          timestampShutter(0),
          bShutterCallbacked(false),
          errors(0) {}
  };

  struct FrameQueue : public std::list<std::shared_ptr<FrameParcel>> {
    MUINT32 latestResultFrameNo;
    FrameQueue() : latestResultFrameNo(0) {}
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                Data Members.
  std::shared_ptr<IMetadataProvider> mpMetadataProvider;
  size_t mAtMostMetaStreamCount;

 protected:  ////                Data Members (CONFIG/REQUEST)
  FrameQueue mFrameQueue;

  std::map<StreamId_T, ImageConfigItem> mImageConfigMap;

  std::map<StreamId_T, MetaConfigItem> mMetaConfigMap;

 protected:  ////                cross mount
  MBOOL mIsExternal;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations: Request
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                Operations.
  MERROR registerStreamBuffers(
      std::map<StreamId_T, std::shared_ptr<AppImageStreamBuffer>> const&
          buffers,
      std::shared_ptr<FrameParcel> const pFrame,
      ImageItemSet* const pItemSet);
  MERROR registerStreamBuffers(
      std::map<StreamId_T, std::shared_ptr<IMetaStreamBuffer>> const& buffers,
      std::shared_ptr<FrameParcel> const pFrame,
      MetaItemSet* const pItemSet);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations: Result
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                Operations.
  /**
   * @param[in] frame: a given frame to check.
   *
   * @return
   *      ==0: uncertain
   *      > 0: it is indeed a request error
   *      < 0: it is indeed NOT a request error
   */
  static MINT checkRequestError(FrameParcel const& frame);

 protected:  ////                Operations.
  MVOID prepareErrorFrame(CallbackParcel* rCbParcel,
                          std::shared_ptr<FrameParcel> const& pFrame);
  MVOID prepareErrorMetaIfPossible(CallbackParcel* rCbParcel,
                                   std::shared_ptr<MetaItem> const& pItem);
  MVOID prepareErrorImage(CallbackParcel* rCbParcel,
                          std::shared_ptr<ImageItem> const& pItem);

 protected:  ////                Operations.
  bool isShutterReturnable(std::shared_ptr<MetaItem> const& pItem);
  bool getShutterTime(std::shared_ptr<MetaItem> const& pItem,
                      MINT64* timestamp);  // true: get; false: not found

  void updateShutterTimeIfPossible(std::shared_ptr<MetaItem> const& pItem);

  bool prepareShutterNotificationIfPossible(
      CallbackParcel* rCbParcel, std::shared_ptr<MetaItem> const& pItem);
  MVOID prepareReturnMeta(CallbackParcel* rCbParcel,
                          std::shared_ptr<MetaItem> const& pItem);
  MBOOL isReturnable(std::shared_ptr<MetaItem> const& pItem) const;

 protected:  ////                Operations.
  MVOID prepareReturnImage(CallbackParcel* rCbParcel,
                           std::shared_ptr<ImageItem> const& pItem);
  MBOOL isReturnable(std::shared_ptr<ImageItem> const& pItem) const;

 protected:  ////                Operations.
  MBOOL isFrameRemovable(std::shared_ptr<FrameParcel> const& pFrame) const;
  MBOOL prepareCallbackIfPossible(CallbackParcel* rCbParcel,
                                  MetaItemSet* rItemSet);
  MBOOL prepareCallbackIfPossible(CallbackParcel* rCbParcel,
                                  ImageItemSet* rItemSet);

 protected:  ////                Operations.
  MVOID updateItemSet(MetaItemSet const& rItemSet);
  MVOID updateItemSet(ImageItemSet const& rItemSet);
  MVOID update(ResultQueueT const& rvResult);
  MVOID update(std::list<CallbackParcel>* rCbList);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  FrameHandler(std::shared_ptr<IMetadataProvider> pMetadataProvider,
               MBOOL isExternal);

  MBOOL isEmptyFrameQueue() const;
  size_t getFrameQueueSize() const;
  MERROR queryOldestRequestNumber(MUINT32* ReqNo) const;

  MVOID addConfigStream(std::shared_ptr<AppImageStreamInfo> pStreamInfo);
  MVOID addConfigStream(std::shared_ptr<AppMetaStreamInfo> pStreamInfo);
  MERROR getConfigStreams(ConfigAppStreams* rStreams) const;
  std::shared_ptr<AppMetaStreamInfo> getConfigMetaStream(size_t index) const;

  MERROR registerFrame(Request const& rRequest);

  MVOID update(ResultQueueT const& rvResult,
               std::list<CallbackParcel>* rCbList);

  MVOID dump() const;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Imp
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_APP_APPSTREAMMGR_H_
