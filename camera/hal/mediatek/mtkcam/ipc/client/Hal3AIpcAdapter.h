/*
 * Copyright (C) 2019 Mediatek Corporation.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HAL3AIPCADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HAL3AIPCADAPTER_H_
#include <base/bind.h>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Errors.h>
#include <IPCHal3a.h>
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/aaa/IHal3ACb.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>

#include "Hal3aIpcCommon.h"
#include "Mediatek3AClient.h"

namespace NS3Av3 {

class Hal3ACbSet : public IHal3ACb {
 public:
  Hal3ACbSet();
  virtual ~Hal3ACbSet();

  virtual void doNotifyCb(MINT32 _msgType,
                          MINTPTR _ext1,
                          MINTPTR _ext2,
                          MINTPTR _ext3);

  virtual MINT32 addCallback(IHal3ACb* cb);
  virtual MINT32 removeCallback(IHal3ACb* cb);

 private:
  std::list<IHal3ACb*> m_CallBacks;
  std::mutex m_Mutex;
};

class Hal3AIpcAdapter : public IHal3A {
 public:
  static IHal3A* getInstance(MINT32 const i4SensorOpenIndex,
                             const char* strUser);

  /**
   * @brief destroy instance of IHal3A
   */
  virtual MVOID destroyInstance(const char* /*strUser*/);
  /**
   * @brief config 3A
   */
  virtual MINT32 config(const ConfigInfo_T& rConfigInfo);
  /**
   * @brief config 3A with default setting
   */
  virtual MINT32 config(MINT32 i4SubsampleCount = 0) {
    ConfigInfo_T info;
    info.i4SubsampleCount = i4SubsampleCount;
    return config(info);
  }
  /**
   * @brief start 3A
   */
  virtual MINT32 start(MINT32 i4StartNum = 0);
  /**
   * @brief stop 3A
   */
  virtual MINT32 stop();

  /**
   * @brief stop statistical data
   */
  virtual MVOID stopStt();

  /**
   * @brief pause 3A
   */
  virtual MVOID pause() { return; }
  /**
   * @brief resume 3A
   */
  virtual MVOID resume(MINT32 MagicNum = 0) { return; }

  /****************************************************************
   *************************CAM3_3A_ISP50_EN***********************
   ***************************************************************/

  /**
   * @brief Set list of controls in terms of metadata via IHal3A
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 startRequestQ(const std::vector<MetaSet_T*>& requestQ);

  /**
   * @brief Set list of controls in terms of metadata of capture request via
   * IHal3A
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 startCapture(const std::vector<MetaSet_T*>& requestQ,
                              MINT32 i4StartNum = 0);

  /**
   * Differences from ISP4.0 set : No more Android List and Size requisition
   * Middleware will guarantee that the size of vector won't be modified during
   * set function is working
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 set(const std::vector<MetaSet_T*>& requestQ);

  /**
   * Difference from set : Due to full CCU feature, current request must be
   * delivered before Vsync Middleware will guarantee that the size of vector
   * won't be modified during set function is working
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 preset(const std::vector<MetaSet_T*>& requestQ);

  /**
   * @brief Set pass2 tuning in terms of metadata via IHal3A
   * @param [in] flowType 0 for processed raw, 1 for pure raw
   * @param [in] control MetaSet_T
   * @param [out] pRegBuf buffer address for register setting
   * @param [out] result IMetadata
   */
  virtual MINT32 setIsp(MINT32 flowType,
                        const MetaSet_T& control,
                        TuningParam* pTuningBuf,
                        MetaSet_T* pResult);

  /**
   * @brief Get dynamic result with specified frame ID via IHal3A
   * @param [in] frmId specified frame ID (magic number)
   * @param [out] result in terms of metadata
   */
  virtual MINT32 get(MUINT32 frmId, MetaSet_T* result);
  virtual MINT32 getCur(MUINT32 frmId, MetaSet_T* result);

  /**
   * @brief Attach callback for notifying
   * @param [in] eId Notification message type
   * @param [in] pCb Notification callback function pointer
   */
  virtual MINT32 attachCb(IHal3ACb::ECb_T eId, IHal3ACb* pCb);

  /**
   * @brief Dettach callback
   * @param [in] eId Notification message type
   * @param [in] pCb Notification callback function pointer
   */
  virtual MINT32 detachCb(IHal3ACb::ECb_T eId, IHal3ACb* pCb);

  /**
   * @brief Get delay frames via IHal3A
   * @param [out] delay_info in terms of metadata with MTK defined tags.
   */
  virtual MINT32 getDelay(NSCam::IMetadata const& delay_info) const {
    return 0;
  }

  /**
   * @brief Get delay frames via IHal3A
   * @param [in] tag belongs to control+dynamic
   * @return
   * - MINT32 delay frame.
   */
  virtual MINT32 getDelay(MUINT32 tag) const { return 0; }

  /**
   * @brief Get capacity of metadata list via IHal3A
   * @return
   * - MINT32 value of capacity.
   */
  virtual MINT32 getCapacity() const { return 0; }

  virtual MINT32 send3ACtrl(E3ACtrl_T e3ACtrl, MINTPTR i4Arg1, MINTPTR i4Arg2);

  /**
   * @brief set sensor mode
   * @param [in] i4SensorMode
   */
  virtual MVOID setSensorMode(MINT32 i4SensorMode);

#ifdef CAM3_3A_ISP50_EN
  virtual MVOID notifyP1Done(MINT32 i4MagicNum, MVOID* pvArg = 0) { return; }
#else
  virtual MVOID notifyP1Done(MUINT32 u4MagicNum, MVOID* pvArg = 0);
#endif

  /**
   * @brief notify sensor power on
   */
  virtual MBOOL notifyPwrOn() { return 0; }
  /**
   * @brief notify sensor power off
   */
  virtual MBOOL notifyPwrOff() { return 0; }
  /**
   * @brief notify CCU power .
   * @return
   * - MBOOL value of TRUE/FALSE.
   */
  virtual MBOOL notifyP1PwrOn();

  /**
   * @brief notify CCU power off.
   * @return
   * - MBOOL value of TRUE/FALSE.
   */
  virtual MBOOL notifyP1PwrOff();
  /**
   * @brief check whether flash on while capture
   */
  virtual MBOOL checkCapFlash() { return 0; }

  virtual MVOID setFDEnable(MBOOL fgEnable) { return; }

  virtual MBOOL setFDInfo(MVOID* prFaces) { return 0; }

  virtual MBOOL setFDInfoOnActiveArray(MVOID* prFaces);

  virtual MBOOL setOTInfo(MVOID* prOT) { return 0; }

  /**
   * @brief dump pass2 tuning in terms of metadata via IHal3A
   * @param [in] flowType 0 for processed raw, 1 for pure raw
   * @param [in] control MetaSet_T
   * @param [out] pRegBuf buffer address for register setting
   */
  virtual MINT32 dumpIsp(MINT32 flowType,
                         const MetaSet_T& control,
                         TuningParam* pTuningBuf,
                         MetaSet_T* pResult) {
    return 0;
  }

  void runCallback(int msg);

 protected:
  explicit Hal3AIpcAdapter(MINT32 const i4SensorIdx);

  virtual ~Hal3AIpcAdapter();

  bool doInit(MINT32 const i4SensorIdx, const char* strUser);
  void doUninit();

  MINT32 sendRequest(IPC_CMD cmd, ShmMemInfo* memInfo, int32_t group);

  MINT32 sendRequest(IPC_CMD cmd, ShmMemInfo* memInfo);

  MINT32 MetaSetFlatten(const std::vector<MetaSet_T*>& requestQ,
                        struct hal3a_metaset_params* params);

  MINT32 send3ACtrlHalSensor(hal3a_send3actrl_params* params,
                             E3ACtrl_T e3ACtrl,
                             MINTPTR i4Arg1,
                             MINTPTR i4Arg2);
  MINT32 send3ACtrlPeriSensor(hal3a_send3actrl_params* params,
                              E3ACtrl_T e3ACtrl,
                              MINTPTR i4Arg1,
                              MINTPTR i4Arg2);

  Mtk3aCommon mCommon;

  bool mInitialized;

  ShmMemInfo mMemInit;
  ShmMemInfo mMemConfig;
  ShmMemInfo mMemStart;
  ShmMemInfo mMemStop;
  ShmMemInfo mMemStopStt;
  ShmMemInfo mMemSet;
  ShmMemInfo mMemSetIsp;
  ShmMemInfo mMemStartCapture;
  ShmMemInfo mMemStartRequestQ;
  ShmMemInfo mMempPreset;
  ShmMemInfo mMemSend3aCtrl;
  ShmMemInfo mMemGetSensorParam;
  ShmMemInfo mMemNotifyCallBack;
  ShmMemInfo mMemTuningPipe;
  ShmMemInfo mMemSttPipe;
  ShmMemInfo mMemStt2Pipe;
  ShmMemInfo mMemHwEvent;
  ShmMemInfo mMemAePlineLimit;
  ShmMemInfo mMemAfLensConfig;
  ShmMemInfo mMemAfLensEnable;
  ShmMemInfo mMemNofifyP1PwrOn;
  ShmMemInfo mMemNofifyP1PwrOff;
  ShmMemInfo mMemNofifyP1Done;
  ShmMemInfo mMemSetSensorMode;
  ShmMemInfo mMemAttachCB;
  ShmMemInfo mMemDetachCB;
  ShmMemInfo mMemGet;
  ShmMemInfo mMemGetCur;
  ShmMemInfo mMemSetFDInfo;

  std::vector<ShmMem> mMems;
  // key: stt1/2 dma fd from client
  // value: handle that returns from RegisterBuffer()
  std::unordered_map<int32_t, int32_t> mSttIPCHandles;
  std::unordered_map<int32_t, int32_t> mStt2IPCHandles;
  std::unordered_map<int32_t, int32_t> mLceIPCHandles;
  std::unordered_map<int32_t, int32_t> mP2TuningBufHandles;
  std::unordered_map<int32_t, int32_t> mP1TuningBufHandles;

  // key: ipc fd
  // value: IPC_Metabuf1_T/IPC_Metabuf2_T
  std::unordered_map<int32_t, IPC_Metabuf1_T> mMetaBuf1Pool;
  std::unordered_map<int32_t, IPC_Metabuf2_T> mMetaBuf2Pool;

  std::shared_ptr<NSCam::IImageBuffer> m_pLsc2ImgBuf;
  std::mutex mIspMutex;
  std::mutex mInitMutex;
  std::unordered_map<std::string, MINT32> m_Users;

  Hal3ACbSet m_CbSet[IHal3ACb::eID_MSGTYPE_NUM];

  MINT32 m_i4SensorIdx;
};

};      // namespace NS3Av3
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HAL3AIPCADAPTER_H_
