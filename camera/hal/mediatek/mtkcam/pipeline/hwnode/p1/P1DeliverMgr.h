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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1DELIVERMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1DELIVERMGR_H_

#define LOG_TAG "MtkCam/P1NodeImp"
//
#include <memory>
#include "P1Common.h"
#include "P1Utility.h"
#include "P1TaskCtrl.h"
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

/******************************************************************************
 *
 ******************************************************************************/
class P1NodeImp;

/******************************************************************************
 *
 ******************************************************************************/
class P1DeliverMgr {
  typedef std::vector<MINT32> NumList_T;
  typedef std::vector<P1FrameAct> ActQueue_T;

 public:
  P1DeliverMgr();

  virtual ~P1DeliverMgr();

  void init(std::shared_ptr<P1NodeImp> pP1NodeImp);

  void uninit(void);

  void config(void);

  void runningSet(MBOOL bRunning);

  MBOOL runningGet(void);

  status_t run(void);

  void join(void);

  void exit(void);

 public:
  MBOOL isActListEmpty(void);

  MBOOL registerActList(MINT32 num);

  MBOOL sendActQueue(P1QueAct* rAct, MBOOL needTrigger);

  MBOOL waitFlush(MBOOL needTrigger);

  MBOOL trigger(void);

  MVOID dumpInfo(void);

 private:
  void dumpNumList(MBOOL isLock = MFALSE);

  void dumpActQueue(MBOOL isLock = MFALSE);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Thread Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  virtual status_t readyToRun();
  void requestExit();

 private:
  virtual bool threadLoop();
  virtual bool _threadLoop();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ENUM.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  enum LOOP_STATE {
    LOOP_STATE_INIT = 0,
    LOOP_STATE_WAITING,
    LOOP_STATE_PROCESSING,
    LOOP_STATE_DONE
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  std::shared_ptr<P1NodeImp> mspP1NodeImp;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MUINT8 mBurstNum;
  MBOOL mLoopRunning;
  LOOP_STATE mLoopState;
  std::condition_variable mDoneCond;
  std::condition_variable mDeliverCond;
  mutable std::mutex mDeliverLock;
  MINT32 mSentNum;
  NumList_T mNumList;
  ActQueue_T mActQueue;
  MBOOL mExitPending;
  std::thread mThread;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Function Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  MBOOL deliverLoop();
};

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1DELIVERMGR_H_
