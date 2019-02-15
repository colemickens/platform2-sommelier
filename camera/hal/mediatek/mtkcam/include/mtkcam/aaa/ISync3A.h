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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_ISYNC3A_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_ISYNC3A_H_

#include <mtkcam/aaa/aaa_hal_common.h>
#include <mtkcam/utils/module/module.h>

namespace NS3Av3 {
/******************************************************************************
 *  ISync3A Interface.
 ******************************************************************************/
class ISync3A {
 public:
  /**
   * Explicitly init 3A N3D Sync manager by MW.
   */
  virtual MBOOL init(MINT32 i4Policy,
                     MINT32 i4Master,
                     MINT32 i4Slave,
                     const char* strName) = 0;

  /**
   * Explicitly uninit 3A N3D Sync manager by MW.
   */
  virtual MBOOL uninit() = 0;

  /**
   * Functions for 3A sync control
   */
  virtual MINT32 sync(MINT32 i4Sensor, MINT32 i4Param, MVOID* pSttBuf) = 0;

  /**
   * Functions for AF sync control
   * update lens position to Main2 lens
   */
  virtual MINT32 syncAF(MINT32 i4Sensor, MBOOL initLens = 0) = 0;
  virtual MVOID enableSync(MBOOL fgOnOff) = 0;

  virtual MBOOL isSyncEnable() const = 0;

  virtual MINT32 getFrameCount() const = 0;

  virtual MBOOL setDebugInfo(void* prDbgInfo) const = 0;

  /**
   * Functions for AE sync control
   */
  virtual MBOOL isAeStable() const = 0;

  virtual MINT32 getAeSchedule() const = 0;

  enum {
    E_SYNC3A_DO_AE = (1 << 0),
    E_SYNC3A_DO_AE_PRECAP = (1 << 1),
    E_SYNC3A_DO_AWB = (1 << 2),
    E_SYNC3A_BYP_AE = (1 << 3),
    E_SYNC3A_DO_HW_SYNC = (1 << 4),
    E_SYNC3A_DO_SW_SYNC = (1 << 5)
  };

  enum E_SYNC3A_SUPPORT {
    E_SYNC3A_SUPPORT_AE = (1 << 0),
    E_SYNC3A_SUPPORT_AWB = (1 << 1),
    E_SYNC3A_SUPPORT_AF = (1 << 2)
  };

  virtual MVOID enableSyncSupport(E_SYNC3A_SUPPORT iSupport) = 0;

  virtual MVOID disableSyncSupport(E_SYNC3A_SUPPORT iSupport) = 0;

  virtual MINT32 getSyncSupport() const = 0;

  virtual MBOOL isAFSyncFinish() = 0;

  virtual MBOOL is2ASyncFinish() = 0;

  virtual MBOOL isAESyncStable() = 0;

  virtual MVOID syncAEInit(MINT32 i4Master, MINT32 i4Slave) = 0;

  virtual MBOOL isPerframeAE() = 0;

 protected:
  virtual ~ISync3A() {}
};

/******************************************************************************
 *  ISync3A Interface.
 ******************************************************************************/
class ISync3AMgr {
 public:
  enum E_SYNC2A_MODE {
    E_SYNC2A_MODE_IDLE = 0,
    E_SYNC2A_MODE_NONE = 1,
    E_SYNC2A_MODE_VSDOF = 2,
    E_SYNC2A_MODE_DENOISE = 3,
    E_SYNC2A_MODE_DUAL_ZOOM = 4,
  };

  enum E_SYNCAF_MODE {
    E_SYNCAF_MODE_IDLE = 0,
    E_SYNCAF_MODE_ON = 1,
    E_SYNCAF_MODE_OFF = 2,
  };

  enum E_HW_FRM_SYNC_MODE {
    E_HW_FRM_SYNC_MODE_IDLE = 0,
    E_HW_FRM_SYNC_MODE_ON = 1,
    E_HW_FRM_SYNC_MODE_OFF = 2,
  };

  enum { E_SYNC3AMGR_PRVIEW = 0, E_SYNC3AMGR_CAPTURE = 1 };

  enum E_SYNC3AMGR_CAPMODE {
    E_SYNC3AMGR_CAPMODE_3D = 0,
    E_SYNC3AMGR_CAPMODE_2D = 1
  };

  enum E_SYNC3AMGR_AF_STATE {
    E_SYNC3AMGR_AF_STATE_IDLE = 0,
    E_SYNC3AMGR_AF_STATE_BEGIN = 1,
    E_SYNC3AMGR_AF_STATE_SCANNING = 2,
  };

  struct Stereo_Param_T {
    MINT32 i4Sync2AMode;
    MINT32 i4SyncAFMode;
    MINT32 i4HwSyncMode;
    MINT32 i4MasterIdx;
    MINT32 i4SlaveIdx;

    Stereo_Param_T()
        : i4Sync2AMode(0),
          i4SyncAFMode(0),
          i4HwSyncMode(0),
          i4MasterIdx(0),
          i4SlaveIdx(0) {}
  };
  /**
   * get singleton.
   */
  static ISync3AMgr* getInstance();

  virtual ISync3A* getSync3A(MINT32 i4Id = E_SYNC3AMGR_PRVIEW) const = 0;

  /**
   * Explicitly init 3A N3D Sync manager by MW.
   */
  virtual MBOOL init(MINT32 i4Policy,
                     MINT32 i4MasterIdx,
                     MINT32 i4SlaveIdx) = 0;

  /**
   * Explicitly uninit 3A N3D Sync manager by MW.
   */
  virtual MBOOL uninit() = 0;

  virtual MVOID DevCount(MBOOL bEnable, MINT32 i4SensorDev) = 0;

  virtual MBOOL isInit() const = 0;

  virtual MBOOL isActive() const = 0;

  virtual MINT32 getMasterDev() const = 0;

  virtual MINT32 getSlaveDev() const = 0;

  virtual MVOID setAFState(MINT32 i4AfState) = 0;

  virtual MINT32 getAFState() const = 0;

  virtual MVOID setStereoParams(Stereo_Param_T const& rNewParam) = 0;

  virtual Stereo_Param_T getStereoParams() const = 0;

  virtual MINT32 getFrmSyncOpt() const = 0;

  virtual MINT32 getAWBMasterDev() const = 0;

  virtual MINT32 getAWBSlaveDev() const = 0;

  // only for 97
  virtual MVOID setAFSyncMode(MINT32 i4AfSyncMode) = 0;

  virtual MVOID enable() = 0;

  virtual MVOID disable() = 0;

  virtual MVOID updateInitParams() = 0;

  virtual MVOID setManualControl(MBOOL bEnable) = 0;

  virtual MBOOL isManualControl() = 0;

 protected:
  virtual ~ISync3AMgr() {}
};

}  // namespace NS3Av3

/**
 * @brief The definition of the maker of ISync3AMgr instance.
 */
typedef NS3Av3::ISync3AMgr* (*Sync3AMgr_FACTORY_T)();
#define MAKE_Sync3AMgr(...)                                                 \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_SYNC_3A_MGR, Sync3AMgr_FACTORY_T, \
                     __VA_ARGS__)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_ISYNC3A_H_
