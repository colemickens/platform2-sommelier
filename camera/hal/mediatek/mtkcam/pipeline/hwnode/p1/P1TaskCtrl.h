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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1TASKCTRL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1TASKCTRL_H_

#define LOG_TAG "MtkCam/P1NodeImp"
//
#include <memory>
#include "P1Common.h"
#include "P1Utility.h"
#include <string>
#include <vector>
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
namespace NSCam {
namespace v3 {
namespace NSP1Node {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#if 1
#define P1ACT_ID_NULL (P1_QUE_ID_NULL)
#endif

#if 1
#define P1ACT_ID_FIRST (P1_QUE_ID_FIRST)
#endif

#if 1
#define P1ACT_NUM_NULL (P1_MAGIC_NUM_NULL)
#endif

#define P1ACT_NUM_FIRST (P1_MAGIC_NUM_FIRST)

#define RET_VOID /* return void */

#define GET_ACT_PTR(pAct, qAct, ret)          \
  (qAct).ptr();                               \
  if (CC_UNLIKELY(pAct == nullptr)) {         \
    MY_LOGE("#%d can-not-get-act", __LINE__); \
    return ret;                               \
  }

#define P1INFO_ACT_STREAM(TYPE)                                      \
  MY_LOGD(P1INFO_STREAM_STR P1INFO_ACT_STR, P1INFO_STREAM_VAR(TYPE), \
          P1INFO_ACT_VAR(*this));

#define P1_CHECK_ACT_STREAM(TYPE, stream) \
  P1_CHECK_MAP_STREAM(TYPE, (*this), stream)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
class P1NodeImp;

/******************************************************************************
 *
 ******************************************************************************/
class P1NodeAct {  // // use-act-ptr
  //
  enum STREAM_BUF_LOCK {
    STREAM_BUF_LOCK_NONE = 0,
    STREAM_BUF_LOCK_R,
    STREAM_BUF_LOCK_W
  };
  //
  struct ActStreamMeta {
    MBOOL bExist;
    MBOOL bWrote;
    std::shared_ptr<IMetaStreamBuffer> spStreamBuf;
    STREAM_BUF_LOCK eLockState;
    IMetadata* pMetadata;
    ActStreamMeta()
        : bExist(MFALSE),
          bWrote(MFALSE),
          spStreamBuf(nullptr),
          eLockState(STREAM_BUF_LOCK_NONE),
          pMetadata(nullptr) {}
  };

  struct ActStreamImage {
    MBOOL bExist;
    MBOOL bWrote;
    std::shared_ptr<IImageStreamBuffer> spStreamBuf;
    STREAM_BUF_LOCK eLockState;
    // std::shared_ptr<IImageBufferHeap>          spHeap;
    std::shared_ptr<IImageBuffer> spImgBuf;
    IMG_BUF_SRC eSrcType;

    ActStreamImage()
        : bExist(MFALSE),
          bWrote(MFALSE),
          spStreamBuf(nullptr),
          eLockState(STREAM_BUF_LOCK_NONE)
          //, spHeap(NULL)
          ,
          spImgBuf(nullptr),
          eSrcType(IMG_BUF_SRC_NULL) {}
  };

 public:
  explicit P1NodeAct(P1NodeImp* pP1NodeImp, MINT32 id = P1ACT_ID_NULL);

  virtual ~P1NodeAct();

 public:
  char const* getNodeName(void) const;

  IPipelineNode::NodeId_T getNodeId() const;

  MINT32 getOpenId(void) const;

 public:
  MINT32 getNum(void) const;

  ACT_TYPE getType(void) const;

  MBOOL getFlush(FLUSH_TYPE type = FLUSH_ALL) const;

  MVOID setFlush(FLUSH_TYPE type);

  MERROR mapFrameStream(void);

  MERROR frameMetadataInit(
      STREAM_META const streamMeta,
      std::shared_ptr<IMetaStreamBuffer>* pMetaStreamBuffer);

  MERROR frameMetadataGet(STREAM_META const streamMeta,
                          IMetadata* pOutMetadata,
                          MBOOL toWrite = MFALSE,
                          IMetadata* pInMetadata = nullptr);

  MERROR frameMetadataPut(STREAM_META const streamMeta);

  MERROR frameImageInit(
      STREAM_IMG const streamImg,
      std::shared_ptr<IImageStreamBuffer>* pImageStreamBuffer);

  MERROR frameImageGet(STREAM_IMG const streamImg,
                       std::shared_ptr<IImageBuffer>* rImgBuf);

  MERROR frameImagePut(STREAM_IMG const streamImg);

  MERROR poolImageGet(STREAM_IMG const streamImg,
                      std::shared_ptr<IImageBuffer>* rImgBuf);

  MERROR poolImagePut(STREAM_IMG const streamImg);

  MERROR stuffImageGet(STREAM_IMG const streamImg,
                       MSize const dstSize,
                       std::shared_ptr<IImageBuffer>* rImgBuf);

  MERROR stuffImagePut(STREAM_IMG const streamImg);

  MVOID updateMetaSet(void);

 public:
  // for performance consideration, this act do not hold the sp of P1NodeImp
  // and the P1NodeImp instance should be held in P1TaskCtrl.mspP1NodeImp
  // therefore, it must keep the life-cycle of P1NodeAct within P1TaskCtrl
  P1NodeImp* mpP1NodeImp;
  std::string mNodeName;
  IPipelineNode::NodeId_T mNodeId;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MINT32 queId;
  MINT32 magicNum;
  MINT32 frmNum;
  MINT32 reqNum;
  MUINT32 sofIdx;  // data type - refer:
                   // NSCam::NSIoPipe::NSCamIOPipe::BufInfo.FrameBased.mSOFidx
  std::shared_ptr<IPipelineFrame> appFrame;
  std::shared_ptr<IImageBuffer> buffer_eiso;
  // std::shared_ptr<IImageBuffer>          buffer_lcso; // no use, need remove
  // when lcso run imageStream
  ActStreamImage streamBufImg[STREAM_IMG_NUM];
  ActStreamMeta streamBufMeta[STREAM_META_NUM];
  MUINT32 portBufIndex[P1_OUTPUT_PORT_TOTAL];
  MUINT32 reqType;            /*REQ_TYPE*/
  MUINT32 reqOutSet;          /*REQ_SET(REQ_OUT)*/
  MUINT32 expRec;             /*EXP_REC(EXP_EVT)*/
  MUINT32 flushSet;           /*FLUSH_TYPE*/
  MUINT32 exeState;           /*EXE_STATE*/
  MUINT32 capType;            /*E_CAPTURE_TYPE*/
  MUINT32 uniSwitchState;     /*UNI_SWITCH_STATE*/
  MUINT32 tgSwitchState;      /*TG_SWITCH_STATE*/
  MUINT32 tgSwitchNum;        /*TG_SWITCH_NUM*/
  MUINT32 qualitySwitchState; /*QUALITY_SWITCH_STATE*/
  SENSOR_STATUS_CTRL ctrlSensorStatus;
  MINT64 frameExpDuration;
  MINT64 frameTimeStamp;
  MINT64 frameTimeStampBoot;
  MBOOL isMapped;
  MBOOL isReadoutReady;
  MBOOL isRawTypeChanged;
  MINT32 fullRawType;
  MSize refBinSize;
  MSize dstSize_full;
  MSize dstSize_resizer;
  MRect cropRect_full;
  MRect cropRect_resizer;
  NS3Av3::MetaSet_T metaSet;
  std::string msg;
  std::string res;
  MINT mReqFmt_Imgo;
  MINT mReqFmt_Rrzo;
};

typedef std::shared_ptr<P1NodeAct> P1Act;  // // use-act-ptr

/******************************************************************************
 *
 ******************************************************************************/
class P1QueAct {
  friend class P1TaskCtrl;
  friend class P1TaskCollector;

 public:
  P1QueAct() : mKeyId(P1ACT_ID_NULL), mpAct(nullptr) {}

  explicit P1QueAct(MINT32 id) : mKeyId(id), mpAct(nullptr) {}

  P1QueAct(P1Act pAct, MINT32 id) : mKeyId(id), mpAct(pAct) {}

  virtual ~P1QueAct() {}

 public:
  MINT32 id() { return mKeyId; }

  P1Act ptr() {
    if (CC_LIKELY(mpAct != nullptr)) {
      return mpAct;
    } else {
      MY_LOGI("Act not ready (%d)", mKeyId);
    }
    return nullptr;
  }

  MINT32 getNum() {
    if (CC_UNLIKELY(mpAct == nullptr)) {
      return P1ACT_NUM_NULL;
    }
    return mpAct->getNum();
  }

  ACT_TYPE getType() {
    if (CC_UNLIKELY(mpAct == nullptr)) {
      return ACT_TYPE_NULL;
    }
    return mpAct->getType();
  }

 protected:
  void set(P1Act ptr, MINT32 id) {
    if (CC_UNLIKELY(id == P1ACT_ID_NULL || ptr == NULL)) {
      MY_LOGI("invalid (%d)[%d]", id, (ptr == nullptr) ? 0 : 1);
      return;
    }
    if (CC_UNLIKELY(mKeyId != P1ACT_ID_NULL || mpAct != nullptr)) {
      MY_LOGI("already (%d)[%d]", mKeyId, (mpAct == nullptr) ? 0 : 1);
      return;
    }
    mKeyId = id;
    mpAct = ptr;
    return;
  }

 private:
  MINT32 mKeyId;
  P1Act mpAct;
};

/******************************************************************************
 *
 ******************************************************************************/
class P1QueJob {
 public:
  P1QueJob() : mSet(), mMaxNum(1), mFirstMagicNum(P1ACT_ID_NULL) {
    mSet.clear();
    mSet.reserve(mMaxNum);
  }

  explicit P1QueJob(MUINT8 num)
      : mSet(), mMaxNum(num), mFirstMagicNum(P1ACT_ID_NULL) {
    mSet.clear();
    mSet.reserve(mMaxNum);
  }

  virtual ~P1QueJob() { mSet.clear(); }

 public:
  MVOID setIdx(MINT32 idx) { mFirstMagicNum = idx; }

  MINT32 getIdx(void) const { return mFirstMagicNum; }

  MUINT8 getMax(void) const { return mMaxNum; }

  MVOID config(MUINT8 maxNum) {
    mFirstMagicNum = P1ACT_ID_NULL;
    mMaxNum = maxNum;
    mSet.clear();
    mSet.reserve(mMaxNum);
  }

  MVOID clear(void) {
    mFirstMagicNum = P1ACT_ID_NULL;
    return mSet.clear();
  }

  MBOOL ready(void) {
    return (mSet.size() == mMaxNum && mFirstMagicNum != P1ACT_ID_NULL);
  }

  MBOOL empty(void) const { return mSet.empty(); }

  size_t size(void) const { return mSet.size(); }

  MVOID push(P1QueAct qAct) {
    if (mSet.size() < mMaxNum) {
      return mSet.push_back(qAct);
    }
  }

  P1QueAct& edit(MUINT8 index) { return mSet.at(index); }

  const P1QueAct& at(MUINT8 index) { return mSet.at(index); }

  MINT32 getLastId(void) {
    if (mSet.size() < mMaxNum) {
      return P1ACT_ID_NULL;
    }
    return mSet.at(mMaxNum - 1).id();
  }

  MINT32 getLastNum(void) {
    if (mSet.size() < mMaxNum) {
      return P1ACT_NUM_NULL;
    }
    return mSet.at(mMaxNum - 1).getNum();
  }

  P1QueAct* getLastAct(void) {
    if (mSet.size() < mMaxNum) {
      return nullptr;
    }
    P1QueAct* qAct = &(mSet.at(mSet.size() - 1));
    return qAct;
  }

 private:
  std::vector<P1QueAct> mSet;
  MUINT8 mMaxNum;
  MINT32 mFirstMagicNum;
};

/******************************************************************************
 *
 ******************************************************************************/
class P1FrameAct {
 public:
  explicit P1FrameAct(P1QueAct* rAct)
      : queId(P1ACT_ID_NULL),
        magicNum(P1ACT_NUM_NULL),
        frmNum(P1_FRM_NUM_NULL),
        reqNum(P1_REQ_NUM_NULL),
        sofIdx(P1SOFIDX_INIT_VAL),
        reqType(REQ_TYPE_UNKNOWN),
        reqOutSet(REQ_SET_NONE),
        expRec(EXP_REC_NONE),
        flushSet(FLUSH_NONEED),
        exeState(EXE_STATE_NULL),
        capType(NS3Av3::E_CAPTURE_NORMAL),
        fullRawType(NSCam::NSIoPipe::NSCamIOPipe::EPipe_PURE_RAW),
        frameTimeStamp(0),
        frameTimeStampBoot(0),
        appFrame(nullptr) {
    P1Act act = GET_ACT_PTR(act, *rAct, RET_VOID);
    //
    queId = act->queId;
    magicNum = act->magicNum;
    frmNum = act->frmNum;
    reqNum = act->reqNum;
    sofIdx = act->sofIdx;
    reqType = act->reqType;
    reqOutSet = act->reqOutSet;
    expRec = act->expRec;
    flushSet = act->flushSet;
    exeState = act->exeState;
    capType = act->capType;
    fullRawType = act->fullRawType;
    frameTimeStamp = act->frameTimeStamp;
    frameTimeStampBoot = act->frameTimeStampBoot;
    //
    appFrame = act->appFrame;
  }

  virtual ~P1FrameAct() {}
  //
  MBOOL ready(void) { return (queId != P1ACT_ID_NULL); }
  //
  MINT32 queId;
  MINT32 magicNum;
  MINT32 frmNum;
  MINT32 reqNum;
  MUINT32 sofIdx;
  //
  MUINT32 reqType;   /*REQ_TYPE*/
  MUINT32 reqOutSet; /*REQ_SET(REQ_OUT)*/
  MUINT32 expRec;    /*EXP_REC(EXP_EVT)*/
  MUINT32 flushSet;  /*FLUSH_TYPE*/
  MUINT32 exeState;  /*EXE_STATE*/
  MUINT32 capType;   /*E_CAPTURE_TYPE*/
  MINT32 fullRawType;
  MINT64 frameTimeStamp;
  MINT64 frameTimeStampBoot;
  //
  std::shared_ptr<IPipelineFrame> appFrame;
};

/******************************************************************************
 *
 ******************************************************************************/
class P1TaskCtrl;

class P1TaskCollector {
 public:
  explicit P1TaskCollector(std::shared_ptr<P1TaskCtrl> spTaskCtrl);

  virtual ~P1TaskCollector();

 public:
  MVOID config(void);

  MVOID reset(void);

  MINT queryAct(P1QueAct* rDupAct, MUINT32 index = 0);

  MINT enrollAct(P1QueAct* rNewAct);

  MINT verifyAct(P1QueAct* rSetAct);

  MINT requireAct(P1QueAct* rGetAct);

  MINT requireJob(P1QueJob* rGetJob);

  MINT remainder(void);

  MVOID dumpRoll(void);

 private:
  MINT settle(void);

 private:
  std::shared_ptr<P1TaskCtrl> mspP1TaskCtrl;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MUINT8 mBurstNum;
  mutable std::mutex mCollectorLock;
  std::vector<P1QueAct> mvqActRoll;
};

/******************************************************************************
 *
 ******************************************************************************/
class P1TaskCtrl {
  friend class P1TaskCollector;

 public:
  explicit P1TaskCtrl(std::shared_ptr<P1NodeImp> spP1NodeImp);

  virtual ~P1TaskCtrl();

 public:
  MINT32 generateId() {
    std::lock_guard<std::mutex> _l(mAccIdLock);
    MINT32 ret = mAccId++;
    if (ret == 0 || ret == (MINT32)(-1)) {
      ret = mAccId = P1ACT_ID_FIRST;
    }
    return ret;
  }

 public:
  MVOID config(void);

  MVOID reset(void);

  MBOOL acquireAct(P1QueAct* rGetAct, MINT32 keyId, P1Act actPtr = nullptr);

  MBOOL registerAct(P1QueAct* rGetAct);

  MBOOL releaseAct(P1QueAct* rPutAct);

  MBOOL flushAct(void);

  MVOID sessionLock(void);

  MVOID sessionUnLock(void);

  MVOID dumpActPool(void);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  std::shared_ptr<P1NodeImp> mspP1NodeImp;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MUINT8 mBurstNum;

 protected:
  mutable std::mutex mTaskLock;
  mutable std::mutex mSessionLock;
  std::vector<P1Act> mvpActPool;

 private:
  mutable std::mutex mAccIdLock;
  MINT32 mAccId;
};

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1TASKCTRL_H_
