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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1REGISTERNOTIFY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1REGISTERNOTIFY_H_

#define LOG_TAG "MtkCam/P1NodeImp"
//
#include <memory>
#include "P1Common.h"
#include "P1Utility.h"
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
class P1RegisterNotify;

/******************************************************************************
 *
 ******************************************************************************/
class P1NotifyCrop : public P1_TUNING_NOTIFY {
 public:
  explicit P1NotifyCrop(std::shared_ptr<P1RegisterNotify> spP1Notify)
      : mspP1Notify(spP1Notify) {}

  virtual ~P1NotifyCrop() {}

  virtual const char* TuningName() { return "P1NodeCrop"; }

  virtual void p1TuningNotify(MVOID* pIn, MVOID* pOut);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  std::shared_ptr<P1RegisterNotify> mspP1Notify;
};

/******************************************************************************
 *
 ******************************************************************************/
class P1NotifyQuality : public P1_TUNING_NOTIFY {
 public:
  explicit P1NotifyQuality(std::shared_ptr<P1RegisterNotify> spP1Notify)
      : mspP1Notify(spP1Notify) {}

  virtual ~P1NotifyQuality() {}

  virtual const char* TuningName() { return "P1NodeQuality"; }

  virtual void p1TuningNotify(MVOID* pIn, MVOID* pOut);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  std::shared_ptr<P1RegisterNotify> mspP1Notify;
};

/******************************************************************************
 *
 ******************************************************************************/
class P1RegisterNotify : public std::enable_shared_from_this<P1RegisterNotify> {
 public:
  explicit P1RegisterNotify(std::shared_ptr<P1NodeImp> spP1NodeImp);

  virtual ~P1RegisterNotify();

 public:
  MVOID init(MVOID);

  MVOID uninit(MVOID);

  MVOID config(MVOID);

  MVOID doNotifyCrop(MVOID* pIn, MVOID* pOut);

  MVOID doNotifyQuality(MVOID* pIn, MVOID* pOut);

  MVOID* getNotifyCrop(MVOID);

  MVOID* getNotifyQuality(MVOID);

  // private:

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  std::shared_ptr<P1NodeImp> mspP1NodeImp;
  MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mLogLevelI;
  MUINT8 mBurstNum;
  //
  std::shared_ptr<P1NotifyCrop> mpNotifyCrop;
  //
  std::shared_ptr<P1NotifyQuality> mpNotifyQuality;
  //

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Function Member.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
};

};  // namespace NSP1Node
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P1_P1REGISTERNOTIFY_H_
