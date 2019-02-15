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

#define LOG_TAG "MtkCam/P1NodeImp"
//
#include <memory>
#include "mtkcam/pipeline/hwnode/p1/P1TaskCtrl.h"
#include <mtkcam/utils/std/Profile.h>
#include "P1NodeImp.h"
#include <string>
#include <vector>
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
// P1NodeAct
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
P1NodeAct::P1NodeAct(P1NodeImp* pP1NodeImp, MINT32 id)
    : mpP1NodeImp(pP1NodeImp),
      mNodeName(""),
      mNodeId(IPipelineNode::NodeId_T(-1)),
      mOpenId(-1),
      mLogLevel(0),
      mLogLevelI(0),
      queId(id),
      magicNum(P1ACT_NUM_NULL),
      frmNum(P1_FRM_NUM_NULL),
      reqNum(P1_REQ_NUM_NULL),
      sofIdx(P1SOFIDX_INIT_VAL),
      appFrame(nullptr),
      buffer_eiso(nullptr)
      //, buffer_lcso(NULL) //TODO
      ,
      reqType(REQ_TYPE_UNKNOWN),
      reqOutSet(REQ_SET_NONE),
      expRec(EXP_REC_NONE),
      flushSet(FLUSH_NONEED),
      exeState(EXE_STATE_NULL),
      capType(NS3Av3::E_CAPTURE_NORMAL),
      uniSwitchState(UNI_SWITCH_STATE_NONE),
      tgSwitchState(TG_SWITCH_STATE_NONE),
      tgSwitchNum(0),
      qualitySwitchState(QUALITY_SWITCH_STATE_NONE),
      ctrlSensorStatus(SENSOR_STATUS_CTRL_NONE),
      frameExpDuration(0),
      frameTimeStamp(0),
      frameTimeStampBoot(0),
      isMapped(MFALSE),
      isReadoutReady(MFALSE),
      isRawTypeChanged(MFALSE),
      fullRawType(EPipe_PURE_RAW),
      metaSet(),
      msg(""),
      res(""),
      mReqFmt_Imgo(eImgFmt_UNKNOWN),
      mReqFmt_Rrzo(eImgFmt_UNKNOWN) {
  ::memset(portBufIndex, P1_FILL_BYTE, sizeof(portBufIndex));
  if (mpP1NodeImp != nullptr) {
    mNodeName += mpP1NodeImp->getNodeName();
    mNodeId = mpP1NodeImp->getNodeId();
    mOpenId = mpP1NodeImp->getOpenId();
    mLogLevel = mpP1NodeImp->mLogLevel;
    mLogLevelI = mpP1NodeImp->mLogLevelI;
  }
  metaSet.MagicNum = magicNum;
  metaSet.Dummy = MFALSE;
  MY_LOGD("[ActTrace] NEW-ACT:  %d", queId);
}

/******************************************************************************
 *
 ******************************************************************************/
P1NodeAct::~P1NodeAct() {
  MY_LOGD("[ActTrace] DEL-ACT:  %d", queId);
}

/******************************************************************************
 *
 ******************************************************************************/
char const* P1NodeAct::getNodeName() const {
  return mNodeName.c_str();
}

/******************************************************************************
 *
 ******************************************************************************/
IPipelineNode::NodeId_T P1NodeAct::getNodeId() const {
  return mNodeId;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
P1NodeAct::getOpenId() const {
  return mOpenId;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
P1NodeAct::getNum() const {
  return magicNum;
}

/******************************************************************************
 *
 ******************************************************************************/
ACT_TYPE
P1NodeAct::getType() const {
  ACT_TYPE type = ACT_TYPE_NULL;
  switch (reqType) {
    case REQ_TYPE_NORMAL:
      type = ACT_TYPE_NORMAL;
      break;
    case REQ_TYPE_INITIAL:
    case REQ_TYPE_PADDING:
    case REQ_TYPE_DUMMY:
      type = ACT_TYPE_INTERNAL;
      break;
    case REQ_TYPE_REDO:
    case REQ_TYPE_YUV:
    case REQ_TYPE_ZSL:
      type = ACT_TYPE_BYPASS;
      break;
    default:
      // case REQ_TYPE_UNKNOWN:
      type = ACT_TYPE_NULL;
      break;
  }
  return type;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1NodeAct::getFlush(FLUSH_TYPE type) const {
  return IS_FLUSH(type, flushSet);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeAct::setFlush(FLUSH_TYPE type) {
  if (type == FLUSH_NONEED) {  // reset flush type set
    flushSet = FLUSH_NONEED;
  } else {
    flushSet |= type;
  }
  return;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::mapFrameStream() {
  if (CC_UNLIKELY(mpP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp not exist");
    return BAD_VALUE;
  }
  if (CC_UNLIKELY(appFrame == nullptr)) {
    return OK;
  }
  IPipelineFrame::InfoIOMapSet rIOMapSet;
  if (CC_UNLIKELY(OK != appFrame->queryInfoIOMapSet(getNodeId(), &rIOMapSet))) {
    MY_LOGE("queryInfoIOMap failed");
    return BAD_VALUE;
  }
  //
  if (CC_UNLIKELY(isMapped)) {
    MY_LOGD("FrameStream Mapped (%d)", isMapped);
    return OK;
  }
  //
  IPipelineFrame::ImageInfoIOMapSet& imageIOMapSet =
      rIOMapSet.mImageInfoIOMapSet;
  MBOOL bImgSetExist = MTRUE;
  if (CC_UNLIKELY(imageIOMapSet.empty())) {
    bImgSetExist = MFALSE;
    MY_LOGI("no imageIOMap in frame");
  }
  //
  IPipelineFrame::MetaInfoIOMapSet& metaIOMapSet = rIOMapSet.mMetaInfoIOMapSet;
  MBOOL bMetaSetExist = MTRUE;
  if (CC_UNLIKELY(metaIOMapSet.empty())) {
    bMetaSetExist = MFALSE;
    MY_LOGI("no metaIOMap in frame");
  }

  if (CC_UNLIKELY(!bImgSetExist && !bMetaSetExist)) {
    MY_LOGE("both imageIOMap and metaIOMap do not exist!");
    return BAD_VALUE;
  }

  //
#define REG_STREAM_IMG(id, item)                               \
  if ((!streamBufImg[item].bExist) &&                          \
      (mpP1NodeImp->mvStreamImg[item] != nullptr) &&           \
      (mpP1NodeImp->mvStreamImg[item]->getStreamId() == id)) { \
    streamBufImg[item].bExist = MTRUE;                         \
    continue;                                                  \
  }
#define REG_STREAM_META(id, item)                               \
  if ((!streamBufMeta[item].bExist) &&                          \
      (mpP1NodeImp->mvStreamMeta[item] != nullptr) &&           \
      (mpP1NodeImp->mvStreamMeta[item]->getStreamId() == id)) { \
    streamBufMeta[item].bExist = MTRUE;                         \
    continue;                                                   \
  }
  //
  for (size_t i = 0; i < imageIOMapSet.size(); i++) {
    IPipelineFrame::ImageInfoIOMap const& imageIOMap = imageIOMapSet[i];
    if (imageIOMap.vIn.size() > 0) {
      for (auto& j : imageIOMap.vIn) {
        StreamId_T const streamId = j.first;
        REG_STREAM_IMG(streamId, STREAM_IMG_IN_YUV);
        REG_STREAM_IMG(streamId, STREAM_IMG_IN_OPAQUE);
      }
    }
    if (imageIOMap.vOut.size() > 0) {
      for (auto& j : imageIOMap.vOut) {
        StreamId_T const streamId = j.first;
        REG_STREAM_IMG(streamId, STREAM_IMG_OUT_OPAQUE);
        REG_STREAM_IMG(streamId, STREAM_IMG_OUT_FULL);
        REG_STREAM_IMG(streamId, STREAM_IMG_OUT_RESIZE);
        REG_STREAM_IMG(streamId, STREAM_IMG_OUT_LCS);
        REG_STREAM_IMG(streamId, STREAM_IMG_OUT_RSS);
      }
    }
  }
  //
  for (size_t i = 0; i < metaIOMapSet.size(); i++) {
    IPipelineFrame::MetaInfoIOMap const& metaIOMap = metaIOMapSet[i];
    if (metaIOMap.vIn.size() > 0) {
      for (auto& j : metaIOMap.vIn) {
        StreamId_T const streamId = j.first;
        REG_STREAM_META(streamId, STREAM_META_IN_APP);
        REG_STREAM_META(streamId, STREAM_META_IN_HAL);
      }
    }
    if (metaIOMap.vOut.size() > 0) {
      for (auto& j : metaIOMap.vOut) {
        StreamId_T const streamId = j.first;
        REG_STREAM_META(streamId, STREAM_META_OUT_APP);
        REG_STREAM_META(streamId, STREAM_META_OUT_HAL);
      }
    }
  }
  //
#undef REG_STREAM_IMG
#undef REG_STREAM_META
  //
  isMapped = MTRUE;
  //
#if (IS_P1_LOGI)
  if (1 <= mLogLevelI) {
    /*
    Log - Received IOmap information of ths pipeline-frame, ex:
    IOmap-Img[1]=<0_I[0]{}_O[2]{0xaa,0xbb}>-Meta[1]=
    <0_I[2]{0x101,0x102}_O[2]{0x201,0x202}>
          ^      ^^ ^      ^    ^           ^
          |      || |      |    |           |
          |      || |      |    |           the meta-map size is 1
          |      || |      |    the first stream-id is 0xaa
          |      || |      the out-stream amount is 2
          |      || the in-stream amount is 0
          |      |the index 0 of image-map
          |      the <...> means one-map, for maps <...>+<...>+<...>
          the img-map size is 1
    */
    std::string strInfo("IOmap");
    strInfo += base::StringPrintf("-Img[%zu]=", imageIOMapSet.size());
    for (size_t i = 0; i < imageIOMapSet.size(); i++) {
      if (i > 0) {
        strInfo += base::StringPrintf("+");
      }
      IPipelineFrame::ImageInfoIOMap const& imageIOMap = imageIOMapSet[i];
      strInfo += base::StringPrintf("<%zu_I[%zu]{", i, imageIOMap.vIn.size());
      if (imageIOMap.vIn.size() > 0) {
        for (auto& j : imageIOMap.vIn) {
          strInfo += base::StringPrintf(",");
          StreamId_T const streamId = j.first;
          strInfo += base::StringPrintf("%#" PRIx64, streamId);
        }
      }
      strInfo += base::StringPrintf("}_O[%zu]{", imageIOMap.vOut.size());
      if (imageIOMap.vOut.size() > 0) {
        for (auto& j : imageIOMap.vOut) {
          strInfo += base::StringPrintf(",");
          StreamId_T const streamId = j.first;
          strInfo += base::StringPrintf("%#" PRIx64, streamId);
        }
      }
      strInfo += base::StringPrintf("}>");
    }
    //
    strInfo += base::StringPrintf("-Meta[%zu]=", metaIOMapSet.size());
    for (size_t i = 0; i < metaIOMapSet.size(); i++) {
      if (i > 0) {
        strInfo += base::StringPrintf("+");
      }
      IPipelineFrame::MetaInfoIOMap const& metaIOMap = metaIOMapSet[i];
      strInfo += base::StringPrintf("<%zu_I[%zu]{", i, metaIOMap.vIn.size());
      if (metaIOMap.vIn.size() > 0) {
        for (auto& j : metaIOMap.vIn) {
          strInfo += base::StringPrintf(",");
          StreamId_T const streamId = j.first;
          strInfo += base::StringPrintf("%#" PRIx64, streamId);
        }
      }
      strInfo += base::StringPrintf("}_O[%zu]{", metaIOMap.vOut.size());
      if (metaIOMap.vOut.size() > 0) {
        for (auto& j : metaIOMap.vOut) {
          strInfo += base::StringPrintf(",");
          StreamId_T const streamId = j.first;
          strInfo += base::StringPrintf("%#" PRIx64, streamId);
        }
      }
      strInfo += base::StringPrintf("}>");
    }
    strInfo += base::StringPrintf(" ");
    //
    /*
    Log - Accepted Configured-Stream information of ths pipeline-frame, ex:
    CfgStream-Img[OutIMG:0xaa(1),OutRRZ:0xbb(1),OutLCS:0xcc(0)]-
      Meta[InAPP:0x101(1),InHAL:0x102(1),OutAPP:0x201(1),OutHAL:0x202(1)]
              ^   ^      ^    ^                             ^   ^
              |   |      |    |                             |   |
              |   |      |    |                             |   the
    configured-meta-stream |   |      |    |                             the
    configured-stream not exist in this pipeline-frame |   |      |    the
    configured-stream exist in this pipeline-frame |   |      the
    configured-stream-id |   the configured-stream-name the
    configured-img-stream
    */
    strInfo += base::StringPrintf("CfgStream-Img[");
    MBOOL bPrintedImg = MFALSE;
    for (MUINT32 stream = STREAM_ITEM_START; stream < STREAM_IMG_NUM;
         stream++) {
      if (mpP1NodeImp->mvStreamImg[stream] != nullptr) {
        if (bPrintedImg) {
          strInfo += base::StringPrintf(",");
        }
        strInfo += base::StringPrintf(
            "%s:%#" PRIx64 "(%d)", mpP1NodeImp->maStreamImgName[stream],
            mpP1NodeImp->mvStreamImg[stream]->getStreamId(),
            streamBufImg[stream].bExist);
        bPrintedImg = MTRUE;
      }
    }
    //
    strInfo += base::StringPrintf("]-Meta[");
    MBOOL bPrintedMeta = MFALSE;
    for (MUINT32 stream = STREAM_ITEM_START; stream < STREAM_META_NUM;
         stream++) {
      if (mpP1NodeImp->mvStreamMeta[stream] != nullptr) {
        if (bPrintedMeta) {
          strInfo += base::StringPrintf(",");
        }
        strInfo += base::StringPrintf(
            "%s:%#" PRIx64 "(%d)", mpP1NodeImp->maStreamMetaName[stream],
            mpP1NodeImp->mvStreamMeta[stream]->getStreamId(),
            streamBufMeta[stream].bExist);
        bPrintedMeta = MTRUE;
      }
    }
    strInfo += base::StringPrintf("] ");
    //
    msg += strInfo;
  }
#endif
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::frameMetadataInit(
    STREAM_META const streamMeta,
    std::shared_ptr<IMetaStreamBuffer>* pMetaStreamBuffer) {
  P1INFO_ACT_STREAM(Meta);
  //
  P1_CHECK_STREAM_SET(META, streamMeta);
  P1_CHECK_ACT_STREAM(Meta, streamMeta);
  //
  StreamId_T const streamId =
      mpP1NodeImp->mvStreamMeta[streamMeta]->getStreamId();
  IStreamBufferSet& rStreamBufferSet = appFrame->getStreamBufferSet();
  MERROR const err = mpP1NodeImp->ensureMetaBufferAvailable(
      appFrame->getFrameNo(), streamId, &rStreamBufferSet, pMetaStreamBuffer);
  if (CC_UNLIKELY(err != OK)) {
    MY_LOGW("check status(%d) Meta " P1INFO_STREAM_STR P1INFO_ACT_STR, err,
            P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
    return err;
  }
  if (CC_LIKELY(*pMetaStreamBuffer != nullptr)) {
    streamBufMeta[streamMeta].spStreamBuf = *pMetaStreamBuffer;
    streamBufMeta[streamMeta].eLockState = STREAM_BUF_LOCK_NONE;
  } else {
    MY_LOGI("cannot get Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
            P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::frameMetadataGet(STREAM_META const streamMeta,
                            IMetadata* pOutMetadata,
                            MBOOL toWrite,
                            IMetadata* pInMetadata) {
  P1INFO_ACT_STREAM(Meta);
  //
  P1_CHECK_STREAM_SET(META, streamMeta);
  P1_CHECK_ACT_STREAM(Meta, streamMeta);
  //
  std::shared_ptr<IMetaStreamBuffer> pMetaStreamBuffer = nullptr;
  pMetaStreamBuffer = streamBufMeta[streamMeta].spStreamBuf;
  if (CC_UNLIKELY(pMetaStreamBuffer ==
                      nullptr &&  // call frameMetadataInit while NULL
                  OK != frameMetadataInit(streamMeta, &pMetaStreamBuffer))) {
    MY_LOGW("Check Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
            P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  //
  STREAM_BUF_LOCK curLock = streamBufMeta[streamMeta].eLockState;
  // current-lock != needed-lock
  if (((toWrite) && (curLock == STREAM_BUF_LOCK_R)) ||
      ((!toWrite) && (curLock == STREAM_BUF_LOCK_W))) {
    if (CC_UNLIKELY(streamBufMeta[streamMeta].pMetadata == nullptr)) {
      MY_LOGE(
          "previous pMetadata is NULL, Lock(%d) Write:%d, "
          "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
          curLock, toWrite, P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
      return BAD_VALUE;
    }
    pMetaStreamBuffer->unlock(getNodeName(),
                              streamBufMeta[streamMeta].pMetadata);
    //
    streamBufMeta[streamMeta].eLockState = STREAM_BUF_LOCK_NONE;
    streamBufMeta[streamMeta].pMetadata = nullptr;
  }
  // current-lock == STREAM_BUF_LOCK_NONE
  if (streamBufMeta[streamMeta].eLockState == STREAM_BUF_LOCK_NONE) {
    IMetadata* pMetadata = nullptr;
    if (toWrite) {
      pMetadata = pMetaStreamBuffer->tryWriteLock(getNodeName());
    } else {
      pMetadata = pMetaStreamBuffer->tryReadLock(getNodeName());
    }
    if (CC_UNLIKELY(pMetadata == nullptr)) {
      MY_LOGE(
          "get pMetadata is NULL, Lock(%d) Write:%d, "
          "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
          curLock, toWrite, P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
      return BAD_VALUE;
    }
    //
    streamBufMeta[streamMeta].eLockState =
        (toWrite) ? STREAM_BUF_LOCK_W : STREAM_BUF_LOCK_R;
    streamBufMeta[streamMeta].pMetadata = pMetadata;
  }
  //
  if (CC_UNLIKELY(streamBufMeta[streamMeta].pMetadata == nullptr)) {
    MY_LOGE(
        "stored pMetadata is NULL, Lock(%d) Write:%d, "
        "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
        curLock, toWrite, P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  //
  if (toWrite && (pInMetadata != nullptr)) {
    pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
    streamBufMeta[streamMeta].bWrote = MTRUE;
    *(streamBufMeta[streamMeta].pMetadata) = *pInMetadata;
  }
  if (pOutMetadata != nullptr) {
    *pOutMetadata = *(streamBufMeta[streamMeta].pMetadata);
  }
  MY_LOGD(
      "MetaGet(%p)(%p), Lock(%d=>%d) Write:%d, "
      "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
      pOutMetadata, pInMetadata, curLock, streamBufMeta[streamMeta].eLockState,
      toWrite, P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::frameMetadataPut(STREAM_META const streamMeta) {
  P1INFO_ACT_STREAM(Meta);
  //
  P1_CHECK_STREAM_SET(META, streamMeta);
  P1_CHECK_ACT_STREAM(Meta, streamMeta);
  //
  StreamId_T const streamId =
      mpP1NodeImp->mvStreamMeta[streamMeta]->getStreamId();
  if (1 <= mLogLevelI) {
    res += base::StringPrintf(" [Meta%s_%d](%#" PRIx64 ")",
                              mpP1NodeImp->maStreamMetaName[streamMeta],
                              streamMeta, streamId);
  }
  //
  STREAM_BUF_LOCK curLock = streamBufMeta[streamMeta].eLockState;
  //
  if (!IS_IN_STREAM_META(streamMeta)) {
    if (getFlush()) {
      std::shared_ptr<IMetaStreamBuffer> pMetaStreamBuffer = nullptr;
      pMetaStreamBuffer = streamBufMeta[streamMeta].spStreamBuf;
      if (CC_UNLIKELY(
              pMetaStreamBuffer == nullptr &&  // call frameMetaInit while NULL
              OK != frameMetadataInit(streamMeta, &pMetaStreamBuffer))) {
        MY_LOGE(
            "get IMetaStreamBuffer but NULL, Lock(%d=>%d), "
            "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
            curLock, streamBufMeta[streamMeta].eLockState,
            P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
        return BAD_VALUE;
      }
      pMetaStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_ERROR);
      streamBufMeta[streamMeta].bWrote = MTRUE;
    }
  }
  //
  if (1 <= mLogLevelI) {
    std::shared_ptr<IMetaStreamBuffer> pMetaStreamBuffer = nullptr;
    pMetaStreamBuffer = streamBufMeta[streamMeta].spStreamBuf;
    if (pMetaStreamBuffer != nullptr) {
      res += base::StringPrintf("<%s:x%x>",
                                (streamBufMeta[streamMeta].bWrote ? "W" : "N"),
                                pMetaStreamBuffer->getStatus());
    } else {  // the stream not init
      res += base::StringPrintf("<%s:0>",
                                (streamBufMeta[streamMeta].bWrote ? "W" : "N"));
    }
  }
  //
  if (curLock != STREAM_BUF_LOCK_NONE) {
    if (streamBufMeta[streamMeta].spStreamBuf != nullptr) {
      if (streamBufMeta[streamMeta].pMetadata != nullptr) {
        streamBufMeta[streamMeta].spStreamBuf->unlock(
            getNodeName(), streamBufMeta[streamMeta].pMetadata);
        streamBufMeta[streamMeta].eLockState = STREAM_BUF_LOCK_NONE;
      } else {
        MY_LOGW(
            "MetaStream locked but no Metadata, Lock(%d=>%d), "
            "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
            curLock, streamBufMeta[streamMeta].eLockState,
            P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
      }
    } else {
      MY_LOGW(
          "MetaStream locked but no StreamBuf, Lock(%d=>%d), "
          "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
          curLock, streamBufMeta[streamMeta].eLockState,
          P1INFO_STREAM_VAR(Meta), P1INFO_ACT_VAR(*this));
    }
  }
  //
  IStreamBufferSet& rStreamBufferSet = appFrame->getStreamBufferSet();
  MUINT32 uStatus =
      (IUsersManager::UserStatus::RELEASE | IUsersManager::UserStatus::USED);
  rStreamBufferSet.markUserStatus(streamId, getNodeId(),
#if 0  // TODO(MTK): if it is not used, only mark RELEASE
        (curLock == STREAM_BUF_LOCK_NONE &&
         exeState == EXE_STATE_REQUESTED) ?
        (IUsersManager::UserStatus::RELEASE) :
#endif
                                  uStatus);
  if (1 <= mLogLevelI) {
    res += base::StringPrintf("<U:x%x>", uStatus);
  }
  //
  MY_LOGD(
      "MetaPut, Lock(%d=>%d), "
      "Meta " P1INFO_STREAM_STR P1INFO_ACT_STR,
      curLock, streamBufMeta[streamMeta].eLockState, P1INFO_STREAM_VAR(Meta),
      P1INFO_ACT_VAR(*this));
  streamBufMeta[streamMeta].pMetadata = nullptr;
  streamBufMeta[streamMeta].spStreamBuf = nullptr;
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::frameImageInit(
    STREAM_IMG const streamImg,
    std::shared_ptr<IImageStreamBuffer>* pImageStreamBuffer) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_ACT_STREAM(Img, streamImg);
  //
  if (CC_UNLIKELY(mpP1NodeImp == nullptr)) {
    return BAD_VALUE;
  }
  //
  StreamId_T const streamId =
      mpP1NodeImp->mvStreamImg[streamImg]->getStreamId();
  IStreamBufferSet& rStreamBufferSet = appFrame->getStreamBufferSet();
  mpP1NodeImp->mLogInfo.setMemo(LogInfo::CP_BUF_BGN, streamImg, streamId,
                                frmNum, reqNum);
  MERROR const err = mpP1NodeImp->ensureImageBufferAvailable(
      appFrame->getFrameNo(), streamId, &rStreamBufferSet, pImageStreamBuffer);
  mpP1NodeImp->mLogInfo.setMemo(LogInfo::CP_BUF_END, streamImg, streamId,
                                frmNum, reqNum);
  if (CC_UNLIKELY(err != OK)) {
    MY_LOGI("check status(%d) Image " P1INFO_STREAM_STR P1INFO_ACT_STR, err,
            P1INFO_STREAM_VAR(Img), P1INFO_ACT_VAR(*this));
    return err;
  }
  if (CC_LIKELY(*pImageStreamBuffer != nullptr)) {
    streamBufImg[streamImg].spStreamBuf = *pImageStreamBuffer;
    streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
  } else {
    MY_LOGI("cannot get Image " P1INFO_STREAM_STR P1INFO_ACT_STR,
            P1INFO_STREAM_VAR(Img), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::frameImageGet(STREAM_IMG const streamImg,
                         std::shared_ptr<IImageBuffer>* rImgBuf) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_ACT_STREAM(Img, streamImg);
  //
  std::shared_ptr<IImageStreamBuffer> pImageStreamBuffer = nullptr;
  pImageStreamBuffer = streamBufImg[streamImg].spStreamBuf;
  if (CC_UNLIKELY(pImageStreamBuffer ==
                      nullptr &&  // call frameImageInit while NULL
                  OK != frameImageInit(streamImg, &pImageStreamBuffer))) {
    MY_LOGI("Check Image " P1INFO_STREAM_STR " in Frame " P1INFO_ACT_STR,
            P1INFO_STREAM_VAR(Img), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  //
  STREAM_BUF_LOCK curLock = streamBufImg[streamImg].eLockState;
  // for stream image, only implement Write Lock
  MUINT groupUsage = 0x0;
  if (curLock == STREAM_BUF_LOCK_NONE) {
    groupUsage = pImageStreamBuffer->queryGroupUsage(getNodeId());
    if (mpP1NodeImp->mDebugScanLineMask != 0) {
      groupUsage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    }
    std::shared_ptr<IImageBufferHeap> pImageBufferHeap = nullptr;
    pImageBufferHeap.reset(
        pImageStreamBuffer->tryWriteLock(getNodeName()),
        [](IImageBufferHeap* p) { MY_LOGI("release implement"); });
    if (CC_UNLIKELY(pImageBufferHeap == nullptr)) {
      MY_LOGE("ImageBufferHeap == NULL " P1INFO_STREAM_IMG_STR
              " " P1INFO_ACT_STR,
              P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
      return BAD_VALUE;
    }
#if 1  // for opaque out image stream add info
    if (streamImg == STREAM_IMG_OUT_OPAQUE) {
      pImageBufferHeap->lockBuf(getNodeName());
      if (OK != OpaqueReprocUtil::setOpaqueInfoToHeap(
                    pImageBufferHeap, mpP1NodeImp->mSensorParams.size,
                    mpP1NodeImp->mRawFormat, mpP1NodeImp->mRawStride,
                    mpP1NodeImp->mRawLength)) {
        MY_LOGW("OUT_OPAQUE setOpaqueInfoToHeap fail " P1INFO_STREAM_IMG_STR
                " " P1INFO_ACT_STR,
                P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
      }
      pImageBufferHeap->unlockBuf(getNodeName());
    }
#endif
    // get ImageBuffer from ImageBufferHeap
    std::shared_ptr<IImageBuffer> pImageBuffer = nullptr;
    if (streamImg == STREAM_IMG_OUT_OPAQUE ||
        streamImg == STREAM_IMG_IN_OPAQUE) {
      pImageBufferHeap->lockBuf(getNodeName());
      MERROR status = OpaqueReprocUtil::getImageBufferFromHeap(pImageBufferHeap,
                                                               &pImageBuffer);
      pImageBufferHeap->unlockBuf(getNodeName());
      if (CC_UNLIKELY(status != OK)) {
        MY_LOGE(
            "Cannot get ImageBuffer from opaque "
            "ImageBufferHeap " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
        return BAD_VALUE;
      }
    } else {
      MINT reqImgFormat = mpP1NodeImp->mvStreamImg[streamImg]->getImgFormat();
      if ((streamImg == STREAM_IMG_OUT_FULL) &&
          (mReqFmt_Imgo != eImgFmt_UNKNOWN)) {
        reqImgFormat = mReqFmt_Imgo;
      }
      if ((streamImg == STREAM_IMG_OUT_RESIZE) &&
          (mReqFmt_Rrzo != eImgFmt_UNKNOWN)) {
        reqImgFormat = mReqFmt_Rrzo;
      }

      ImgBufCreator creator(reqImgFormat);
      pImageBuffer = pImageBufferHeap->createImageBuffer(&creator);
    }
    if (CC_UNLIKELY(pImageBuffer == nullptr)) {
      MY_LOGE("ImageBuffer == NULL " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
              P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
      return BAD_VALUE;
    } else {
      groupUsage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
      groupUsage |= GRALLOC_USAGE_SW_READ_OFTEN;
      pImageBuffer->lockBuf(getNodeName(), groupUsage);
      streamBufImg[streamImg].spImgBuf = pImageBuffer;
      streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_W;
      streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_FRAME;
    }
  }
  //
  if (streamBufImg[streamImg].eLockState == STREAM_BUF_LOCK_W) {
    if (CC_UNLIKELY(streamBufImg[streamImg].spImgBuf == nullptr)) {
      MY_LOGE("stored ImageBuffer is nullptr " P1INFO_STREAM_IMG_STR
              " " P1INFO_ACT_STR,
              P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
      return BAD_VALUE;
    }
    *rImgBuf = streamBufImg[streamImg].spImgBuf;
  }
  MY_LOGD("ImgGet-frame, " P1INFO_STREAM_IMG_STR
          " Lock(%d=>%d) Usage(0x%x) " P1INFO_ACT_STR,
          P1INFO_STREAM_IMG_VAR(*this), curLock,
          streamBufImg[streamImg].eLockState, groupUsage,
          P1INFO_ACT_VAR(*this));
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::frameImagePut(STREAM_IMG const streamImg) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_ACT_STREAM(Img, streamImg);
  //
  StreamId_T const streamId =
      mpP1NodeImp->mvStreamImg[streamImg]->getStreamId();
  if (1 <= mLogLevelI) {
    res += base::StringPrintf(" [Img%s_%d](%#" PRIx64 ")",
                              mpP1NodeImp->maStreamImgName[streamImg],
                              streamImg, streamId);
  }
  //
  STREAM_BUF_LOCK curLock = streamBufImg[streamImg].eLockState;
  //
  if (!IS_IN_STREAM_IMG(streamImg)) {
    if (exeState == EXE_STATE_REQUESTED || exeState == EXE_STATE_PROCESSING ||
        exeState == EXE_STATE_DONE) {
      std::shared_ptr<IImageStreamBuffer> pImageStreamBuffer = nullptr;
      pImageStreamBuffer = streamBufImg[streamImg].spStreamBuf;
      if (CC_UNLIKELY(pImageStreamBuffer ==
                          nullptr &&  // call frameImgInit while NULL
                      OK != frameImageInit(streamImg, &pImageStreamBuffer))) {
        MY_LOGE("get ImageStreamBuffer but NULL, " P1INFO_STREAM_IMG_STR
                " " P1INFO_ACT_STR,
                P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
        return BAD_VALUE;
      }
      pImageStreamBuffer->markStatus((getFlush())
                                         ? STREAM_BUFFER_STATUS::WRITE_ERROR
                                         : STREAM_BUFFER_STATUS::WRITE_OK);
      streamBufImg[streamImg].bWrote = MTRUE;
    }
  }
  //
  if (1 <= mLogLevelI) {
    std::shared_ptr<IImageStreamBuffer> pImageStreamBuffer = nullptr;
    pImageStreamBuffer = streamBufImg[streamImg].spStreamBuf;
    if (pImageStreamBuffer != nullptr) {
      res += base::StringPrintf("<%s:x%x>",
                                (streamBufImg[streamImg].bWrote ? "W" : "N"),
                                pImageStreamBuffer->getStatus());
    } else {  // the stream not init
      res += base::StringPrintf("<%s:0>",
                                (streamBufImg[streamImg].bWrote ? "W" : "N"));
    }
  }
  //
  if (curLock != STREAM_BUF_LOCK_NONE) {
    if (streamBufImg[streamImg].spStreamBuf != nullptr) {
      if (streamBufImg[streamImg].spImgBuf != nullptr) {
        streamBufImg[streamImg].spImgBuf->unlockBuf(getNodeName());
        streamBufImg[streamImg].spStreamBuf->unlock(
            getNodeName(),
            streamBufImg[streamImg].spImgBuf->getImageBufferHeap().get());
        streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
      } else {
        MY_LOGW(
            "ImageStream locked but no ImageBuffer, "
            "Lock(%d=>%d), " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
            curLock, streamBufImg[streamImg].eLockState,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
      }
    } else {
      MY_LOGW(
          "ImageStream locked but no StreamBuf, "
          "Lock(%d=>%d), " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
          curLock, streamBufImg[streamImg].eLockState,
          P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    }
  }
  MY_LOGD("ImgPut-frame, " P1INFO_STREAM_IMG_STR
          " Lock(%d=>%d) " P1INFO_ACT_STR,
          P1INFO_STREAM_IMG_VAR(*this), curLock,
          streamBufImg[streamImg].eLockState, P1INFO_ACT_VAR(*this));
  //
  IStreamBufferSet& rStreamBufferSet = appFrame->getStreamBufferSet();
  MUINT32 uStatus =
      (IUsersManager::UserStatus::RELEASE | IUsersManager::UserStatus::USED);
  rStreamBufferSet.markUserStatus(streamId, getNodeId(),
#if 0  // TODO(MTK): if it is not used, only mark RELEASE
        (curLock == STREAM_BUF_LOCK_NONE &&
         exeState == EXE_STATE_REQUESTED) ?
        (IUsersManager::UserStatus::RELEASE) :
#endif
                                  uStatus);
  if (1 <= mLogLevelI) {
    res += base::StringPrintf("<U:x%x>", uStatus);
  }
  //
  streamBufImg[streamImg].spImgBuf = nullptr;
  streamBufImg[streamImg].spStreamBuf = nullptr;
  streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_NULL;
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::poolImageGet(STREAM_IMG const streamImg,
                        std::shared_ptr<IImageBuffer>* rImgBuf) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_CFG_STREAM(Img, (*this), streamImg);
  //
  MERROR err = OK;
  std::shared_ptr<P1Node::IImageStreamBufferPoolT> pStreamBufPool = nullptr;
  switch (streamImg) {
    case STREAM_IMG_OUT_FULL:
    case STREAM_IMG_OUT_OPAQUE:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_full;
      break;
    case STREAM_IMG_OUT_RESIZE:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_resizer;
      break;
    case STREAM_IMG_OUT_LCS:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_lcso;
      break;
    case STREAM_IMG_OUT_RSS:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_rsso;
      break;
    default:
      MY_LOGE("INVALID POOL %d", streamImg);
      return INVALID_OPERATION;
  }
  if (CC_UNLIKELY(pStreamBufPool == nullptr)) {
    MY_LOGE("StreamBufPool is NULL " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  err = pStreamBufPool->acquireFromPool(getNodeName(),
                                        &(streamBufImg[streamImg].spStreamBuf),
                                        NSCam::Utils::s2ns(300));
  if (CC_UNLIKELY(err != OK)) {
    if (err == TIMED_OUT) {
      MY_LOGW("acquire timeout " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
              P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    } else {
      MY_LOGW("acquire failed " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
              P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    }
    pStreamBufPool->dumpPool();
    return BAD_VALUE;
  }
  //
  if (CC_UNLIKELY(streamBufImg[streamImg].spStreamBuf == nullptr)) {
    MY_LOGE("ImageStreamBuffer is NULL " P1INFO_STREAM_IMG_STR
            " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  //
  MUINT usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_CAMERA_READ |
                GRALLOC_USAGE_HW_CAMERA_WRITE | GRALLOC_USAGE_SW_WRITE_OFTEN;
  if (mpP1NodeImp->mDebugScanLineMask != 0) {
    usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  }
  std::shared_ptr<IImageBufferHeap> pImageBufferHeap = nullptr;
  pImageBufferHeap.reset(
      streamBufImg[streamImg].spStreamBuf->tryWriteLock(getNodeName()),
      [](IImageBufferHeap* p) { MY_LOGI("release implement"); });
  if (CC_UNLIKELY(pImageBufferHeap == nullptr)) {
    MY_LOGE("pImageBufferHeap == NULL " P1INFO_STREAM_IMG_STR
            " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  MINT reqImgFormat = mpP1NodeImp->mvStreamImg[streamImg]->getImgFormat();
  if ((streamImg == STREAM_IMG_OUT_FULL) && (mReqFmt_Imgo != eImgFmt_UNKNOWN)) {
    reqImgFormat = mReqFmt_Imgo;
  }
  if ((streamImg == STREAM_IMG_OUT_RESIZE) &&
      (mReqFmt_Rrzo != eImgFmt_UNKNOWN)) {
    reqImgFormat = mReqFmt_Rrzo;
  }

  ImgBufCreator creator(reqImgFormat);
  *rImgBuf = pImageBufferHeap->createImageBuffer(&creator);
  if (CC_UNLIKELY(rImgBuf == nullptr)) {
    MY_LOGE("pImageBuffer == NULL " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  (*rImgBuf)->lockBuf(getNodeName(), usage);
  streamBufImg[streamImg].spImgBuf = *rImgBuf;
  streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_W;
  streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_POOL;
  MY_LOGD("ImgGet-pool, " P1INFO_STREAM_IMG_STR " Usage(0x%x) " P1INFO_ACT_STR,
          P1INFO_STREAM_IMG_VAR(*this), usage, P1INFO_ACT_VAR(*this));
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::poolImagePut(STREAM_IMG const streamImg) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_CFG_STREAM(Img, (*this), streamImg);
  //
  std::shared_ptr<P1Node::IImageStreamBufferPoolT> pStreamBufPool = nullptr;
  switch (streamImg) {
    case STREAM_IMG_OUT_FULL:
    case STREAM_IMG_OUT_OPAQUE:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_full;
      break;
    case STREAM_IMG_OUT_RESIZE:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_resizer;
      break;
    case STREAM_IMG_OUT_LCS:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_lcso;
      break;
    case STREAM_IMG_OUT_RSS:
      pStreamBufPool = mpP1NodeImp->mpStreamPool_rsso;
      break;
    default:
      MY_LOGE("INVALID POOL %d", streamImg);
      return INVALID_OPERATION;
  }
  if (CC_UNLIKELY(pStreamBufPool == nullptr)) {
    MY_LOGE("StreamBufPool is NULL " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  MY_LOGD("ImgPut-pool, " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
          P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
  if (streamBufImg[streamImg].eLockState != STREAM_BUF_LOCK_NONE) {
    streamBufImg[streamImg].spImgBuf->unlockBuf(getNodeName());
    streamBufImg[streamImg].spStreamBuf->unlock(
        getNodeName(),
        streamBufImg[streamImg].spImgBuf->getImageBufferHeap().get());
  }
  //
  pStreamBufPool->releaseToPool(getNodeName(),
                                streamBufImg[streamImg].spStreamBuf);
  //
  streamBufImg[streamImg].spImgBuf = nullptr;
  streamBufImg[streamImg].spStreamBuf = nullptr;
  streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
  streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_NULL;
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::stuffImageGet(STREAM_IMG const streamImg,
                         MSize const dstSize,
                         std::shared_ptr<IImageBuffer>* rImgBuf) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_CFG_STREAM(Img, (*this), streamImg);
  //
  MERROR err = OK;
  if (streamImg == STREAM_IMG_OUT_OPAQUE) {
    char const* szName = "Hal:Image:P1:OPAQUESTUFFraw";
    std::vector<MUINT32> stride;
    stride.clear();
    stride.reserve(P1NODE_IMG_BUF_PLANE_CNT_MAX);
    stride.push_back(mpP1NodeImp->mRawStride);  // OpaqueRaw : 1-plane
    err = mpP1NodeImp->createStuffBuffer(
        rImgBuf, szName, mpP1NodeImp->mRawFormat,
        MSize(mpP1NodeImp->mSensorParams.size.w, dstSize.h), stride);
  } else {
    if (CC_UNLIKELY(mpP1NodeImp->mvStreamImg[streamImg] == nullptr)) {
      MY_LOGE("create stuff buffer without stream info " P1INFO_STREAM_IMG_STR
              " " P1INFO_ACT_STR,
              P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
      return BAD_VALUE;
    }
    MINT streamImgFormat = mpP1NodeImp->mvStreamImg[streamImg]->getImgFormat();
    MINT reqImgFormat = streamImgFormat;
    if ((streamImg == STREAM_IMG_OUT_FULL) &&
        (mReqFmt_Imgo != eImgFmt_UNKNOWN)) {
      reqImgFormat = mReqFmt_Imgo;
    }
    if ((streamImg == STREAM_IMG_OUT_RESIZE) &&
        (mReqFmt_Rrzo != eImgFmt_UNKNOWN)) {
      reqImgFormat = mReqFmt_Rrzo;
    }
    MSize bufSize =
        MSize(mpP1NodeImp->mvStreamImg[streamImg]->getImgSize().w, dstSize.h);
    std::vector<MUINT32> bufStride;
    bufStride.clear();
    bufStride.reserve(P1NODE_IMG_BUF_PLANE_CNT_MAX);
    if (reqImgFormat == streamImgFormat) {  // get stride from stream info.
      IImageStreamInfo::BufPlanes_t const& bufPlanes =
          mpP1NodeImp->mvStreamImg[streamImg]->getBufPlanes();
      size_t bufPlaneNum =
          ::NSCam::Utils::Format::queryPlaneCount(reqImgFormat);
      for (size_t i = 0; i < bufPlaneNum; i++) {
        bufStride.push_back(bufPlanes[i].rowStrideInBytes);
      }
    } else {  // since reqImgFormat != streamImgFormat, streamImg must be
              // STREAM_IMG_OUT_FULL or STREAM_IMG_OUT_RESIZE
      err = mpP1NodeImp->mStuffBufMgr.collectBufferInfo(
          mpP1NodeImp->mSensorParams.pixelMode,
          (streamImg == STREAM_IMG_OUT_FULL), reqImgFormat, bufSize,
          &bufStride);
    }
    if (CC_LIKELY(err == OK)) {
      err = mpP1NodeImp->createStuffBuffer(
          rImgBuf, mpP1NodeImp->mvStreamImg[streamImg]->getStreamName(),
          reqImgFormat, bufSize, bufStride);
    }
  }
  if (CC_UNLIKELY(err != OK)) {
    MY_LOGE("create stuff buffer with stream info failed " P1INFO_STREAM_IMG_STR
            " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  //
  if (*rImgBuf != nullptr) {
    streamBufImg[streamImg].spImgBuf = *rImgBuf;
    streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_W;
    streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_STUFF;
  }
  MY_LOGD("ImgGet-stuff, " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
          P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
P1NodeAct::stuffImagePut(STREAM_IMG const streamImg) {
  P1INFO_ACT_STREAM(Img);
  //
  P1_CHECK_STREAM_SET(IMG, streamImg);
  P1_CHECK_CFG_STREAM(Img, (*this), streamImg);
  //
  if (CC_UNLIKELY(streamBufImg[streamImg].spImgBuf == nullptr)) {
    MY_LOGE("destroy stuff buffer without ImageBuffer " P1INFO_STREAM_IMG_STR
            " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  if (CC_UNLIKELY(streamBufImg[streamImg].eLockState == STREAM_BUF_LOCK_NONE)) {
    MY_LOGI("destroy stuff buffer skip " P1INFO_STREAM_IMG_STR
            " " P1INFO_ACT_STR,
            P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
    return BAD_VALUE;
  }
  MY_LOGD("ImgPut-stuff, " P1INFO_STREAM_IMG_STR " " P1INFO_ACT_STR,
          P1INFO_STREAM_IMG_VAR(*this), P1INFO_ACT_VAR(*this));
  //
  mpP1NodeImp->destroyStuffBuffer(&(streamBufImg[streamImg].spImgBuf));
  //
  streamBufImg[streamImg].eLockState = STREAM_BUF_LOCK_NONE;
  streamBufImg[streamImg].spImgBuf = nullptr;
  streamBufImg[streamImg].spStreamBuf = nullptr;
  streamBufImg[streamImg].eSrcType = IMG_BUF_SRC_NULL;
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1NodeAct::updateMetaSet() {
  ACT_TYPE type = getType();
  if (CC_UNLIKELY(type != ACT_TYPE_NORMAL && type != ACT_TYPE_INTERNAL)) {
    MY_LOGW("not-support-type (%d)", type);
    return;
  }
  MINT32 num = magicNum;
  MUINT8 dummy = (type == ACT_TYPE_NORMAL) ? (0) : (1);
  //
  metaSet.MagicNum = num;
  IMetadata::IEntry entry_num(MTK_P1NODE_PROCESSOR_MAGICNUM);
  entry_num.push_back(num, Type2Type<MINT32>());
  metaSet.halMeta.update(MTK_P1NODE_PROCESSOR_MAGICNUM, entry_num);
  //
  metaSet.Dummy = dummy;
  IMetadata::IEntry entry_dummy(MTK_HAL_REQUEST_DUMMY);
  entry_dummy.push_back(dummy, Type2Type<MUINT8>());
  metaSet.halMeta.update(MTK_HAL_REQUEST_DUMMY, entry_dummy);
  //
  return;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// P1TaskCollector
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
P1TaskCollector::P1TaskCollector(std::shared_ptr<P1TaskCtrl> spP1TaskCtrl)
    : mspP1TaskCtrl(spP1TaskCtrl),
      mOpenId(-1),
      mLogLevel(0),
      mLogLevelI(0),
      mBurstNum(1) {
  config();
}

/******************************************************************************
 *
 ******************************************************************************/
P1TaskCollector::~P1TaskCollector() {
  reset();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCollector::config() {
  if (mspP1TaskCtrl != nullptr) {
    mOpenId = mspP1TaskCtrl->mOpenId;
    mLogLevel = mspP1TaskCtrl->mLogLevel;
    mLogLevelI = mspP1TaskCtrl->mLogLevelI;
    if (mspP1TaskCtrl->mBurstNum > 1) {
      mBurstNum = mspP1TaskCtrl->mBurstNum;
    }
  }
  reset();
  {
    std::lock_guard<std::mutex> _l(mCollectorLock);
    mvqActRoll.reserve(mBurstNum * P1NODE_DEF_QUEUE_DEPTH);
    mvqActRoll.clear();
    MY_LOGI("ActRoll.Capacity[%d]", (MUINT32)mvqActRoll.capacity());
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCollector::reset() {
  settle();
  std::lock_guard<std::mutex> _l(mCollectorLock);
  mvqActRoll.clear();
}

/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::remainder() {
  std::lock_guard<std::mutex> _l(mCollectorLock);
  return ((MINT)mvqActRoll.size());
}

/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::queryAct(P1QueAct* rDupAct, MUINT32 index) {
  if (CC_UNLIKELY((rDupAct->mKeyId != P1ACT_ID_NULL) ||
                  (rDupAct->mpAct != NULL))) {
    MY_LOGI("Act is already existing (%d)[%d]", rDupAct->mKeyId,
            ((rDupAct->mpAct == nullptr) ? 0 : 1));
    return (-1);
  }
  //
  if (CC_UNLIKELY(mspP1TaskCtrl == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp / P1TaskCtrl NULL");
    return (-1);
  }
  //
  std::lock_guard<std::mutex> _l(mCollectorLock);
  if (mvqActRoll.size() > (size_t)index) {
    std::vector<P1QueAct>::iterator it = mvqActRoll.begin();
    it += index;
    if (it < mvqActRoll.end()) {
      *rDupAct = (*it);
    }
  }
  //
  return ((MINT)mvqActRoll.size());
}

/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::enrollAct(P1QueAct* rNewAct) {
  if (CC_UNLIKELY((rNewAct->mKeyId != P1ACT_ID_NULL) ||
                  (rNewAct->mpAct != nullptr))) {
    MY_LOGI("Act is already existing (%d)[%d]", rNewAct->mKeyId,
            ((rNewAct->mpAct == nullptr) ? 0 : 1));
    return (-1);
  }
  //
  if (CC_UNLIKELY(mspP1TaskCtrl == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp / P1TaskCtrl NULL");
    return (-1);
  }
  //
  MINT32 id = mspP1TaskCtrl->generateId();
  P1Act pAct =
      std::make_shared<P1NodeAct>(mspP1TaskCtrl->mspP1NodeImp.get(), id);
  if (CC_UNLIKELY(pAct == nullptr)) {
    MY_LOGE("[ActTrace] CANNOT Allocate Act:  %d", id);
    return (-1);
  }
  //
  rNewAct->set(pAct, id);
  //
  std::lock_guard<std::mutex> _l(mCollectorLock);
  mvqActRoll.push_back(*rNewAct);
  //
  return ((MINT)mvqActRoll.size());
}
/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::verifyAct(P1QueAct* rSetAct) {
  if (CC_UNLIKELY((rSetAct->mKeyId == P1ACT_ID_NULL) ||
                  (rSetAct->mpAct == NULL))) {
    MY_LOGI("Act is not ready");
    return (-1);
  }
  //
  if (CC_UNLIKELY(mspP1TaskCtrl == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp->mpDeliverMgr == nullptr)) {
    MY_LOGE("P1NodeImp / TaskCtrl / DeliverMgr NULL");
    return (-1);
  }
  //
  std::lock_guard<std::mutex> _l(mCollectorLock);
  /*
      this function called after enrollAct() and createAction()
      if act type is normal, add act id to DeliverMgrList
      if act type is internal, NOT add to DeliverMgrList
      if act type is bypass, NOT add act id to DeliverMgrList, remove act from
     mvqActRoll and call onReturnFrame() directly
  */
  ACT_TYPE type = rSetAct->getType();
  if (type == ACT_TYPE_NORMAL) {
    if (CC_UNLIKELY(rSetAct->mpAct->appFrame == nullptr)) {
      MY_LOGE("IPipelineFrame is NULL");
      return (-1);
    }
    mspP1TaskCtrl->mspP1NodeImp->mpDeliverMgr->registerActList(
        rSetAct->mpAct->appFrame->getFrameNo());
  } else if (type == ACT_TYPE_INTERNAL) {
    // will not need the DeliverMgr dispatching
  } else if (type == ACT_TYPE_BYPASS) {
    // since the BYPASS request do not need the delivery-in-order, it do not
    // need to add to DeliverMgrList (registerActList)
    if (!mvqActRoll.empty()) {
      std::vector<P1QueAct>::iterator it = mvqActRoll.begin();
      for (; it != mvqActRoll.end(); it++) {
        if (rSetAct->ptr() == (*it).ptr()) {
          mvqActRoll.erase(it);
          mspP1TaskCtrl->registerAct(rSetAct);
          MY_LOGD("send the ZSL request and try to trigger");
          mspP1TaskCtrl->mspP1NodeImp->onReturnFrame(rSetAct, FLUSH_NONEED,
                                                     MTRUE);
          /* DO NOT use this P1QueAct after onReturnFrame() */
          break;
        }
      }
    }
  } else {  // ACT_TYPE_NULL
    MY_LOGW("P1_ACT_TYPE_NULL");
  }
  //
  return ((MINT)mvqActRoll.size());
}

/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::requireAct(P1QueAct* rGetAct) {
  if (CC_UNLIKELY((rGetAct->mKeyId != P1ACT_ID_NULL) ||
                  (rGetAct->mpAct != NULL))) {
    MY_LOGI("Act is already existing (%d)[%d]", rGetAct->mKeyId,
            ((rGetAct->mpAct == nullptr) ? 0 : 1));
    return (-1);
  }
  //
  if (CC_UNLIKELY(mspP1TaskCtrl == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp / P1TaskCtrl NULL");
    return (-1);
  }
  //
  std::lock_guard<std::mutex> _l(mCollectorLock);
  if (!mvqActRoll.empty()) {
    std::vector<P1QueAct>::iterator it = mvqActRoll.begin();
    // P1QueAct qAct = (*it);
    if (MFALSE == mspP1TaskCtrl->registerAct(&(*it))) {
      MY_LOGE("register Act fail");
      return (-1);
    }
    *rGetAct = (*it);
    mvqActRoll.erase(it);
  }
  //
  return ((MINT)mvqActRoll.size());
}

/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::requireJob(P1QueJob* rGetJob) {
  if (CC_UNLIKELY(!rGetJob->empty())) {
    MY_LOGI("Job is already existing (%d)[%zu]", rGetJob->getIdx(),
            rGetJob->size());
    return (-1);
  }
  //
  if (CC_UNLIKELY(mspP1TaskCtrl == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp / P1TaskCtrl NULL");
    return (-1);
  }
  //
  std::lock_guard<std::mutex> _l(mCollectorLock);
  MUINT32 cnt = 0;
  if (CC_LIKELY(!mvqActRoll.empty() &&
                mvqActRoll.size() >= rGetJob->getMax())) {
    std::vector<P1QueAct>::iterator it = mvqActRoll.begin();
    // for(; it != mvqActRoll.end(); /*it++*/) {
    while (it != mvqActRoll.end()) {
      if (MFALSE == mspP1TaskCtrl->registerAct(&(*it))) {
        MY_LOGI("RegAct(%d) ret(-1) - [%zu]>=(%d)", it->id(), mvqActRoll.size(),
                rGetJob->getMax());
        return (-1);
      }
      rGetJob->push(*it);
      cnt++;
      if (cnt == 1) {  // set FirstMagicNum as Job ID
        rGetJob->setIdx((*it).getNum());
      }
      it = mvqActRoll.erase(mvqActRoll.begin());  // it++
      //
      if (cnt >= rGetJob->getMax()) {  // Job push complete
        break;
      }
    }
  } else {
    MY_LOGI("Roll[%zu] < (%d)", mvqActRoll.size(), rGetJob->getMax());
  }
  //
  return ((MINT)mvqActRoll.size());
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCollector::dumpRoll() {
  std::vector<P1QueAct>::iterator it = mvqActRoll.begin();
  MUINT idx = 0;
  for (; it != mvqActRoll.end(); it++, idx++) {
    P1Act pAct = it->ptr();
    if (CC_UNLIKELY(pAct == nullptr)) {
      MY_LOGI("cannot get act");
      return;
    }
    MY_LOGI(
        "[P1QueActCheck] ROLL[%d/%zu] : Act[id:%d num:%d "
        "type:%d] " P1INFO_ACT_STR,
        idx, mvqActRoll.size(), it->id(), it->getNum(), it->getType(),
        P1INFO_ACT_VAR(*pAct));
  }
  return;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT P1TaskCollector::settle() {
  if (CC_UNLIKELY(mspP1TaskCtrl == nullptr ||
                  mspP1TaskCtrl->mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp / P1TaskCtrl NULL");
    return (-1);
  }
  //
  std::lock_guard<std::mutex> _l(mCollectorLock);
  //
  if (CC_UNLIKELY(!mvqActRoll.empty())) {
    MY_LOGI("[P1ActCheck] settle remainder [%zu]", mvqActRoll.size());
  }
  //
  while (!mvqActRoll.empty()) {
    std::vector<P1QueAct>::iterator it = mvqActRoll.begin();
    mspP1TaskCtrl->registerAct(&(*it));
    mvqActRoll.erase(it);
  }
  //
  if (CC_UNLIKELY(!mvqActRoll.empty())) {
    MY_LOGE("[P1ActCheck] settle not clean [%zu]", mvqActRoll.size());
  }
  //
  return ((MINT)mvqActRoll.size());
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// P1TaskCtrl
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
P1TaskCtrl::P1TaskCtrl(std::shared_ptr<P1NodeImp> spP1NodeImp)
    : mspP1NodeImp(spP1NodeImp),
      mOpenId(-1),
      mLogLevel(0),
      mLogLevelI(0),
      mBurstNum(1),
      mAccId(P1ACT_ID_FIRST) {
  config();
}

/******************************************************************************
 *
 ******************************************************************************/
P1TaskCtrl::~P1TaskCtrl() {
  reset();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCtrl::config() {
  if (mspP1NodeImp != nullptr) {
    mOpenId = mspP1NodeImp->getOpenId();
    mLogLevel = mspP1NodeImp->mLogLevel;
    mLogLevelI = mspP1NodeImp->mLogLevelI;
    if (mspP1NodeImp->mBurstNum > 1) {
      mBurstNum = mspP1NodeImp->mBurstNum;
    }
  }
  reset();
  {
    std::lock_guard<std::mutex> _l(mTaskLock);
    mvpActPool.reserve(mBurstNum * P1NODE_DEF_QUEUE_DEPTH);
    mvpActPool.clear();
    MY_LOGI("ActPool.Capacity[%d]", (MUINT32)mvpActPool.capacity());
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCtrl::reset() {
  flushAct();
  {
    std::lock_guard<std::mutex> _l(mTaskLock);
    mvpActPool.clear();
  }
  {
    std::lock_guard<std::mutex> _ll(mAccIdLock);
    mAccId = P1ACT_ID_FIRST;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1TaskCtrl::registerAct(P1QueAct* rSetAct) {
  MBOOL ret = MFALSE;
  if (CC_UNLIKELY(mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp is NULL");
    return ret;
  }
  if (CC_UNLIKELY(rSetAct->ptr() == nullptr)) {
    MY_LOGI("Act is not ready (%d)", rSetAct->id());
    return ret;
  }
  //
  std::lock_guard<std::mutex> _l(mTaskLock);
  ACT_TYPE type = rSetAct->getType();
  if (type == ACT_TYPE_NORMAL || type == ACT_TYPE_INTERNAL) {
    rSetAct->ptr()->magicNum = mspP1NodeImp->get_and_increase_magicnum();
    rSetAct->ptr()->updateMetaSet();
    mspP1NodeImp->mTagReq.set(rSetAct->ptr()->magicNum);
  } else {
    rSetAct->ptr()->magicNum = P1ACT_NUM_NULL;
  }
  mvpActPool.push_back(rSetAct->ptr());
  ret = MTRUE;
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1TaskCtrl::releaseAct(P1QueAct* rPutAct) {
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mTaskLock);
  //
  // in the most case, it will find the act at the first item
  MUINT idx = 0;
  std::vector<P1Act>::iterator it = mvpActPool.begin();
  for (; it != mvpActPool.end(); idx++) {
    if (CC_UNLIKELY(*it == nullptr)) {
      MY_LOGI("[P1ActCheck] POOL[%d] NULL ActPtr in ActQueue", idx);
      it = mvpActPool.erase(it);
      continue;
    } else if (*it == rPutAct->mpAct) {
      {
        MY_LOGI("[P1::ACT][%d] (id:%d num:%d type:%d)", idx, rPutAct->id(),
                rPutAct->getNum(), rPutAct->getType());
      }
      mvpActPool.erase(it);
      ret = MTRUE;
      break;
    }
    it++;
  }
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1TaskCtrl::flushAct(void) {
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mTaskLock);
  //
  // in the most case, there should be no act in the pool while flush
  MY_LOGI("[P1ActCheck] flush act [%zu]", mvpActPool.size());
  while (!mvpActPool.empty()) {
    MY_LOGI("flush act [%zu]", mvpActPool.size());
    dumpActPool();
    //
    std::vector<P1Act>::iterator it = mvpActPool.begin();
    if (*it == nullptr) {
      MY_LOGI("[P1ActCheck] NULL ActPtr in ActQueue");
      mvpActPool.erase(it);
      continue;
    } else {
      // in the most case, it should not exist act in queue while flush
      MY_LOGI("[P1ActCheck] Act(%d) in ActQueue", ((P1Act)(*it))->magicNum);
      // delete(*it); // use-act-ptr
      mvpActPool.erase(it);
      ret = MTRUE;
    }
  }
  MY_LOGI("flush act done [%zu]", mvpActPool.size());
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCtrl::sessionLock(void) {
  mSessionLock.lock();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCtrl::sessionUnLock(void) {
  mSessionLock.unlock();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1TaskCtrl::dumpActPool() {
  MY_LOGI("[P1ActCheck] dump ActPool [%zu]", mvpActPool.size());
  //
  std::vector<P1Act>::iterator it = mvpActPool.begin();
  MUINT idx = 0;
  for (; it != mvpActPool.end(); it++, idx++) {
    if (*it == nullptr) {
      MY_LOGI("[P1ActCheck] POOL[%d] : Act(NULL)", idx);
      continue;
    } else {
      // in the most case, it should not exist act in queue while flush
      MY_LOGI("[P1ActCheck] POOL[%d] : Act(%d) " P1INFO_ACT_STR, idx,
              ((P1Act)(*it))->magicNum, P1INFO_ACT_VAR(*(*it)));
    }
  }
  return;
}

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam
