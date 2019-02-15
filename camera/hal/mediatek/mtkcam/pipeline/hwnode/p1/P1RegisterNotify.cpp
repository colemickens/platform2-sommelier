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
#include "mtkcam/pipeline/hwnode/p1/P1RegisterNotify.h"
#include "P1TaskCtrl.h"
#include "P1NodeImp.h"
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
// P1NotifyCrop
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
void P1NotifyCrop::p1TuningNotify(MVOID* pIn, MVOID* pOut) {
  if (mspP1Notify != nullptr) {
    mspP1Notify->doNotifyCrop(pIn, pOut);
  }
  return;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// P1NotifyQuality
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
void P1NotifyQuality::p1TuningNotify(MVOID* pIn, MVOID* pOut) {
  if (mspP1Notify != nullptr) {
    mspP1Notify->doNotifyQuality(pIn, pOut);
  }
  return;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// P1RegisterNotify
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
P1RegisterNotify::P1RegisterNotify(std::shared_ptr<P1NodeImp> spP1NodeImp)
    : mspP1NodeImp(spP1NodeImp),
      mOpenId(-1),
      mLogLevel(0),
      mLogLevelI(0),
      mBurstNum(1),
      mpNotifyCrop(nullptr),
      mpNotifyQuality(nullptr) {
  config();
}

/******************************************************************************
 *
 ******************************************************************************/
P1RegisterNotify::~P1RegisterNotify() {}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1RegisterNotify::init() {
  MY_LOGD("+++");
  uninit();
  //
  if (mpNotifyCrop == nullptr) {
    mpNotifyCrop = std::make_shared<P1NotifyCrop>(shared_from_this());
  }
  //
  if (mpNotifyQuality == nullptr) {
    mpNotifyQuality = std::make_shared<P1NotifyQuality>(shared_from_this());
  }
  //
  MY_LOGD("---");
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1RegisterNotify::uninit() {
  MY_LOGD("+++");
  //
  mpNotifyCrop = nullptr;
  mpNotifyQuality = nullptr;
  //
  MY_LOGD("---");
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1RegisterNotify::config() {
  if (mspP1NodeImp != nullptr) {
    mOpenId = mspP1NodeImp->getOpenId();
    mLogLevel = mspP1NodeImp->mLogLevel;
    mLogLevelI = mspP1NodeImp->mLogLevelI;
    if (mspP1NodeImp->mBurstNum > 1) {
      mBurstNum = mspP1NodeImp->mBurstNum;
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1RegisterNotify::doNotifyCrop(MVOID* pIn, MVOID* pOut) {
  if (CC_UNLIKELY(mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp not exist");
    return;
  }
  if (CC_UNLIKELY(pIn == nullptr || pOut == nullptr)) {
    MY_LOGE("NotifyCrop In/Out NULL - In[%p] Out[%p]", pIn, pOut);
    return;
  }
  MY_LOGI("NotifyCrop - In[%p] Out[%p]", pIn, pOut);
  BIN_INPUT_INFO* pInInfo = reinterpret_cast<BIN_INPUT_INFO*>(pIn);
  RRZ_REG_CFG* pOutCfg = reinterpret_cast<RRZ_REG_CFG*>(pOut);
  MSize curSize = MSize(pInInfo->TarBinOut_W, pInInfo->TarBinOut_H);
  //
  mspP1NodeImp->setCurrentBinSize(curSize);
  pOutCfg->bRRZ_Bypass = MTRUE;
  //
  {
    // if (pInInfo != NULL && pOutCfg != NULL) // no need to check since the
    // pIn/pOut has checked
    MBOOL found = MFALSE;
    std::lock_guard<std::mutex> _l(mspP1NodeImp->mProcessingQueueLock);
    if (mspP1NodeImp->mProcessingQueue.empty()) {
      MY_LOGI("ProcessingQueue is empty num:%d", (MINT32)pInInfo->Magic);
      return;
    }
    P1QueAct qAct;
    std::vector<P1QueJob>::iterator it_job =
        mspP1NodeImp->mProcessingQueue.begin();
    for (; it_job != mspP1NodeImp->mProcessingQueue.end(); it_job++) {
      for (size_t i = 0; i < it_job->size(); i++) {
        if (it_job->edit(i).getNum() == (MINT32)pInInfo->Magic) {
          qAct = it_job->edit(i);
          found = MTRUE;
          break;
        }
      }
    }

    if (found) {
      if (qAct.getNum() != P1ACT_NUM_NULL) {
        P1Act act = GET_ACT_PTR(act, qAct, RET_VOID);
        MBOOL isSetChange = MFALSE;
        MBOOL isSizeChange = (act->refBinSize == curSize) ? MFALSE : MTRUE;
        // if (IS_BURST_OFF) // exclude burst mode
        mspP1NodeImp->attemptCtrlResize(&qAct, &isSetChange);
        if (isSetChange || isSizeChange) {
          MY_LOGI(
              "Resize Change set(%d) size(%d) cur" P1_SIZE_STR "ref" P1_SIZE_STR
              "crop" P1_RECT_STR "dst" P1_SIZE_STR "MaxRatio(%d)",
              isSetChange, isSizeChange, P1_SIZE_VAR(curSize),
              P1_SIZE_VAR(act->refBinSize), P1_RECT_VAR(act->cropRect_resizer),
              P1_SIZE_VAR(act->dstSize_resizer), mspP1NodeImp->mResizeRatioMax);
          if (act->refBinSize.w > curSize.w) {
            BIN_RESIZE(act->cropRect_resizer.p.x);
            BIN_RESIZE(act->cropRect_resizer.p.y);
            BIN_RESIZE(act->cropRect_resizer.s.w);
            BIN_RESIZE(act->cropRect_resizer.s.h);
          } else if (act->refBinSize.w < curSize.w) {
            BIN_REVERT(act->cropRect_resizer.p.x);
            BIN_REVERT(act->cropRect_resizer.p.y);
            BIN_REVERT(act->cropRect_resizer.s.w);
            BIN_REVERT(act->cropRect_resizer.s.h);
          }  // for performance consideration, only check W
          MY_LOGI("BIN check crop" P1_RECT_STR "dst" P1_SIZE_STR,
                  P1_RECT_VAR(act->cropRect_resizer),
                  P1_SIZE_VAR(act->dstSize_resizer));
          //
          if (act->cropRect_resizer.s.w * mspP1NodeImp->mResizeRatioMax >
              act->dstSize_resizer.w * 100) {
            act->dstSize_resizer.w =
                ((act->cropRect_resizer.s.w * mspP1NodeImp->mResizeRatioMax) +
                 (100 - 1)) /
                100;
            act->dstSize_resizer.w = ALIGN_UPPER(act->dstSize_resizer.w, 2);
          } else if (act->cropRect_resizer.s.w < act->dstSize_resizer.w) {
            act->dstSize_resizer.w = act->cropRect_resizer.s.w;
          }
          //
          if (act->cropRect_resizer.s.h * mspP1NodeImp->mResizeRatioMax >
              act->dstSize_resizer.h * 100) {
            act->dstSize_resizer.h =
                ((act->cropRect_resizer.s.h * mspP1NodeImp->mResizeRatioMax) +
                 (100 - 1)) /
                100;
            act->dstSize_resizer.h = ALIGN_UPPER(act->dstSize_resizer.h, 2);
          } else if (act->cropRect_resizer.s.h < act->dstSize_resizer.h) {
            act->dstSize_resizer.h = act->cropRect_resizer.s.h;
          }
          MY_LOGI("LMT check crop" P1_RECT_STR "dst" P1_SIZE_STR,
                  P1_RECT_VAR(act->cropRect_resizer),
                  P1_SIZE_VAR(act->dstSize_resizer));
          {
            // if (IS_BURST_OFF) // exclude burst mode
            pOutCfg->bRRZ_Bypass = MFALSE;
            pOutCfg->src_x = act->cropRect_resizer.p.x;
            pOutCfg->src_y = act->cropRect_resizer.p.y;
            pOutCfg->src_w = act->cropRect_resizer.s.w;
            pOutCfg->src_h = act->cropRect_resizer.s.h;
            pOutCfg->tar_w = act->dstSize_resizer.w;
            pOutCfg->tar_h = act->dstSize_resizer.h;
            MY_LOGI(
                "Resize Change set(%d) size(%d) End -"
                "OutCfg[Bypass:%d src(%d,%d-%dx%d) tar(%dx%d)]",
                isSetChange, isSizeChange, pOutCfg->bRRZ_Bypass, pOutCfg->src_x,
                pOutCfg->src_y, pOutCfg->src_w, pOutCfg->src_h, pOutCfg->tar_w,
                pOutCfg->tar_h);
          }
        }
      }
    } else {
      MY_LOGW("Notify Frame Not Found - Drv(%d)", pInInfo->Magic);
      size_t amount = mspP1NodeImp->mProcessingQueue.size();
      MUINT32 i = 0;
      std::vector<P1QueJob>::iterator it =
          mspP1NodeImp->mProcessingQueue.begin();
      for (; it != mspP1NodeImp->mProcessingQueue.end(); it++, i++) {
        MY_LOGW("ProcessingQueue[%d/%zu] = job(%d-%d)", i, amount,
                (*it).getIdx(), (*it).getLastNum());
      }
    }
  }
  return;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1RegisterNotify::doNotifyQuality(MVOID* pIn, MVOID* pOut) {
  if (CC_UNLIKELY(mspP1NodeImp == nullptr)) {
    MY_LOGE("P1NodeImp not exist");
    return;
  }
  MY_LOGI("NotifyQuality - In[%p] Out[%p]", pIn, pOut);
  //
  mspP1NodeImp->setQualitySwitching(MFALSE);
  return;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID* P1RegisterNotify::getNotifyCrop() {
  if (CC_UNLIKELY(mpNotifyCrop == nullptr)) {
    MY_LOGE("NotifyCrop not exist");
  }
  return reinterpret_cast<MVOID*>(mpNotifyCrop.get());
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID* P1RegisterNotify::getNotifyQuality() {
  if (CC_UNLIKELY(mpNotifyQuality == nullptr)) {
    MY_LOGE("NotifyQuality not exist");
  }
  return reinterpret_cast<MVOID*>(mpNotifyQuality.get());
}

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam
