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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1UTILITY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1UTILITY_H_

#include <map>
#include <memory>
#include <mtkcam/utils/std/Profile.h>
#include "P1Common.h"
#include <queue>
#include <string>
#include <vector>

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
namespace NSCam {
namespace v3 {
namespace NSP1Node {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#if (IS_P1_LOGD)  // for system use LOGD
#define P1_TIMING_CHECK(str, timeout_ms, type)                        \
  std::shared_ptr<TimingChecker::Client> TimingCheckerClient =        \
      (mpTimingCheckerMgr != nullptr)                                 \
          ? (mpTimingCheckerMgr->createClient(str, timeout_ms, type)) \
          : (nullptr);
#else
#define P1_TIMING_CHECK(str, timeout_ms, type) \
  {}
#endif

#define TC_W TimingChecker::EVENT_TYPE_WARNING
#define TC_E TimingChecker::EVENT_TYPE_ERROR
#define TC_F TimingChecker::EVENT_TYPE_FATAL

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
#if MTKCAM_HAVE_SANDBOX_SUPPORT
NSCam::NSIoPipe::NSCamIOPipe::IV4L2PipeFactory* getNormalPipeModule();
#else
INormalPipeModule* getNormalPipeModule();
#endif

MUINT32 getResizeMaxRatio(MUINT32 imageFormat);

MBOOL calculateCropInfoFull(MUINT32 pixelMode,
                            MSize const& sensorSize,
                            MSize const& bufferSize,
                            MRect const& querySrcRect,
                            MRect* resultSrcRect,
                            MSize* resultDstSize,
                            MINT32 mLogLevelI = 0);

MBOOL calculateCropInfoResizer(MUINT32 pixelMode,
                               MUINT32 imageFormat,
                               MSize const& sensorSize,
                               MSize const& bufferSize,
                               MRect const& querySrcRect,
                               MRect* resultSrcRect,
                               MSize* resultDstSize,
                               MINT32 mLogLevelI = 0);

MBOOL verifySizeResizer(MUINT32 pixelMode,
                        MUINT32 imageFormat,
                        MSize const& sensorSize,
                        MSize const& streamBufSize,
                        MSize const& queryBufSize,
                        MSize* resultBufSize,
                        MINT32 mLogLevelI);

void queryRollingSkew(MUINT const openId, MINT64* nsRolling, MINT32 mLogLevelI);

void generateMetaInfoStr(IMetadata::IEntry const& entry, std::string* string);

void logMeta(MINT32 option,
             IMetadata const* pMeta,
             char const* pInfo,
             MUINT32 tag = 0x0);

#if 1
/******************************************************************************
 *
 ******************************************************************************/
class StuffBufferPool {
#define STUFF_BUFFER_WATER_MARK 8   // "0" the pool will not store buffer
#define STUFF_BUFFER_MAX_AMOUNT 16  // the max amount for general use case

  enum BUF_STATE { BUF_STATE_RELEASED = 0, BUF_STATE_ACQUIRED };

  struct BufNote {
   public:
    BufNote() : msName(""), mState(BUF_STATE_RELEASED) {}
    BufNote(std::string name, BUF_STATE state) : msName(name), mState(state) {}
    virtual ~BufNote() {}

    std::string msName;
    BUF_STATE mState;
  };

 public:
  StuffBufferPool()
      : mLogLevel(0),
        mLogLevelI(0),
        msName(""),
        mFormat(0),
        mSize(0, 0),
        mStride0(0),
        mStride1(0),
        mStride2(0),
        mPlaneCnt(0),
        mUsage(0),
        mSerialNum(0),
        mWaterMark(STUFF_BUFFER_WATER_MARK),
        mMaxAmount(STUFF_BUFFER_MAX_AMOUNT) {
    MY_LOGD("+++");
    mUsage = (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_CAMERA_READ |
              GRALLOC_USAGE_HW_CAMERA_WRITE);
    mvInfoMap.clear();
    MY_LOGD("loglevel %d %d ---", mLogLevel, mLogLevelI);
  }

  StuffBufferPool(char const* szName,
                  MINT32 format,
                  MSize size,
                  MUINT32 stride0,
                  MUINT32 stride1,
                  MUINT32 stride2,
                  MUINT8 multiple,  // for burst mode
                  MBOOL writable,   // for SW write
                  MINT32 log,
                  MINT32 logi)
      : mLogLevel(log),
        mLogLevelI(logi),
        msName(szName),
        mFormat(format),
        mSize(size),
        mStride0(stride0),
        mStride1(stride1),
        mStride2(stride2),
        mPlaneCnt(0),
        mUsage(0),
        mSerialNum(0),
        mWaterMark(STUFF_BUFFER_WATER_MARK * multiple),
        mMaxAmount(STUFF_BUFFER_MAX_AMOUNT * multiple) {
    MY_LOGD("+++");
    MY_LOGI(
        "[%s]"
        " 0x%x-%dx%d-%d.%d.%d *%d +%d",
        szName, format, size.w, size.h, stride0, stride1, stride2, multiple,
        writable);
    //
    if (mStride2 > 0) {
      if (mStride1 > 0 && mStride0 > 0) {
        mPlaneCnt = 3;
      }
    } else if (mStride1 > 0) {
      if (mStride0 > 0) {
        mPlaneCnt = 2;
      }
    } else if (mStride0 > 0) {
      mPlaneCnt = 1;
    }
    if (mPlaneCnt == 0) {
      MY_LOGW("[%s] stride invalid (%d.%d.%d)", msName.c_str(), mStride0,
              mStride1, mStride2);
    }
    //
    mUsage = (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_CAMERA_READ |
              GRALLOC_USAGE_HW_CAMERA_WRITE);
    if (writable) {
      mUsage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    }
    mvInfoMap.clear();
    MY_LOGD("loglevel %d %d ---", mLogLevel, mLogLevelI);
  }

  virtual ~StuffBufferPool() {
    MY_LOGD("+++");
    MY_LOGI("InfoMap.size(%zu)", mvInfoMap.size());
    while (mvInfoMap.size() > 0) {
      destroyBuffer((size_t)0);  // it remove one of mvInfoMap
    }
    mvInfoMap.clear();
    MY_LOGI("[%s] 0x%x-%dx%d-%d.%d.%d", msName.c_str(), mFormat, mSize.w,
            mSize.h, mStride0, mStride1, mStride2);
    MY_LOGD("---");
  }

  MBOOL compareLayout(MINT32 format,
                      MSize size,
                      MUINT32 stride0,
                      MUINT32 stride1,
                      MUINT32 stride2);

  MERROR acquireBuffer(std::shared_ptr<IImageBuffer>* imageBuffer);

  MERROR releaseBuffer(std::shared_ptr<IImageBuffer>* imageBuffer);

  MERROR createBuffer(std::shared_ptr<IImageBuffer>* imageBuffer);

  MERROR destroyBuffer(std::shared_ptr<IImageBuffer>* imageBuffer);

  MERROR destroyBuffer(size_t index);

 private:
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  std::string msName;
  MINT32 mFormat;
  MSize mSize;
  MUINT32 mStride0;
  MUINT32 mStride1;
  MUINT32 mStride2;
  MUINT8 mPlaneCnt;
  MUINT mUsage;
  MUINT32 mSerialNum;
  // it will destroy buffer while releasing, if pool_size > WaterMark
  MUINT32 mWaterMark;
  // it will not create buffer while acquiring, if pool_size >= MaxAmount
  MUINT32 mMaxAmount;
  std::map<std::shared_ptr<IImageBuffer>, BufNote> mvInfoMap;
};

/******************************************************************************
 *
 ******************************************************************************/
class StuffBufferManager {
 private:
  class InfoSet {
   public:
    InfoSet()
        : mOpenId(-1),
          mLogLevel(0),
          mLogLevelI(0),
          mFormat((MINT32)eImgFmt_UNKNOWN),
          mSize(),
          mvStride() {
      MY_LOGD("+++");
      mvStride.clear();
      mvStride.reserve(P1NODE_IMG_BUF_PLANE_CNT_MAX);
      MY_LOGD("---");
    }

    InfoSet(MINT32 id, MINT32 log, MINT32 logi)
        : mOpenId(id),
          mLogLevel(log),
          mLogLevelI(logi),
          mFormat((MINT32)eImgFmt_UNKNOWN),
          mSize(),
          mvStride() {
      MY_LOGD("+++");
      mvStride.clear();
      mvStride.reserve(P1NODE_IMG_BUF_PLANE_CNT_MAX);
      MY_LOGD("---");
    }

    virtual ~InfoSet() {
      MY_LOGD("+++");
      mvStride.clear();
      MY_LOGD("---");
    }

   public:
    MINT32 mOpenId;
    MINT32 mLogLevel;
    MINT32 mLogLevelI;
    MINT32 mFormat;
    MSize mSize;
    std::vector<MUINT32> mvStride;
  };

 public:
  StuffBufferManager() : mOpenId(-1), mLogLevel(0), mLogLevelI(0), mvPoolSet() {
    MY_LOGD("+++");
    mvPoolSet.clear();
    mvPoolSet.reserve(32);
    mvInfoSet.clear();
    mvInfoSet.reserve(32);
    MY_LOGD("---");
  }

  StuffBufferManager(MINT32 id, MINT32 log, MINT32 logi)
      : mOpenId(id), mLogLevel(log), mLogLevelI(logi) {
    MY_LOGD("+++");
    mvPoolSet.clear();
    mvPoolSet.reserve(32);
    mvInfoSet.clear();
    mvInfoSet.reserve(32);
    MY_LOGD("---");
  }

  virtual ~StuffBufferManager() {
    MY_LOGD("+++");
    MY_LOGD("InfoSet.size(%zu)", mvInfoSet.size());
    mvInfoSet.clear();
    MY_LOGD("PoolSet.size(%zu)", mvPoolSet.size());
    mvPoolSet.clear();
    MY_LOGD("---");
  }

  void setLog(MINT32 id, MINT32 log, MINT32 logi) {
    std::lock_guard<std::mutex> _l(mLock);
    mOpenId = id;
    mLogLevel = log;
    mLogLevelI = logi;
    MY_LOGD("StuffBufferManager id(%d) log(%d,%d)", id, log, logi);
  }

  MERROR acquireStoreBuffer(std::shared_ptr<IImageBuffer>* imageBuffer,
                            char const* szName,
                            MINT32 format,
                            MSize size,
                            std::vector<MUINT32> vStride,
                            MUINT8 multiple = 1,       // for burst mode
                            MBOOL writable = MFALSE);  // for SW write

  MERROR releaseStoreBuffer(std::shared_ptr<IImageBuffer>* imageBuffer);

  MERROR collectBufferInfo(MUINT32 pixelMode,
                           MBOOL isFull,
                           MINT32 format,
                           MSize size,
                           std::vector<MUINT32>* stride);

 private:
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  mutable std::mutex mLock;
  std::vector<std::shared_ptr<StuffBufferPool> > mvPoolSet;
  std::vector<InfoSet> mvInfoSet;
};
#endif

#if 1
/******************************************************************************
 *
 ******************************************************************************/
class TimingChecker {
 public:
  enum EVENT_TYPE {
    EVENT_TYPE_NONE = 0,
    EVENT_TYPE_WARNING,
    EVENT_TYPE_ERROR,
    EVENT_TYPE_FATAL
  };
  //
 public:
  class Client {
   public:
    Client(char const* str, MUINT32 uTimeoutMs, EVENT_TYPE eType)
        : mOpenId(-1),
          mLogLevel(0),
          mLogLevelI(0),
          mStr(str),
          mTimeInvMs(uTimeoutMs),
          mBeginTsNs(0),
          mEndTsNs(0),
          mType(eType) {
      mBeginTsNs = NSCam::Utils::getTimeInNs();
      mEndTsNs = mBeginTsNs + (ONE_MS_TO_NS * uTimeoutMs);
      setLog(mOpenId, mLogLevel, mLogLevelI);
      dump("TC_Client::CTR");
    }
    virtual ~Client() {
      dump("TC_Client::onLastStrongRef");
      dump("TC_Client::DTR");
    }
    //
   public:
    MUINT32 getTimeInterval(void) {
      std::lock_guard<std::mutex> _l(mLock);
      return mTimeInvMs;
    }
    int64_t getTimeStamp(void) {
      std::lock_guard<std::mutex> _l(mLock);
      return mEndTsNs;
    }
    void setLog(MINT32 id, MINT32 log, MINT32 logi) {
      std::lock_guard<std::mutex> _l(mLock);
      mOpenId = id;
      mLogLevel = log;
      mLogLevelI = logi;
      MY_LOGD("TimingChecker::Client id(%d) log(%d,%d)", id, log, logi);
      return;
    }
    //
   public:
    void dump(char const* tag = nullptr);
    void action(void);
    //
   private:
    mutable std::mutex mLock;
    MINT32 mOpenId;
    MINT32 mLogLevel;
    MINT32 mLogLevelI;
    std::string mStr;
    MUINT32 mTimeInvMs;  // TimeInterval in msec
    int64_t mBeginTsNs;  // Begin TimeStamp in nsec
    int64_t mEndTsNs;    // End TimeStamp in nsec
    EVENT_TYPE mType;
  };
  //
 private:
  class Record {
   public:
    Record(int64_t ns, std::weak_ptr<TimingChecker::Client> pc)
        : mTimeMarkNs(ns), mpClient(pc) {}
    virtual ~Record() {}
    int64_t mTimeMarkNs;
    std::weak_ptr<TimingChecker::Client> mpClient;
  };
  //
  typedef TimingChecker::Record* RecPtr;
  //
 private:
  class RecCmp {
   public:
    RecCmp() {}
    virtual ~RecCmp() {}
    MBOOL operator()(const RecPtr& rpL, const RecPtr& rpR) {
      return (rpL->mTimeMarkNs > rpR->mTimeMarkNs);
    }
  };
  //
 private:
  class RecStore {
   public:
    RecStore() : mHeap() {}
    virtual ~RecStore() {}
    size_t size(void);

    MBOOL isEmpty(void);

    MBOOL addRec(RecPtr rp);

    RecPtr const& getMin(void);

    void delMin(void);

    //
   private:
    void dump(char const* tag = nullptr);
    //
   private:
    std::priority_queue<RecPtr, std::vector<RecPtr>, RecCmp> mHeap;
  };
  //
 public:
  TimingChecker(MINT32 nOpenId, MINT32 nLogLevel, MINT32 nLogLevelI)
      : mOpenId(nOpenId),
        mLogLevel(nLogLevel),
        mLogLevelI(nLogLevelI),
        mWakeTiming(0),
        mExitPending(MFALSE),
        mRunning(MFALSE) {}

  virtual ~TimingChecker() { doRequestExit(); }
  //
 public:
  MBOOL doThreadLoop(void);

  void doRequestExit(void);

  void doWaitReady(void);
  //
 public:
  std::shared_ptr<TimingChecker::Client> createClient(char const* str,
                                                      MUINT32 uTimeoutMs,
                                                      EVENT_TYPE eType);
  //
 private:
  int64_t checkList(int64_t time);
  //
 private:
  mutable std::mutex mLock;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  std::condition_variable mClientCond;
  int64_t mWakeTiming;
  // for loop control
  MBOOL mExitPending;
  MBOOL mRunning;
  std::condition_variable mExitedCond;
  std::condition_variable mEnterCond;
  // for client record stroage
  RecStore mData;
};
#endif

/******************************************************************************
 *
 ******************************************************************************/
class TimingCheckerMgr {
 public:
  TimingCheckerMgr(MUINT32 factor,
                   MINT32 nOpenId,
                   MINT32 nLogLevel,
                   MINT32 nLogLevelI)
      : mOpenId(nOpenId),
        mLogLevel(nLogLevel),
        mLogLevelI(nLogLevelI),
        mIsEn(MFALSE),
        mFactor(factor),
        mpTimingChecker(nullptr) {
#if (IS_P1_LOGD)
    mpTimingChecker =
        std::make_shared<TimingChecker>(mOpenId, mLogLevel, mLogLevelI);
#endif
  }
  virtual ~TimingCheckerMgr() {}

 public:
  void setEnable(MBOOL en);

  void waitReady(void);

  void onCheck(void);

  std::shared_ptr<TimingChecker::Client> createClient(
      char const* str,
      MUINT32 uTimeoutMs,
      TimingChecker::EVENT_TYPE eType = TimingChecker::EVENT_TYPE_WARNING);
  //
 private:
  mutable std::mutex mLock;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MBOOL mIsEn;
  MUINT32 mFactor;
  std::shared_ptr<TimingChecker> mpTimingChecker;
};

/******************************************************************************
 *
 ******************************************************************************/
class LongExposureStatus {
#define P1_LONG_EXP_TIME_TH (500 * 1000000)  // (1ms = 1000000ns)

 public:
  LongExposureStatus()
      : mOpenId(-1),
        mLogLevel(0),
        mLogLevelI(0),
        mThreshold(P1_LONG_EXP_TIME_TH),
        mRunning(MFALSE) {
    mvSet.clear();
    mvSet.reserve(P1NODE_DEF_QUEUE_DEPTH);
  }

  virtual ~LongExposureStatus() {}

  MVOID config(MINT32 nOpenId, MINT32 nLogLevel, MINT32 nLogLevelI);

  MBOOL reset(MINT num);

  MBOOL set(MINT num, MINT64 exp_ns);

  MBOOL get(void);

 private:
  mutable std::mutex mLock;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MINT64 mThreshold;
  MBOOL mRunning;
  std::vector<MINT32> mvSet;
};

/******************************************************************************
 *
 ******************************************************************************/
enum STAGE_DONE {
  STAGE_DONE_START = 0,
  STAGE_DONE_INIT_ITEM,
  STAGE_DONE_TOTAL
};

/******************************************************************************
 *
 ******************************************************************************/
class ProcedureStageControl {
  class StageNote {
   public:
    explicit StageNote(MUINT32 uId)
        : mId(uId), mCond(), mWait(MFALSE), mDone(MFALSE), mSuccess(MFALSE) {}
    virtual ~StageNote() {
      std::lock_guard<std::mutex> _l(mLock);
      mDone = MTRUE;
      if (mWait) {
        mCond.notify_all();
      }
      mWait = MFALSE;
    }
    //
    MUINT32 mId;
    mutable std::mutex mLock;
    std::condition_variable mCond;
    MBOOL mWait;
    MBOOL mDone;
    MBOOL mSuccess;
  };

 public:
  ProcedureStageControl(MUINT32 nStageAmount,
                        MINT32 nLogLevel,
                        MINT32 nLogLevelI,
                        MINT32 nSysLevel)
      : mLogLevel(nLogLevel),
        mLogLevelI(nLogLevelI),
        mSysLevel(nSysLevel),
        mvpStage() {
    mvpStage.clear();
    mvpStage.reserve(nStageAmount);
    for (MUINT32 i = 0; i < nStageAmount; i++) {
      std::shared_ptr<StageNote> p = std::make_shared<StageNote>(i);
      mvpStage.push_back(p);
    }
    MY_LOGI("StageNum(%zu loglevel %d %d)", mvpStage.size(), mLogLevel,
            mLogLevelI);
  }

  virtual ~ProcedureStageControl() {
    mvpStage.clear();
    MY_LOGD("StageNum(%zu)", mvpStage.size());
  }

  MBOOL reset(void);

  MBOOL wait(MUINT32 eStage, MBOOL* rSuccess);

  MBOOL done(MUINT32 eStage, MBOOL bSuccess);

 private:
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MINT32 mSysLevel;
  std::vector<std::shared_ptr<StageNote> > mvpStage;
};

/******************************************************************************
 *
 ******************************************************************************/
class ConcurrenceControl {
 public:
  ConcurrenceControl(MINT32 nLogLevel, MINT32 nLogLevelI, MINT32 nSysLevel)
      : mLogLevel(nLogLevel),
        mLogLevelI(nLogLevelI),
        mSysLevel(nSysLevel),
        mIsAssistUsing(MFALSE),
        mpBufInfo(nullptr),
        mpStageCtrl(nullptr) {
    mpStageCtrl = std::make_shared<ProcedureStageControl>(
        STAGE_DONE_TOTAL, mLogLevel, mLogLevelI, mSysLevel);
    if (mpStageCtrl == nullptr) {
      MY_LOGE("ProcedureStageControl create fail");
    }
  }

  virtual ~ConcurrenceControl() { initBufInfo_clean(); }

  MBOOL initBufInfo_clean(void);

  MBOOL initBufInfo_get(NSCam::NSIoPipe::NSCamIOPipe::QBufInfo** ppBufInfo);

  MBOOL initBufInfo_create(NSCam::NSIoPipe::NSCamIOPipe::QBufInfo** ppBufInfo);

  void setAidUsage(MBOOL enable);

  MBOOL getAidUsage(void);

  void cleanAidStage(void);

  std::shared_ptr<ProcedureStageControl> getStageCtrl(void);

 private:
  mutable std::mutex mLock;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MINT32 mSysLevel;
  MBOOL mIsAssistUsing;
  NSCam::NSIoPipe::NSCamIOPipe::QBufInfo* mpBufInfo;
  std::shared_ptr<ProcedureStageControl> mpStageCtrl;
};

/******************************************************************************
 *
 ******************************************************************************/
class HardwareStateControl {
 public:
  enum STATE {
    STATE_NORMAL = 0,
    STATE_SUS_WAIT_NUM,   // received the suspend meta and wait the act magic
                          // number assign
    STATE_SUS_WAIT_SYNC,  // received the suspend meta and wait the 3A CB to set
                          // frame
    // STATE_SUS_WAITING,      // received the suspend meta and wait for the
    // 3A/DRV suspend function execution
    STATE_SUS_READY,      // already called 3A/DRV suspend function
    STATE_SUS_DONE,       // thread was blocked and wait resume
    STATE_RES_WAIT_NUM,   // received the resume meta and wait the act magic
                          // number assign
    STATE_RES_WAIT_SYNC,  // called 3A/DRV resume function and wait the 3A CB
                          // for EnQ
    STATE_RES_WAIT_DONE,  // received the 3A CB after 3A/DRV resume function and
                          // wait the previous frames done
    STATE_MAX
  };

 public:
  HardwareStateControl()
      : mOpenId(-1),
        mLogLevel(0),
        mLogLevelI(0),
        mSysLevel(P1_SYS_LV_DEFAULT),
        mBurstNum(1),
        mIsLegacyStandby(MFALSE),
        mState(STATE_NORMAL),
        mvStoreNum(),
        mStandbySetNum(0),
        mStreamingSetNum(0),
        mShutterTimeUs(0),
        mRequestPass(MFALSE),
        mRequestCond(),
        mThreadCond(),
        mpCamIO(nullptr),
        mp3A(nullptr) {
    mvStoreNum.clear();
    mvStoreNum.reserve(P1NODE_DEF_QUEUE_DEPTH);
  }

  virtual ~HardwareStateControl() { mvStoreNum.clear(); }

  void config(MINT32 nOpenId,
              MINT32 nLogLevel,
              MINT32 nLogLevelI,
              MINT32 nSysLevel,
              MUINT8 nBurstNum,
              NSCam::NSIoPipe::NSCamIOPipe::V4L2IIOPipe* pCamIO,
              std::shared_ptr<IHal3A_T> p3A,
              MBOOL isLegacyStandby);

  MBOOL isActive(void);

  SENSOR_STATUS_CTRL
  checkReceiveFrame(IMetadata* pMeta);

  MBOOL checkReceiveRestreaming(void);

  void checkShutterTime(MINT32* rShutterTimeUs);

  void checkRestreamingNum(MINT32 num);

  MBOOL checkCtrlStandby(MINT32 num);

  void checkRequest(void);

  void checkThreadStandby(void);

  void checkThreadWeakup(void);

  MBOOL checkFirstSync(void);

  MBOOL checkSkipSync(void);

  MBOOL checkSkipWait(void);

  MBOOL checkSkipBlock(void);

  MBOOL checkBufferState(void);

  MBOOL checkDoneNum(MINT32 num);

  void checkNotePass(MBOOL pass = MTRUE);

  void setDropNum(MINT32 num);

  MINT32 getDropNum(void);

  MBOOL isLegacyStandby(void);

  void reset(void);

  void clean(void);

  void dump(void);

 private:
  mutable std::mutex mLock;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MINT32 mSysLevel;
  MUINT8 mBurstNum;
  MBOOL mIsLegacyStandby;
  STATE mState;
  std::vector<MINT32> mvStoreNum;
  MINT32 mStandbySetNum;
  MINT32 mStreamingSetNum;
  MINT32 mShutterTimeUs;
  MBOOL mRequestPass;
  std::condition_variable mRequestCond;
  std::condition_variable mThreadCond;
  NSCam::NSIoPipe::NSCamIOPipe::V4L2IIOPipe* mpCamIO;
  std::shared_ptr<IHal3A_T> mp3A;
};

/******************************************************************************
 *
 ******************************************************************************/
class FrameNote {
 public:
  explicit FrameNote(MUINT32 capacity)
      : mLastTv(),
        mLastTid(0),
        mLastNum(P1NODE_FRAME_NOTE_NUM_UNKNOWN),
        mSlotCapacity(capacity),
        mSlotIndex(0),
        mvSlot() {
    pthread_rwlock_init(&mLock, NULL);
    mLastTv.tv_sec = 0;
    mLastTv.tv_usec = 0;
    if (CC_UNLIKELY(mSlotCapacity == 0)) {
      MY_LOGW("Capacity(%d)", mSlotCapacity);
      return;
    } else {  // (mSlotCapacity >= 1)
      mSlotIndex = mSlotCapacity -
                   1;  // mSlotIndex is the position of the last filled data
    }
    mvSlot.reserve((size_t)mSlotCapacity);
    for (MUINT32 i = 0; i < mSlotCapacity; i++) {
      mvSlot.push_back(P1NODE_FRAME_NOTE_NUM_UNKNOWN);
    }
  }

  virtual ~FrameNote() { pthread_rwlock_destroy(&mLock); }

  MVOID set(MINT32 num);

  MVOID get(std::string* pStr);

 private:
  mutable pthread_rwlock_t mLock;
  struct timeval mLastTv;
  MUINT32 mLastTid;
  MINT32 mLastNum;
  MUINT32 mSlotCapacity;
  MUINT32 mSlotIndex;
  std::vector<MINT32> mvSlot;
};

/******************************************************************************
 *
 ******************************************************************************/
class LogInfo {
 public:
#ifdef TEXT_LEN
#undef TEXT_LEN
#endif
#define TEXT_LEN (128)

#ifdef NOTE_LEN
#undef NOTE_LEN
#endif
#define NOTE_LEN (32)

#ifdef PARAM_NUM
#undef PARAM_NUM
#endif
#define PARAM_NUM (4)

#define CC_AMOUNT_MAX (64)  // (MUINT64)

#define CC_OP_TIMEOUT (MUINT64)(0x0000000000000001)
#define CC_OP_TIMEOUT_ALL (MUINT64)(0x000000000000FFFF)

#define CC_WAIT_OVERTIME (MUINT64)(0x0000000000010000)
#define CC_WAIT_OVERTIME_ALL (MUINT64)(0x00000000FFFF0000)

#define CC_DEDUCE (MUINT64)(0x0000000100000000)
#define CC_DEDUCE_ALL (MUINT64)(0xFFFFFFFF00000000)

#ifdef HAS
#undef HAS
#endif
#define HAS(type, code) ((mCode & ((MUINT64)(type << code))) > 0)

#ifdef ADD
#undef ADD
#endif
#define ADD(type, code)                 \
  do {                                  \
    mCode |= ((MUINT64)(type << code)); \
  } while (0);

#ifdef DIFF_NOW
#undef DIFF_NOW
#endif
#define DIFF_NOW(CP, DURATION) (mData.mNowTime > (mData.mTime[CP] + DURATION))

#ifdef CHECK_OP
#undef CHECK_OP
#endif
#define CHECK_OP(BGN, END, CODE)                   \
  if ((mData.mTime[BGN] > mData.mTime[END]) &&     \
      (DIFF_NOW(BGN, P1_GENERAL_OP_TIMEOUT_US))) { \
    ADD(CC_OP_TIMEOUT, CODE);                      \
  }

#ifdef CHECK_WAIT
#undef CHECK_WAIT
#endif
#define CHECK_WAIT(RET, REV, CODE)                    \
  if ((mData.mTime[RET] > mData.mTime[REV]) &&        \
      (DIFF_NOW(RET, P1_GENERAL_WAIT_OVERTIME_US))) { \
    ADD(CC_WAIT_OVERTIME, CODE);                      \
  }

#ifdef CHECK_STUCK
#undef CHECK_STUCK
#endif
#define CHECK_STUCK(BGN, END, CODE)                 \
  if ((mData.mTime[BGN] > mData.mTime[END]) &&      \
      (DIFF_NOW(BGN, P1_GENERAL_STUCK_JUDGE_US))) { \
    ADD(CC_DEDUCE, CODE);                           \
  }

  enum InspectType {
    IT_COMMON_DUMP = 0,
    IT_PERIODIC_CHECK,
    IT_STOP_NO_REQ_IN_GENERAL,  // stop but no request received, in the flow of
                                // START_SET_GENERAL
    IT_STOP_NO_REQ_IN_CAPTURE,  // stop but no request received, in the flow of
                                // START_SET_CAPTURE
    IT_STOP_NO_REQ_IN_REQUEST,  // stop but no request received, in the flow of
                                // START_SET_REQUEST
    IT_WAIT_CATURE,
    IT_WAIT_AAA_CB,
    IT_BUFFER_EXCEPTION,
    IT_NO_DELIVERY,
    IT_FLUSH_BLOCKING,
    IT_EVT_WAIT_DRAIN_TIMEOUT,
    IT_MAX
  };

  enum CheckPoint {
    CP_FIRST = 0,
    CP_REQ_ARRIVE = CP_FIRST,
    CP_REQ_ACCEPT,
    CP_REQ_REV,
    CP_REQ_RET,
    CP_REQ_NOTIFY_BGN,
    CP_REQ_NOTIFY_END,
    CP_CB_SYNC_REV,
    CP_CB_SYNC_RET,
    CP_CB_PROC_REV,  // CP_CB_DONE_REV,
    CP_CB_PROC_RET,  // CP_CB_DONE_RET,
    CP_START_SET_BGN,
    CP_START_SET_END,
    CP_PRE_SET_BGN,
    CP_PRE_SET_END,
    CP_SET_BGN,
    CP_SET_END,
    CP_BUF_BGN,
    CP_BUF_END,
    CP_ENQ_BGN,
    CP_ENQ_END,
    CP_DEQ_BGN,
    CP_DEQ_END,
    CP_OUT_BGN,
    CP_OUT_END,
    //
    CP_OP_START_BGN,
    CP_OP_START_3A_PWRON_BGN,
    CP_OP_START_3A_PWRON_END,
    CP_OP_START_3A_CFG_BGN,
    CP_OP_START_3A_CFG_END,
    CP_OP_START_3A_START_BGN,
    CP_OP_START_3A_START_END,
    CP_OP_START_DRV_INIT_BGN,
    CP_OP_START_DRV_INIT_END,
    CP_OP_START_DRV_CFG_BGN,
    CP_OP_START_DRV_CFG_END,
    CP_OP_START_DRV_START_BGN,
    CP_OP_START_DRV_START_END,
    CP_OP_START_REQ_WAIT_BGN,
    CP_OP_START_REQ_WAIT_END,
    CP_OP_START_END,
    //
    CP_OP_STOP_BGN,
    CP_OP_STOP_3A_PWROFF_BGN,
    CP_OP_STOP_3A_PWROFF_END,
    CP_OP_STOP_3A_STOPSTT_BGN,
    CP_OP_STOP_3A_STOPSTT_END,
    CP_OP_STOP_3A_STOP_BGN,
    CP_OP_STOP_3A_STOP_END,
    CP_OP_STOP_DRV_UNINIT_BGN,
    CP_OP_STOP_DRV_UNINIT_END,
    CP_OP_STOP_DRV_STOP_BGN,
    CP_OP_STOP_DRV_STOP_END,
    CP_OP_STOP_HW_LOCK_BGN,
    CP_OP_STOP_HW_LOCK_END,
    CP_OP_STOP_END,
    //
    CP_API_FLUSH_BGN,
    CP_API_FLUSH_END,
    CP_API_UNINIT_BGN,
    CP_API_UNINIT_END,
    //
    CP_MAX
  };

  enum StartSet { START_SET_GENERAL = 0, START_SET_CAPTURE, START_SET_REQUEST };

  enum CcOpTimeout {
    // for new item, please replace one of reservation
    CcOpTimeout_StartSet = 0,
    CcOpTimeout_PreSet,
    CcOpTimeout_Set,
    CcOpTimeout_Buf,
    CcOpTimeout_Enq,
    CcOpTimeout_Deq,
    CcOpTimeout_Dispatch,
    CcOpTimeout__Reserve07,
    CcOpTimeout__Reserve08,
    CcOpTimeout__Reserve09,
    CcOpTimeout__Reserve10,
    CcOpTimeout__Reserve11,
    CcOpTimeout__Reserve12,
    CcOpTimeout__Reserve13,
    CcOpTimeout__Reserve14,
    CcOpTimeout__Reserve15,
    CcOpTimeout_MAX  // = 16
  };
  static_assert((CcOpTimeout_MAX == 16), "CcOpTimeout_MAX != 16");

  enum CcWaitOvertime {
    // for new item, please replace one of reservation
    CcWaitOvertime_Request = 0,
    CcWaitOvertime_3aCbSyncDone,
    CcWaitOvertime_3aCbProcFinish,
    CcWaitOvertime__Reserve03,
    CcWaitOvertime__Reserve04,
    CcWaitOvertime__Reserve05,
    CcWaitOvertime__Reserve06,
    CcWaitOvertime__Reserve07,
    CcWaitOvertime__Reserve08,
    CcWaitOvertime__Reserve09,
    CcWaitOvertime__Reserve10,
    CcWaitOvertime__Reserve11,
    CcWaitOvertime__Reserve12,
    CcWaitOvertime__Reserve13,
    CcWaitOvertime__Reserve14,
    CcWaitOvertime__Reserve15,
    CcWaitOvertime_MAX  // = 16
  };
  static_assert((CcWaitOvertime_MAX == 16), "CcWaitOvertime_MAX != 16");

  enum CcDeduce {
    // for new item, please replace one of reservation
    CcDeduce_FwNoRequestAccept = 0,
    CcDeduce_3aNoFirstCbInGeneral,
    CcDeduce_3aNoFirstCbInCapture,
    CcDeduce_3aNoFirstCbInRequest,
    CcDeduce_3aStuckWithSet,
    CcDeduce_3aStuckWithBuf,
    CcDeduce_3aStuckWithEnq,
    CcDeduce_3aLookForCbSyncDone,
    CcDeduce_3aLookForCbProcFinish,
    CcDeduce_DrvDeqDelay,
    CcDeduce_OpStartBlocking,
    CcDeduce_OpStopBlocking,
    CcDeduce_UninitNotCalledAfterFlush,
    CcDeduce__Reserve13,
    CcDeduce__Reserve14,
    CcDeduce__Reserve15,
    CcDeduce__Reserve16,
    CcDeduce__Reserve17,
    CcDeduce__Reserve18,
    CcDeduce__Reserve19,
    CcDeduce__Reserve20,
    CcDeduce__Reserve21,
    CcDeduce__Reserve22,
    CcDeduce__Reserve23,
    CcDeduce__Reserve24,
    CcDeduce__Reserve25,
    CcDeduce__Reserve26,
    CcDeduce__Reserve27,
    CcDeduce__Reserve28,
    CcDeduce__Reserve29,
    CcDeduce__Reserve30,
    CcDeduce__Reserve31,
    CcDeduce_MAX  // = 32
  };
  static_assert((CcDeduce_MAX == 32), "CcDeduce_MAX != 32");

  /*
      ClueCodeListTable
      CCLT(A, B, C, D, E...)
      A: the ENUM of ClueCode in each type
      B: the type of ClueCode CC_OP_TIMEOUT/CC_WAIT_OVERTIME/...
      C: the string of ClueCode for message print
      D: the description of ClueCode for message print
      E...: the related note-tag(s) of ClueCode
  */
#define ClueCodeListTable                                                      \
  CCLT(CcOpTimeout_StartSet, (CC_OP_TIMEOUT), "CcOpTimeout_StartSet", "",      \
       CP_START_SET_BGN, CP_START_SET_END)                                     \
                                                                               \
  CCLT(CcOpTimeout_PreSet, (CC_OP_TIMEOUT), "CcOpTimeout_PreSet", "",          \
       CP_PRE_SET_BGN, CP_PRE_SET_END)                                         \
                                                                               \
  CCLT(CcOpTimeout_Set, (CC_OP_TIMEOUT), "CcOpTimeout_Set", "", CP_SET_BGN,    \
       CP_SET_END)                                                             \
                                                                               \
  CCLT(CcOpTimeout_Buf, (CC_OP_TIMEOUT), "CcOpTimeout_Buf", "", CP_BUF_BGN,    \
       CP_BUF_END)                                                             \
                                                                               \
  CCLT(CcOpTimeout_Enq, (CC_OP_TIMEOUT), "CcOpTimeout_Enq", "", CP_ENQ_BGN,    \
       CP_ENQ_END)                                                             \
                                                                               \
  CCLT(CcOpTimeout_Deq, (CC_OP_TIMEOUT), "CcOpTimeout_Deq", "", CP_DEQ_BGN,    \
       CP_DEQ_END)                                                             \
                                                                               \
  CCLT(CcOpTimeout_Dispatch, (CC_OP_TIMEOUT), "CcOpTimeout_Dispatch", "",      \
       CP_OUT_BGN, CP_OUT_END)                                                 \
                                                                               \
  CCLT(CcWaitOvertime_Request, (CC_WAIT_OVERTIME), "CcWaitOvertime_Request",   \
       "", CP_REQ_RET)                                                         \
                                                                               \
  CCLT(CcWaitOvertime_3aCbSyncDone, (CC_WAIT_OVERTIME),                        \
       "CcWaitOvertime_3aCbSyncDone", "", CP_CB_SYNC_RET)                      \
                                                                               \
  CCLT(CcWaitOvertime_3aCbProcFinish, (CC_WAIT_OVERTIME),                      \
       "CcWaitOvertime_3aCbProcFinish", "", CP_CB_PROC_RET)                    \
                                                                               \
  CCLT(CcDeduce_FwNoRequestAccept, (CC_DEDUCE), "CcDeduce_FwNoRequestAccept",  \
       "waiting for the next acceptable request by queue() from "              \
       "PipelineContext and PipelineModel",                                    \
       CP_REQ_ARRIVE, CP_REQ_ACCEPT, CP_REQ_REV, CP_REQ_RET,                   \
       CP_REQ_NOTIFY_BGN, CP_REQ_NOTIFY_END)                                   \
                                                                               \
  CCLT(CcDeduce_3aNoFirstCbInGeneral, (CC_DEDUCE),                             \
       "CcDeduce_3aNoFirstCbInGeneral",                                        \
       "cannot get the 3A first callback after the first general 3A.set()",    \
       CP_START_SET_BGN, CP_START_SET_END, CP_CB_PROC_REV, CP_CB_PROC_RET,     \
       CP_CB_SYNC_REV, CP_CB_SYNC_RET)                                         \
                                                                               \
  CCLT(CcDeduce_3aNoFirstCbInCapture, (CC_DEDUCE),                             \
       "CcDeduce_3aNoFirstCbInCapture",                                        \
       "cannot get the 3A first callback after 3A.startCapture()",             \
       CP_START_SET_BGN, CP_START_SET_END, CP_CB_PROC_REV, CP_CB_PROC_RET,     \
       CP_CB_SYNC_REV, CP_CB_SYNC_RET)                                         \
                                                                               \
  CCLT(CcDeduce_3aNoFirstCbInRequest, (CC_DEDUCE),                             \
       "CcDeduce_3aNoFirstCbInRequest",                                        \
       "cannot get the 3A first callback after 3A.startRequestQ()",            \
       CP_START_SET_BGN, CP_START_SET_END, CP_CB_PROC_REV, CP_CB_PROC_RET,     \
       CP_CB_SYNC_REV, CP_CB_SYNC_RET)                                         \
                                                                               \
  CCLT(CcDeduce_3aStuckWithSet, (CC_DEDUCE), "CcDeduce_3aStuckWithSet",        \
       "the 3A_CB_eID_NOTIFY_VSYNC_DONE is stuck with 3A.set() function",      \
       CP_SET_BGN)                                                             \
                                                                               \
  CCLT(CcDeduce_3aStuckWithBuf, (CC_DEDUCE), "CcDeduce_3aStuckWithBuf",        \
       "the 3A_CB_eID_NOTIFY_3APROC_FINISH is stuck with buffer acquiring",    \
       CP_BUF_BGN)                                                             \
                                                                               \
  CCLT(                                                                        \
      CcDeduce_3aStuckWithEnq, (CC_DEDUCE), "CcDeduce_3aStuckWithEnq",         \
      "the 3A_CB_eID_NOTIFY_3APROC_FINISH is stuck with DRV.enque() function", \
      CP_ENQ_BGN)                                                              \
                                                                               \
  CCLT(CcDeduce_3aLookForCbSyncDone, (CC_DEDUCE),                              \
       "CcDeduce_3aLookForCbSyncDone",                                         \
       "looking for the next 3A_CB_eID_NOTIFY_VSYNC_DONE", CP_CB_SYNC_REV,     \
       CP_CB_SYNC_RET, CP_CB_PROC_REV, CP_CB_PROC_RET)                         \
                                                                               \
  CCLT(CcDeduce_3aLookForCbProcFinish, (CC_DEDUCE),                            \
       "CcDeduce_3aLookForCbProcFinish",                                       \
       "looking for the next 3A_CB_eID_NOTIFY_3APROC_FINISH", CP_CB_PROC_REV,  \
       CP_CB_PROC_RET, CP_CB_SYNC_REV, CP_SET_BGN, CP_SET_END, CP_CB_SYNC_RET) \
                                                                               \
  CCLT(CcDeduce_DrvDeqDelay, (CC_DEDUCE), "CcDeduce_DrvDeqDelay",              \
       "the DRV.deque() function return delay", CP_DEQ_END, CP_ENQ_BGN,        \
       CP_ENQ_END, CP_DEQ_BGN)                                                 \
                                                                               \
  CCLT(CcDeduce_OpStartBlocking, (CC_DEDUCE), "CcDeduce_OpStartBlocking",      \
       "the operation of HW-Start flow is blocking in somewhere",              \
       CP_OP_START_BGN, CP_OP_START_3A_PWRON_BGN, CP_OP_START_3A_PWRON_END,    \
       CP_OP_START_3A_CFG_BGN, CP_OP_START_3A_CFG_END,                         \
       CP_OP_START_3A_START_BGN, CP_OP_START_3A_START_END,                     \
       CP_OP_START_DRV_INIT_BGN, CP_OP_START_DRV_INIT_END,                     \
       CP_OP_START_DRV_CFG_BGN, CP_OP_START_DRV_CFG_END,                       \
       CP_OP_START_DRV_START_BGN, CP_OP_START_DRV_START_END,                   \
       CP_OP_START_REQ_WAIT_BGN, CP_OP_START_REQ_WAIT_END, CP_START_SET_BGN,   \
       CP_START_SET_END, CP_OP_START_END)                                      \
                                                                               \
  CCLT(CcDeduce_OpStopBlocking, (CC_DEDUCE), "CcDeduce_OpStopBlocking",        \
       "the operation of HW-Stop flow is blocking in somewhere",               \
       CP_OP_STOP_BGN, CP_OP_STOP_3A_PWROFF_BGN, CP_OP_STOP_3A_PWROFF_END,     \
       CP_OP_STOP_3A_STOPSTT_BGN, CP_OP_STOP_3A_STOPSTT_END,                   \
       CP_OP_STOP_3A_STOP_BGN, CP_OP_STOP_3A_STOP_END,                         \
       CP_OP_STOP_DRV_UNINIT_BGN, CP_OP_STOP_DRV_UNINIT_END,                   \
       CP_OP_STOP_DRV_STOP_BGN, CP_OP_STOP_DRV_STOP_END,                       \
       CP_OP_STOP_HW_LOCK_BGN, CP_OP_STOP_HW_LOCK_END, CP_OP_STOP_END)         \
                                                                               \
  CCLT(CcDeduce_UninitNotCalledAfterFlush, (CC_DEDUCE),                        \
       "CcDeduce_UninitNotCalledAfterFlush",                                   \
       "the API function uninit() is not called after flush() done and the "   \
       "user also did not queue() acceptable request again",                   \
       CP_API_FLUSH_BGN, CP_API_FLUSH_END, CP_API_UNINIT_BGN,                  \
       CP_API_UNINIT_END, CP_REQ_ARRIVE, CP_REQ_ACCEPT, CP_REQ_REV,            \
       CP_REQ_RET)                                                             \
                                                                               \
  //
  //
#ifdef CC_ENUM
#undef CC_ENUM
#endif
#define CC_ENUM(code) e_##code
  //
#ifdef CCLT
#undef CCLT
#endif
#define CCLT(code, type, name, info, ...) \
  CC_ENUM(code) = (MUINT64)(type << code),
  enum ClueCode {
    CC_NONE = (MUINT64)(0x0000000000000000),
    //
    ClueCodeListTable
        //
        CC_ALL = (MUINT64)(0xFFFFFFFFFFFFFFFF)
  };
#undef CCLT

 public:
  class AutoMemo {
   public:
    AutoMemo(LogInfo* logInfo,
             LogInfo::CheckPoint cpEnter,
             LogInfo::CheckPoint cpExit,
             MINT64 param0 = 0,
             MINT64 param1 = 0,
             MINT64 param2 = 0,
             MINT64 param3 = 0)
        : mLogInfo(*logInfo),
          mCpEnter(cpEnter),
          mCpExit(cpExit),
          mP0(param0),
          mP1(param1),
          mP2(param2),
          mP3(param3) {
      mLogInfo.setMemo(mCpEnter, mP0, mP1, mP2, mP3);
    }
    virtual ~AutoMemo() { mLogInfo.setMemo(mCpExit, mP0, mP1, mP2, mP3); }

   private:
    LogInfo& mLogInfo;
    LogInfo::CheckPoint mCpEnter;
    LogInfo::CheckPoint mCpExit;
    MINT64 mP0;
    MINT64 mP1;
    MINT64 mP2;
    MINT64 mP3;
  };

 private:
  struct Slot {
   public:
    Slot() : mTid(0), mTv() {
      pthread_rwlock_init(&mLock, NULL);
      clear();
    }

    virtual ~Slot() { pthread_rwlock_destroy(&mLock); }

    void clear(void) {
      mTid = 0;
      mTv.tv_sec = 0;
      mTv.tv_usec = 0;
      for (int i = 0; i < PARAM_NUM; i++) {
        mParam[i] = 0;
      }
    }

    mutable pthread_rwlock_t mLock;
    MUINT32 mTid;
    struct timeval mTv;
    MINT64 mParam[PARAM_NUM];
  };

  struct Text {
    char main[TEXT_LEN];
  };

  struct Note {
    LogInfo::CheckPoint idx;
    char main[NOTE_LEN];
    char sub[PARAM_NUM][NOTE_LEN];
  };

  class Data {
   public:
    struct timeval mTv[CP_MAX];
    MINT64 mTime[CP_MAX];
    MUINT32 mTid[CP_MAX];
    //
    MBOOL mReady;
    struct timeval mNowTv;
    MINT64 mNowTime;
    MUINT32 mNowTid;
    //
    MINT32 mCbSyncType;
    MINT32 mCbProcType;
    //
    MINT32 mStartSetType;
    MINT32 mStartSetMn;
    //
    MINT32 mPreSetKey;
    //
    MINT32 mSetFn;
    //
    MINT32 mSetMn;
    MINT32 mEnqMn;
    MINT32 mDeqMn;
    //
    MINT32 mBufStream;
    MINT32 mBufMn;
    MINT32 mBufFn;
    MINT32 mBufRn;
    //
    MINT32 mAcceptFn;
    MINT32 mAcceptRn;
    MINT32 mAcceptResult;
    //
    MINT32 mRevFn;
    MINT32 mRevRn;
    //
    MINT32 mOutFn;
    MINT32 mOutRn;
    //
   public:
    Data()
        : mReady(MFALSE),
          mNowTv(),
          mNowTime(0),
          mNowTid(0)
          //
          ,
          mCbSyncType(0),
          mCbProcType(0)
          //
          ,
          mStartSetType(START_SET_GENERAL),
          mStartSetMn(P1_MAGIC_NUM_NULL)
          //
          ,
          mPreSetKey(P1_QUE_ID_NULL)
          //
          ,
          mSetFn(P1_FRM_NUM_NULL)
          //
          ,
          mSetMn(P1_MAGIC_NUM_NULL),
          mEnqMn(P1_MAGIC_NUM_NULL),
          mDeqMn(P1_MAGIC_NUM_NULL)
          //
          ,
          mBufStream(-1),
          mBufMn(P1_MAGIC_NUM_NULL),
          mBufFn(P1_FRM_NUM_NULL),
          mBufRn(P1_REQ_NUM_NULL)
          //
          ,
          mAcceptFn(P1_FRM_NUM_NULL),
          mAcceptRn(P1_REQ_NUM_NULL),
          mAcceptResult(REQ_REV_RES_UNKNOWN)
          //
          ,
          mRevFn(P1_FRM_NUM_NULL),
          mRevRn(P1_REQ_NUM_NULL)
          //
          ,
          mOutFn(P1_FRM_NUM_NULL),
          mOutRn(P1_REQ_NUM_NULL) {
      memset(mTv, 0, sizeof(mTv));
      memset(mTime, 0, sizeof(mTime));
      memset(mTid, 0, sizeof(mTid));
      mNowTv.tv_sec = 0;
      mNowTv.tv_usec = 0;
    }
    virtual ~Data() {}
  };

 public:
  LogInfo()
      : mOpenId(-1),
        mLogLevel(0),
        mLogLevelI(0),
        mBurstNum(1),
        mPid(0),
        mIsActive(MFALSE),
        mData(),
        mCode(CC_NONE) {
    pthread_rwlock_init(&mLock, NULL);
    mPid = (MUINT32)getpid();
    clear();
  }

  virtual ~LogInfo() {
    if (CC_UNLIKELY(mLogLevel > 9 || mLogLevelI > 9)) {
      inspect(LogInfo::IT_COMMON_DUMP);
    }
    clear();
    pthread_rwlock_destroy(&mLock);
  }

  void clear(void);

  void config(MINT32 nOpenId,
              MINT32 nLogLevel,
              MINT32 nLogLevelI,
              MUINT8 nBurstNum) {
    mOpenId = nOpenId;
    mLogLevel = nLogLevel;
    mLogLevelI = nLogLevelI;
    mBurstNum = nBurstNum;
    clear();
  }

  void setActive(MBOOL enable) {
    mIsActive = enable;
    return;
  }

  MBOOL getActive(void) { return (mIsActive); }

  void setMemo(LogInfo::CheckPoint cp,
               MINT64 param0 = 0,
               MINT64 param1 = 0,
               MINT64 param2 = 0,
               MINT64 param3 = 0);

  void getMemo(LogInfo::CheckPoint cp, std::string* str);

  void inspect(LogInfo::InspectType type = IT_COMMON_DUMP,
               char const* info = nullptr);

 protected:
  void write(LogInfo::CheckPoint cp,
             MINT64 param0 = 0,
             MINT64 param1 = 0,
             MINT64 param2 = 0,
             MINT64 param3 = 0);

  void read(LogInfo::CheckPoint cp, std::string* str);

  void reset(void) {
    mCode = CC_NONE;
    mData.mReady = MFALSE;
  }

  void extract(void);

  void analyze(MBOOL bForceToPrint = MFALSE);

  void bitStr(MUINT64 bitClueCode, std::string* str) {
    if (CC_UNLIKELY(str == nullptr))
      return;
    switch (bitClueCode) {
#ifdef CCLT
#undef CCLT
#endif
#define CCLT(code, type, name, info, ...)             \
  case CC_ENUM(code):                                 \
    base::StringAppendF(str, "[%s] %s ", name, info); \
    break;
      //
      ClueCodeListTable
      //
#undef CCLT
          default : break;
    }
    //
    std::vector<LogInfo::CheckPoint> vTag;
    vTag.clear();
    bitTag(bitClueCode, &vTag);
    if (vTag.size() > 0) {
      base::StringAppendF(str, " - reference tag ");
      for (size_t i = 0; i < vTag.size(); i++) {
        if (vTag[i] < LogInfo::CP_MAX) {
          base::StringAppendF(str, "<%s> ", mNotes[vTag[i]].main);
        }
      }
    }
  }

  void bitTag(MUINT64 bitClueCode, std::vector<LogInfo::CheckPoint>* rvTag) {
    rvTag->clear();
    switch (bitClueCode) {
#ifdef CCLT
#undef CCLT
#endif
#define CCLT(code, type, name, info, ...) \
  case CC_ENUM(code):                     \
    *(rvTag) = {__VA_ARGS__};             \
    break;
      //
      ClueCodeListTable
      //
#undef CCLT
          default : break;
    }
  }

 private:
  mutable pthread_rwlock_t mLock;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MUINT8 mBurstNum;
  MUINT32 mPid;
  MBOOL mIsActive;

  // if the text string length is over TEXT_LEN,
  // or the text element number is over IT_MAX,
  // it should be discovered in the compile time.
  Text mTexts[IT_MAX] = {
      /* IT_COMMON_DUMP */
      {"check in common case and dump"},

      // IT_PERIODIC_CHECK
      {"check the status in the regular periodic timing"},

      // IT_STOP_NO_REQ_IN_GENERAL
      {"check while stop and request not arrival in general preview flow"},

      // IT_STOP_NO_REQ_IN_CAPTURE
      {"check while stop but request not ready in start capture flow"},

      // IT_STOP_NO_REQ_IN_REQUEST
      {"check while stop but request not ready in initial request flow"},

      // IT_WAIT_CATURE
      {"check as start capture flow waiting AAA-CB"},

      // IT_WAIT_AAA_CB
      {"check as queue waiting AAA-CB"},

      // IT_BUFFER_EXCEPTION
      {"check as the image buffer cannot acquire"},

      // IT_NO_DELIVERY
      {"check since no more frame delivery"},

      // IT_FLUSH_BLOCKING
      {"check since wait flush but timeout"},

      // IT_EVT_WAIT_DRAIN_TIMEOUT
      {"check since IO event inform streaming off but wait request drain "
       "timeout"},
  };

  // if the note string length is over NOTE_LEN,
  // or the note element number is over CP_MAX,
  // it should be discovered in the compile time.
  Note mNotes[CP_MAX] = {
      {CP_REQ_ARRIVE, "Queue@Arrive", {"FrameNum", "RequestNum", "", ""}},

      {CP_REQ_ACCEPT,
       "Queue@Accept",
       {"FrameNum", "RequestNum", "IsAccepted", "ReceivingResult"}},

      {CP_REQ_REV, "Queue@REV", {"FrameNum", "RequestNum", "", ""}},

      {CP_REQ_RET, "Queue@RET", {"FrameNum", "RequestNum", "", ""}},

      {CP_REQ_NOTIFY_BGN,
       "ReqNotify+++",
       {"LastFrameNum", "LastRequestNum", "PipelineCbButNotQueueCnt", ""}},

      {CP_REQ_NOTIFY_END,
       "ReqNotify---",
       {"LastFrameNum", "LastRequestNum", "PipelineCbButNotQueueCnt", ""}},

      {CP_CB_SYNC_REV, "3A_CB_SYNC@REV", {"MsgType", "", "", ""}},

      {CP_CB_SYNC_RET, "3A_CB_SYNC@RET", {"MsgType", "Skip", "", ""}},

      {CP_CB_PROC_REV, "3A_CB_PROC@REV", {"MsgType", "MagicNum", "SofIdx", ""}},

      {CP_CB_PROC_RET, "3A_CB_PROC@RET", {"MsgType", "Skip", "", ""}},

      {CP_START_SET_BGN, "3A.StartSet+++", {"Type", "MagicNum", "", ""}},

      {CP_START_SET_END, "3A.StartSet---", {"Type", "MagicNum", "", ""}},

      {CP_PRE_SET_BGN,
       "3A.PreSet+++",
       {"PreSetKey", "Dummy", "FrameNum", "RequestNum"}},

      {CP_PRE_SET_END,
       "3A.PreSet---",
       {"PreSetKey", "Dummy", "FrameNum", "RequestNum"}},

      {CP_SET_BGN,
       "3A.Set+++",
       {"PreSetKey", "MagicNum", "FrameNum", "RequestNum"}},

      {CP_SET_END,
       "3A.Set---",
       {"PreSetKey", "MagicNum", "FrameNum", "RequestNum"}},

      {CP_BUF_BGN,
       "AcqBuf+++",
       {"StreamNum", "StreamId", "FrameNum", "RequestNum"}},

      {CP_BUF_END,
       "AcqBuf---",
       {"StreamNum", "StreamId", "FrameNum", "RequestNum"}},

      {CP_ENQ_BGN,
       "DRV.EnQ+++",
       {"MagicNum", "FrameNum", "RequestNum", "SofIdx"}},

      {CP_ENQ_END,
       "DRV.EnQ---",
       {"MagicNum", "FrameNum", "RequestNum", "SofIdx"}},

      {CP_DEQ_BGN, "DRV.DeQ+++", {"", "", "", ""}},

      {CP_DEQ_END, "DRV.DeQ---", {"ResultMagicNum", "", "", ""}},

      {CP_OUT_BGN,
       "DispatchNext+++",
       {"MagicNum", "FrameNum", "RequestNum", ""}},

      {CP_OUT_END,
       "DispatchNext---",
       {"MagicNum", "FrameNum", "RequestNum", ""}},

      // OPs Checking
      // OP_START

      {CP_OP_START_BGN, "OpStart+++", {"BurstMode", "StartCap", "InitReq", ""}},

      {CP_OP_START_3A_PWRON_BGN, "OpStart.3aPwrOn+++", {"", "", "", ""}},

      {CP_OP_START_3A_PWRON_END, "OpStart.3aPwrOn---", {"", "", "", ""}},

      {CP_OP_START_3A_CFG_BGN, "OpStart.3aCfg+++", {"", "", "", ""}},

      {CP_OP_START_3A_CFG_END, "OpStart.3aCfg---", {"", "", "", ""}},

      {CP_OP_START_3A_START_BGN, "OpStart.3aStart+++", {"", "", "", ""}},

      {CP_OP_START_3A_START_END, "OpStart.3aStart---", {"", "", "", ""}},

      {CP_OP_START_DRV_INIT_BGN, "OpStart.DrvInit+++", {"", "", "", ""}},

      {CP_OP_START_DRV_INIT_END, "OpStart.DrvInit---", {"", "", "", ""}},

      {CP_OP_START_DRV_CFG_BGN, "OpStart.DrvCfg+++", {"", "", "", ""}},

      {CP_OP_START_DRV_CFG_END, "OpStart.DrvCfg---", {"", "", "", ""}},

      {CP_OP_START_DRV_START_BGN, "OpStart.DrvStart+++", {"", "", "", ""}},

      {CP_OP_START_DRV_START_END, "OpStart.DrvStart---", {"", "", "", ""}},

      {CP_OP_START_REQ_WAIT_BGN, "OpStart.ReqWait+++", {"Type", "", "", ""}},

      {CP_OP_START_REQ_WAIT_END, "OpStart.ReqWait---", {"Type", "", "", ""}},

      {CP_OP_START_END,
       "OpStart---",
       {"BurstMode", "StartCap", "InitReq", "Type"}},

      // OP_STOP
      {CP_OP_STOP_BGN,
       "OpStop+++",
       {"LastFrameNum", "LastRequestNum", "PipelineCbButNotQueueCnt", ""}},

      {CP_OP_STOP_3A_PWROFF_BGN, "OpStop.3aPwrOff+++", {"", "", "", ""}},

      {CP_OP_STOP_3A_PWROFF_END, "OpStop.3aPwrOff---", {"", "", "", ""}},

      {CP_OP_STOP_3A_STOPSTT_BGN, "OpStop.3aStopStt+++", {"", "", "", ""}},

      {CP_OP_STOP_3A_STOPSTT_END, "OpStop.3aStopStt---", {"", "", "", ""}},

      {CP_OP_STOP_3A_STOP_BGN, "OpStop.3aStop+++", {"", "", "", ""}},

      {CP_OP_STOP_3A_STOP_END, "OpStop.3aStop---", {"", "", "", ""}},

      {CP_OP_STOP_DRV_UNINIT_BGN, "OpStop.DrvUninit+++", {"", "", "", ""}},

      {CP_OP_STOP_DRV_UNINIT_END, "OpStop.DrvUninit---", {"", "", "", ""}},

      {CP_OP_STOP_DRV_STOP_BGN, "OpStop.DrvStop+++", {"", "", "", ""}},

      {CP_OP_STOP_DRV_STOP_END, "OpStop.DrvStop---", {"IsAbort", "", "", ""}},

      {CP_OP_STOP_HW_LOCK_BGN, "OpStop.HwLock+++", {"", "", "", ""}},

      {CP_OP_STOP_HW_LOCK_END, "OpStop.HwLock---", {"", "", "", ""}},

      {CP_OP_STOP_END,
       "OpStop---",
       {"LastFrameNum", "LastRequestNum", "PipelineCbButNotQueueCnt", ""}},

      // API Checking
      {CP_API_FLUSH_BGN, "ApiFlush+++", {"", "", "", ""}},

      {CP_API_FLUSH_END, "ApiFlush---", {"", "", "", ""}},

      {CP_API_UNINIT_BGN, "ApiUninit+++", {"", "", "", ""}},

      {CP_API_UNINIT_END, "ApiUninit---", {"", "", "", ""}},
  };

  Slot mSlots[CP_MAX];
  Data mData;
  MUINT64 mCode;  // ClueCode
};

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1UTILITY_H_
