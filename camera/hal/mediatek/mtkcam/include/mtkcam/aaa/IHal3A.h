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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IHAL3A_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IHAL3A_H_

#include <memory>
#include <vector>
#include <mtkcam/aaa/aaa_hal_common.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/module/module.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include "IHal3ACb.h"

namespace NS3Av3 {
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
struct MetaSet_T {
  MINT32 MagicNum;
  MUINT8 Dummy;
  MINT32 PreSetKey;
  NSCam::IMetadata appMeta;
  NSCam::IMetadata halMeta;

  MetaSet_T() : MagicNum(-1), Dummy(0), PreSetKey(-1) {}
  MetaSet_T(MINT32 _MagicNum,
            MUINT8 _Dummy,
            MINT32 _PreSetKey,
            NSCam::IMetadata _appMeta,
            NSCam::IMetadata _halMeta)
      : MagicNum(_MagicNum),
        Dummy(_Dummy),
        PreSetKey(_PreSetKey),
        appMeta(_appMeta),
        halMeta(_halMeta) {}
  MetaSet_T(NSCam::IMetadata appMeta, NSCam::IMetadata halMeta)
      : MetaSet_T(-1, 0, -1, appMeta, halMeta) {}
};
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**
 * @brief Interface of 3A Hal Class
 */
class IHal3A {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  //    Ctor/Dtor.
  IHal3A() {}
  virtual ~IHal3A() {}

 private:  // disable copy constructor and copy assignment operator
  IHal3A(const IHal3A&);
  IHal3A& operator=(const IHal3A&);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  enum E_VER { E_Camera_1 = 0, E_Camera_3 = 1 };

  /**
   * @brief destroy instance of IHal3A
   */
  virtual MVOID destroyInstance(const char* /*strUser*/) = 0;
  /**
   * @brief config 3A
   */
  virtual MINT32 config(const ConfigInfo_T& rConfigInfo) = 0;
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
  virtual MINT32 start(MINT32 i4StartNum = 0) = 0;
  /**
   * @brief stop 3A
   */
  virtual MINT32 stop() = 0;

  /**
   * @brief stop Stt
   */
  virtual MVOID stopStt() = 0;

  /**
   * @brief pause 3A
   */
  virtual MVOID pause() = 0;
  /**
   * @brief resume 3A
   */
  virtual MVOID resume(MINT32 MagicNum = 0) = 0;

  /****************************************************************
   *************************CAM3_3A_ISP50_EN***********************
   ***************************************************************/

  /**
   * @brief Set list of controls in terms of metadata via IHal3A
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 startRequestQ(const std::vector<MetaSet_T*>& requestQ) = 0;

  /**
   * @brief Set list of controls in terms of metadata of capture request via
   * IHal3A
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 startCapture(const std::vector<MetaSet_T*>& requestQ,
                              MINT32 i4StartNum = 0) = 0;

  /**
   * Differences from ISP4.0 set : No more Android List and Size requisition
   * Middleware will guarantee that the size of vector won't be modified during
   * set function is working
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 set(const std::vector<MetaSet_T*>& requestQ) = 0;

  /**
   * Difference from set : Due to full CCU feature, current request must be
   * delivered before Vsync Middleware will guarantee that the size of vector
   * won't be modified during set function is working
   * @param [in] controls list of MetaSet_T
   */
  virtual MINT32 preset(const std::vector<MetaSet_T*>& requestQ) = 0;

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
                        MetaSet_T* pResult) = 0;

  /**
   * @brief Get dynamic result with specified frame ID via IHal3A
   * @param [in] frmId specified frame ID (magic number)
   * @param [out] result in terms of metadata
   */
  virtual MINT32 get(MUINT32 frmId, MetaSet_T* result) = 0;
  virtual MINT32 getCur(MUINT32 frmId, MetaSet_T* result) = 0;

  /**
   * @brief Attach callback for notifying
   * @param [in] eId Notification message type
   * @param [in] pCb Notification callback function pointer
   */
  virtual MINT32 attachCb(IHal3ACb::ECb_T eId, IHal3ACb* pCb) = 0;

  /**
   * @brief Dettach callback
   * @param [in] eId Notification message type
   * @param [in] pCb Notification callback function pointer
   */
  virtual MINT32 detachCb(IHal3ACb::ECb_T eId, IHal3ACb* pCb) = 0;

  /**
   * @brief Get delay frames via IHal3A
   * @param [out] delay_info in terms of metadata with MTK defined tags.
   */
  virtual MINT32 getDelay(NSCam::IMetadata const& delay_info) const = 0;

  /**
   * @brief Get delay frames via IHal3A
   * @param [in] tag belongs to control+dynamic
   * @return
   * - MINT32 delay frame.
   */
  virtual MINT32 getDelay(MUINT32 tag) const = 0;

  /**
   * @brief Get capacity of metadata list via IHal3A
   * @return
   * - MINT32 value of capacity.
   */
  virtual MINT32 getCapacity() const = 0;

  virtual MINT32 send3ACtrl(E3ACtrl_T e3ACtrl,
                            MINTPTR i4Arg1,
                            MINTPTR i4Arg2) = 0;

  /**
   * @brief set sensor mode
   * @param [in] i4SensorMode
   */
  virtual MVOID setSensorMode(MINT32 i4SensorMode) = 0;

#ifdef CAM3_3A_ISP50_EN
  virtual MVOID notifyP1Done(MINT32 i4MagicNum, MVOID* pvArg = 0) = 0;
#else
  virtual MVOID notifyP1Done(MUINT32 u4MagicNum, MVOID* pvArg = 0) = 0;
#endif

  /**
   * @brief notify sensor power on
   */
  virtual MBOOL notifyPwrOn() = 0;
  /**
   * @brief notify sensor power off
   */
  virtual MBOOL notifyPwrOff() = 0;
  /**
   * @brief notify CCU power .
   * @return
   * - MBOOL value of TRUE/FALSE.
   */
  virtual MBOOL notifyP1PwrOn() = 0;

  /**
   * @brief notify CCU power off.
   * @return
   * - MBOOL value of TRUE/FALSE.
   */
  virtual MBOOL notifyP1PwrOff() = 0;
  /**
   * @brief check whether flash on while capture
   */
  virtual MBOOL checkCapFlash() = 0;

  virtual MVOID setFDEnable(MBOOL fgEnable) = 0;

  virtual MBOOL setFDInfo(MVOID* prFaces) = 0;

  virtual MBOOL setFDInfoOnActiveArray(MVOID* prFaces) = 0;

  virtual MBOOL setOTInfo(MVOID* prOT) = 0;

  /**
   * @brief dump pass2 tuning in terms of metadata via IHal3A
   * @param [in] flowType 0 for processed raw, 1 for pure raw
   * @param [in] control MetaSet_T
   * @param [out] pRegBuf buffer address for register setting
   */
  virtual MINT32 dumpIsp(MINT32 flowType,
                         const MetaSet_T& control,
                         TuningParam* pTuningBuf,
                         MetaSet_T* pResult) = 0;
};

};  // namespace NS3Av3

/**
 * @brief The definition of the maker of IHal3A instance.
 */

#if (MTKCAM_ENABLE_IPC == 1)

typedef NS3Av3::IHal3A* (*Hal3A_FACTORY_T)(MINT32 const i4SensorIdx,
                                           const char* strUser);
#define MAKE_Hal3A(_ret, _deleter, ...)                                      \
  do {                                                                       \
    std::shared_ptr<NS3Av3::IHal3A> _pHal3A(                                 \
        MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_HAL_IPC_3A, Hal3A_FACTORY_T, \
                           __VA_ARGS__),                                     \
        _deleter);                                                           \
    _ret = _pHal3A;                                                          \
  } while (0)

#else

typedef NS3Av3::IHal3A* (*Hal3A_FACTORY_T)(MINT32 const i4SensorIdx,
                                           const char* strUser);
#define MAKE_Hal3A(_ret, _deleter, ...)                                  \
  do {                                                                   \
    std::shared_ptr<NS3Av3::IHal3A> _pHal3A(                             \
        MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_HAL_3A, Hal3A_FACTORY_T, \
                           __VA_ARGS__),                                 \
        _deleter);                                                       \
    _ret = _pHal3A;                                                      \
  } while (0)

#endif

typedef NS3Av3::IHal3A* (*Hal3A_IPC_FACTORY_T)(MINT32 const i4SensorIdx,
                                               const char* strUser);
#define MAKE_Hal3A_IPC(_ret, _deleter, ...)                                  \
  do {                                                                       \
    std::shared_ptr<NS3Av3::IHal3A> _pHal3A(                                 \
        MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_HAL_3A, Hal3A_IPC_FACTORY_T, \
                           __VA_ARGS__),                                     \
        _deleter);                                                           \
    _ret = _pHal3A;                                                          \
  } while (0)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IHAL3A_H_
