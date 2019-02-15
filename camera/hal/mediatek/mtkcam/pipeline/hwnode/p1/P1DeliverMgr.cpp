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
#include "mtkcam/pipeline/hwnode/p1/P1DeliverMgr.h"
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
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/******************************************************************************
 *
 ******************************************************************************/
P1DeliverMgr::P1DeliverMgr()
    : mspP1NodeImp(nullptr),
      mOpenId(-1),
      mLogLevel(0),
      mLogLevelI(0),
      mBurstNum(1),
      mLoopRunning(MFALSE),
      mLoopState(LOOP_STATE_INIT),
      mSentNum(0),
      mExitPending(MFALSE) {}

/******************************************************************************
 *
 ******************************************************************************/
P1DeliverMgr::~P1DeliverMgr() {
  mNumList.clear();
  mActQueue.clear();
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::init(std::shared_ptr<P1NodeImp> pP1NodeImp) {
  mspP1NodeImp = pP1NodeImp;
  config();
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::uninit() {
  exit();
  mspP1NodeImp = nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::config() {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  mNumList.clear();
  mActQueue.clear();
  if (mspP1NodeImp != nullptr) {
    mOpenId = mspP1NodeImp->getOpenId();
    mLogLevel = mspP1NodeImp->mLogLevel;
    mLogLevelI = mspP1NodeImp->mLogLevelI;
    if (mspP1NodeImp->mBurstNum > 1) {
      mBurstNum = mspP1NodeImp->mBurstNum;
    }
    size_t const res_size = (size_t)(mBurstNum * P1NODE_DEF_QUEUE_DEPTH);
    mNumList.reserve(res_size);
    mNumList.clear();
    mActQueue.reserve(res_size);
    mActQueue.clear();
    MY_LOGI("ActQueue.Capacity[%d]", (MUINT32)mActQueue.capacity());
  }
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::runningSet(MBOOL bRunning) {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  mLoopRunning = bRunning;
  return;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::runningGet() {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  return mLoopRunning;
}

/******************************************************************************
 *
 ******************************************************************************/

status_t P1DeliverMgr::run() {
  mThread = std::thread(std::bind(&P1DeliverMgr::threadLoop, this));
  return NO_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::join() {
  if (mThread.joinable()) {
    mThread.join();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::exit() {
  MY_LOGD("DeliverMgr loop exit");
  {
    std::lock_guard<std::mutex> _l(mDeliverLock);
    mExitPending = MTRUE;
  }
  trigger();
  MY_LOGD("DeliverMgr loop join");
  if (mThread.joinable()) {
    mThread.join();
  }
  MY_LOGD("DeliverMgr loop finish");
}

/******************************************************************************
 *
 ******************************************************************************/
status_t P1DeliverMgr::readyToRun() {
  MY_LOGD("readyToRun DeliverMgr thread");
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::requestExit() {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  mExitPending = true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool P1DeliverMgr::threadLoop() {
  while (this->_threadLoop() == MTRUE) {
  }
  MY_LOGI("threadLoop exit");
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool P1DeliverMgr::_threadLoop() {
  {
    std::lock_guard<std::mutex> _l(mDeliverLock);
    if (mExitPending) {
      MY_LOGD("DeliverMgr try to leave");
      if (mActQueue.size() > 0) {
        MY_LOGI("the deliver queue is not empty, go-on the loop");
      } else {
        MY_LOGI("DeliverMgr Leaving");
        return MFALSE;
      }
    }
  }

  return deliverLoop();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::deliverLoop() {
  MBOOL res = MTRUE;
  std::vector<P1FrameAct> outQueue;
  outQueue.clear();
  outQueue.reserve((size_t)(mBurstNum * P1NODE_DEF_QUEUE_DEPTH));
  //
  if (mspP1NodeImp != nullptr && mspP1NodeImp->mpTimingCheckerMgr != nullptr) {
    mspP1NodeImp->mpTimingCheckerMgr->onCheck();
  }
  //
  if (mspP1NodeImp != nullptr) {  // check the DropQ
    mspP1NodeImp->onProcessDropFrame(MFALSE);
  }
  //
  {
    std::unique_lock<std::mutex> _l(mDeliverLock);
    MINT32 currentNum = 0;
    //
    mLoopState = LOOP_STATE_INIT;
    currentNum = (mActQueue.empty()) ? (P1_FRM_NUM_NULL)
                                     : ((mActQueue.end() - 1)->frmNum);

    if (!mExitPending) {
      if (currentNum == mSentNum) {
        mLoopState = LOOP_STATE_WAITING;
        MY_LOGD("deliverLoop wait ++");
        std::cv_status status = mDeliverCond.wait_for(
            _l, std::chrono::nanoseconds(P1_DELIVERY_CHECK_INV_NS));
        if (status == std::cv_status::timeout) {
          MY_LOGI("Delivery(%lld) NumList[%zu] NodeQueue[%zu]",
                  P1_DELIVERY_CHECK_INV_NS, mNumList.size(), mActQueue.size());
          if ((!mNumList.empty()) || (!mActQueue.empty())) {
            dumpNumList(MFALSE);
            dumpActQueue(MFALSE);
          }
          if (mspP1NodeImp != nullptr) {
            if (mspP1NodeImp->mLongExp.get() == MFALSE) {
              mspP1NodeImp->mLogInfo.inspect(LogInfo::IT_NO_DELIVERY);
            }
          }
        }
        MY_LOGD("deliverLoop wait --");
      }  // else , there is new coming node need to check
    } else {
      MY_LOGD("deliverLoop need to exit");
    }
    //
    mLoopState = LOOP_STATE_PROCESSING;
    mSentNum = currentNum;
    //
    NumList_T::iterator it_list = mNumList.begin();
    ActQueue_T::iterator it_act = mActQueue.begin();
    MBOOL isFound = MFALSE;
    size_t i = mNumList.size();
    for (; it_list != mNumList.end() && i > 0; i--) {
      isFound = MFALSE;
      it_act = mActQueue.begin();
      for (; it_act != mActQueue.end();) {
        if (it_act->frmNum == (*it_list)) {
          outQueue.push_back(*it_act);
          it_act = mActQueue.erase(it_act);
          isFound = MTRUE;
          break;
        } else {
          it_act++;
        }
      }
      if (isFound) {  // this number of list is found in the ActQueue, go-on to
                      // find the next number of list to output
        it_list = mNumList.erase(it_list);
      } else {  // this number of list is not found in the ActQueue, stop the
                // searching and wait for the next trigger (ex: sendActQueue)
        break;
      }
    }
    // check the BYPASS act
    if (!mActQueue.empty()) {  // in the most case, while BYPASS act exist, the
                               // BYPASS act is the first and the only one act
                               // in the ActQueue
      ActQueue_T::iterator it_check = mActQueue.begin();
      for (; it_check != mActQueue.end();) {  // the bypass acts should be
                                              // in-order in the ActQueue
        if (it_check->reqType == REQ_TYPE_ZSL ||
            it_check->reqType == REQ_TYPE_REDO ||
            it_check->reqType == REQ_TYPE_YUV) {
          outQueue.push_back(*it_check);
          it_check = mActQueue.erase(it_check);  // go-on
        } else {
          it_check++;
        }
      }
    }
  }
  //
  if (!outQueue.empty()) {
    std::vector<P1FrameAct>::iterator it = outQueue.begin();
    for (; it != outQueue.end(); it++) {
      mspP1NodeImp->releaseFrame(&(*it));
      /* DO NOT use this P1QueAct after releaseAction() */
    }
    outQueue.clear();
  }

  {
    std::lock_guard<std::mutex> _l(mDeliverLock);
    mLoopState = LOOP_STATE_DONE;
    mDoneCond.notify_all();
  }

  return res;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::isActListEmpty() {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  if (CC_UNLIKELY(!mLoopRunning)) {
    return MTRUE;
  }
  return (mNumList.empty());
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::registerActList(MINT32 num) {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  if (CC_UNLIKELY(!mLoopRunning)) {
    return MFALSE;
  }
  mNumList.push_back(num);
  return (!mNumList.empty());
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::sendActQueue(P1QueAct* rAct, MBOOL needTrigger) {
  P1Act act = GET_ACT_PTR(act, *rAct, MFALSE);
  P1FrameAct frameAct(rAct);
  if (CC_LIKELY(frameAct.ready())) {
    std::lock_guard<std::mutex> _l(mDeliverLock);
    mActQueue.push_back(frameAct);
  } else {
    MY_LOGE("FrameAct not ready to deliver - " P1INFO_ACT_STR,
            P1INFO_ACT_VAR(*act));
  }
  if (needTrigger) {
    trigger();
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::waitFlush(MBOOL needTrigger) {
  size_t queueSize = 0;
  LOOP_STATE loopState = LOOP_STATE_INIT;
  MBOOL isListEmpty = MFALSE;
  if (!runningGet()) {
    return MTRUE;
  }
  //
  {
    std::lock_guard<std::mutex> _l(mDeliverLock);
    queueSize = mActQueue.size();
    isListEmpty =
        mNumList.empty();  // (mNumList.size() > 0) ? (MFALSE) : (MTRUE);
    loopState = mLoopState;
  }
  status_t status = OK;
  while ((queueSize > 0) || (loopState == LOOP_STATE_PROCESSING) ||
         (status != OK)) {
    if (needTrigger) {
      trigger();
    }
    {
      std::unique_lock<std::mutex> _l(mDeliverLock);
      if ((mActQueue.size() > 0 && mLoopState == LOOP_STATE_WAITING) ||
          (mLoopState == LOOP_STATE_PROCESSING)) {
        MY_LOGD("doneLoop wait ++");
        std::cv_status err = mDoneCond.wait_for(
            _l, std::chrono::nanoseconds(P1_COMMON_CHECK_INV_NS));
        MY_LOGD("doneLoop wait --");
        if (err == std::cv_status::timeout) {
          MY_LOGI(
              "WaitFlushStatus(%d) LoopState(%d) "
              "NumList[%zu] NodeQueue[%zu]",
              status, mLoopState, mNumList.size(), mActQueue.size());
          if (mspP1NodeImp != nullptr) {
            mspP1NodeImp->mLogInfo.inspect(LogInfo::IT_FLUSH_BLOCKING);
          }
        }
      } else {
        status = OK;
      }
      queueSize = mActQueue.size();
      isListEmpty =
          mNumList.empty();  // (mNumList.size() > 0) ? (MFALSE) : (MTRUE);
      loopState = mLoopState;
    }
  }
  //
  if (CC_UNLIKELY(!isListEmpty)) {
    MY_LOGW("ListEmpty(%d)", isListEmpty);
    dumpInfo();
    return MFALSE;
  }
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
P1DeliverMgr::trigger(void) {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  if (mLoopRunning) {
    MY_LOGD("DeliverMgr trigger (%zu)", mActQueue.size());
    mDeliverCond.notify_all();
  }
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
P1DeliverMgr::dumpInfo(void) {
  std::lock_guard<std::mutex> _l(mDeliverLock);
  MY_LOGI("DeliverMgr - Burst(%d) LoopRunning(%d) LoopState(%d)", mBurstNum,
          mLoopRunning, mLoopState);
  dumpNumList(MFALSE);
  dumpActQueue(MFALSE);
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::dumpNumList(MBOOL isLock) {
  std::string str("");
  size_t size = 0;
  //
  NEED_LOCK(isLock, mDeliverLock);
  //
  size = mNumList.size();
  NumList_T::iterator it = mNumList.begin();
  for (; it != mNumList.end(); it++) {
    str += base::StringPrintf("%d ", *it);
  }
  //
  NEED_UNLOCK(isLock, mDeliverLock);
  //
  MY_LOGI("dump NumList[%zu] = {%s}", size, str.c_str());
}

/******************************************************************************
 *
 ******************************************************************************/
void P1DeliverMgr::dumpActQueue(MBOOL isLock) {
  std::string str("");
  size_t size = 0;
  //
  NEED_LOCK(isLock, mDeliverLock);
  //
  size = mActQueue.size();
  ActQueue_T::iterator it = mActQueue.begin();
  for (; it != mActQueue.end(); it++) {
    str += base::StringPrintf("%d ", (*it).queId);
  }
  //
  NEED_UNLOCK(isLock, mDeliverLock);
  //
  MY_LOGI("dump ActQueue[%zu] = {%s}", size, str.c_str());
}

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam
