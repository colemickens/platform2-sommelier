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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IISPMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IISPMGR_H_

#include <mtkcam/aaa/aaa_hal_common.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/module/module.h>

namespace NS3Av3 {
using NSCam::MRect;
using NSCam::MSize;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
struct RMG_Config_Param {
  MBOOL iEnable;
  MUINT32 leFirst;
  MBOOL zEnable;
  MUINT32 zPattern;
};

struct RMM_Config_Param {};

struct LCSO_Param {
  MSize size;
  MINT format;
  size_t stride;
  MUINT32 bitDepth;

  LCSO_Param() : size(MSize(0, 0)), format(0), stride(0), bitDepth(0) {}
};

struct NR3D_Config_Param {
  MBOOL enable;
  MRect onRegion;      // region modified by GMV
  MRect fullImg;       // image full size for demo mode calculation
  MUINT32 vipiOffst;   // in byte
  MSize vipiReadSize;  // image size for vipi in pixel

  NR3D_Config_Param()
      : enable(MFALSE),
        onRegion(MRect(0, 0)),
        fullImg(MRect(0, 0)),
        vipiOffst(0),
        vipiReadSize(MSize(0, 0)) {}
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**
 * @brief Interface of IspMgr Interface
 */
class IIspMgr {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  //          Ctor/Dtor.
  virtual ~IIspMgr() {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  //
  /**
   * @brief get instance of IIspMgr
   */
  static IIspMgr* getInstance();
  /**
   * @brief enable/disable PDC
   */
  virtual MVOID setPDCEnable(MINT32 sensorIndex, MBOOL enable) = 0;
  /**
   * @brief enable/disable PDCout
   */
  virtual MVOID setPDCoutEnable(MINT32 sensorIndex, MBOOL enable) = 0;
  /**
   * @brief enable/disable RMG
   */
  virtual MVOID setRMGEnable(MINT32 sensorIndex, MBOOL enable) = 0;
  /**
   * @brief enable/disable RMM
   */
  virtual MVOID setRMMEnable(MINT32 sensorIndex, MBOOL enable) = 0;
  /**
   * @brief enable/disable RMG debug
   */
  virtual MVOID setRMGDebug(MINT32 sensorIndex, MINT32 debugDump) = 0;
  /**
   * @brief enable/disable RMM debug
   */
  virtual MVOID setRMMDebug(MINT32 sensorIndex, MINT32 debugDump) = 0;
  /**
   * @brief enable/disable CPN debug
   */
  virtual MVOID setCPNDebug(MINT32 sensorIndex, MINT32 debugDump) {
    (void)sensorIndex;
    (void)debugDump;
  }
  /**
   * @brief enable/disable DCPN debug
   */
  virtual MVOID setDCPNDebug(MINT32 sensorIndex, MINT32 debugDump) {
    (void)sensorIndex;
    (void)debugDump;
  }
  /**
   * @brief config RMG,RMG2 initialize Parameter
   */
  virtual MVOID configRMG_RMG2(MINT32 sensorIndex, RMG_Config_Param* param) = 0;
  /**
   * @brief config RMM.RMM2 initialize Parameter
   */
  virtual MVOID configRMM_RMM2(MINT32 sensorIndex,
                               RMM_Config_Param const& param) = 0;
  /**
   * @brief config CPN.CPN2 initialize Parameter
   */
  virtual MVOID configCPN_CPN2(MINT32 sensorIndex, MBOOL zEnable) {
    (void)sensorIndex;
    (void)zEnable;
  }
  /**
   * @brief config DCPN.DCPN2 initialize Parameter
   */
  virtual MVOID configDCPN_DCPN2(MINT32 sensorIndex, MBOOL zEnable) {
    (void)sensorIndex;
    (void)zEnable;
  }
  /**
   * @brief query LCSO parameters, such as size, format, stride...
   */
  virtual MVOID queryLCSOParams(LCSO_Param* param) = 0;

  /**
   * @brief set NR3D parameter and modify data in pTuning buffer
   */
  virtual MVOID postProcessNR3D(MINT32 sensorIndex,
                                NR3D_Config_Param* param,
                                void* pTuning) = 0;

  /**
   * @brief get iso-corresponding ABF tuning data from NVRAM
   */
  virtual MVOID* getAbfTuningData(MINT32 const sensorIndex, int iso) = 0;
};

};  // namespace NS3Av3

/**
 * @brief The definition of the maker of IIspMgr instance.
 */
#if (MTKCAM_ENABLE_IPC == 1)

namespace NS3Av3 {
class IIspMgrIPC {
 protected:
  virtual ~IIspMgrIPC() {}

 public:
  virtual MVOID queryLCSOParams(LCSO_Param* param) = 0;
  virtual MVOID postProcessNR3D(MINT32 sensorIndex,
                                NR3D_Config_Param* param,
                                void* pTuning) = 0;
  virtual void uninit(const char* strUser) = 0;
};
}  // namespace NS3Av3

typedef NS3Av3::IIspMgrIPC* (*IspMgrIPCClient_FACTORY_T)(const char* strUser);
#define MAKE_IspMgr(...)                               \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_ISP_MGR_IPC, \
                     IspMgrIPCClient_FACTORY_T, __VA_ARGS__)

#else

typedef NS3Av3::IIspMgr* (*IspMgr_FACTORY_T)();
#define MAKE_IspMgr(...)                                             \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_ISP_MGR, IspMgr_FACTORY_T, \
                     __VA_ARGS__)

#endif

typedef NS3Av3::IIspMgr* (*IspMgr_FACTORY_T)();
#define MAKE_IspMgr_IPC(...)                                         \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_ISP_MGR, IspMgr_FACTORY_T, \
                     __VA_ARGS__)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IISPMGR_H_
