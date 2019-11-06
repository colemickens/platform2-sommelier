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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCHAL3A_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCHAL3A_H_

#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/aaa/IHal3ACb.h>
#include <mtkcam/v4l2/ipc_hw_event.h>

#include "mtkcam/drv/IHalSensor.h"
#include <mtkcam/v4l2/IPCIHalSensor.h>
#include <libcamera_feature/libfdft_lib/include/faces.h>

#define MAX_APP_META_SIZE 8192
#define MAX_SET_HAL_META_SIZE 51200
#define MAX_CONFIG_HAL_META_SIZE 51200
#define MAX_SETISP_HAL_META_SIZE 151200
#define MAX_CB_HAL_META_SIZE 8192
#define MAX_GET_HAL_META_SIZE 151200
#define MAX_SHADING_SIZE 25600

struct hal3a_metaset_params {
  MINT32 MagicNum;
  MUINT8 Dummy;
  MINT32 PreSetKey;
  char appMetaBuffer[MAX_APP_META_SIZE];
  char halMetaBuffer[MAX_SET_HAL_META_SIZE];
};

struct hal3a_common_params {
  MINT32 m_i4SensorIdx;
  MINT32 m_i4SensorDev;
  MINT32 bufferHandle;
};

struct hal3a_init_params {
  hal3a_common_params common;
  NSCam::SensorStaticInfo sensorStaticInfo;
};

struct hal3a_config_params {
  hal3a_common_params common;
  NS3Av3::ConfigInfo_T rConfigInfo;
  char CfgHalMeta[MAX_CONFIG_HAL_META_SIZE];
  char CfgAppMeta[MAX_APP_META_SIZE];
};

struct hal3a_start_params {
  hal3a_common_params common;
  MINT32 i4StartNum;
};

struct hal3a_stop_params {
  hal3a_common_params common;
};

struct hal3a_stop_stt_params {
  hal3a_common_params common;
};

struct hal3a_set_params {
  hal3a_common_params common;
  hal3a_metaset_params requestQ;
};

struct hal3a_isp_buf_info {
  MINT imgFormat;
  MUINT32 width;
  MUINT32 height;
  MUINT32 planeCount;
  MUINT32 bufStrides[3];
  MUINT32 bufScanlines[3];
  MUINTPTR bufVA[3];
  MUINTPTR bufPA[3];
  MINT32 fd[3];
  MINT32 imgBits;
  MUINT32 bufStridesPixel[3];
  MUINT32 bufSize[3];
};

struct hal3a_setisp_params {
  hal3a_common_params common;
  hal3a_isp_buf_info lsc2BufInfo;
  hal3a_isp_buf_info lceBufInfo;
  MINT32 flowType;
  NS3Av3::MetaSet_T control;
  NS3Av3::TuningParam tuningBuf;
  NS3Av3::MetaSet_T metaSetResult;
  MUINT32 u4Lsc2Enable;
  MUINT32 u4LceEnable;
  char inAppMetaBuffer[MAX_APP_META_SIZE];
  char inHalMetaBuffer[MAX_SETISP_HAL_META_SIZE];
  char outAppMetaBuffer[MAX_APP_META_SIZE];
  char outHalMetaBuffer[MAX_SETISP_HAL_META_SIZE];
  MINT32 p2tuningbuf_handle;
  /* VA should be filled by IPC server via search shared memory map table. IPC
   * client is forbidden to fill it */
  MUINTPTR p2tuningbuf_va;

  char pLsc2BufCont[MAX_SHADING_SIZE];
};

struct hal3a_start_requestq_params {
  hal3a_common_params common;
  hal3a_metaset_params requestQ;
};

struct hal3a_start_capture_params {
  hal3a_common_params common;
  hal3a_metaset_params requestQ;
};

struct hal3a_preset_params {
  hal3a_common_params common;
  hal3a_metaset_params requestQ;
};

struct IpcSensorStaticInfo_T {
  MUINT32 idx;
  MUINT32 type;
  MUINT32 deviceId;
  NSCam::SensorStaticInfo sensorStaticInfo;
};

struct hal3a_send3actrl_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  union {
    NS3Av3::AEInitExpoSetting_T initExpoSetting;
    NS3Av3::IpcMetaStaticInfo_T ipcMetaStaticInfo;
    NSCam::SensorDynamicInfo sensorDynamicInfo;
    NSCam::IIPCHalSensor::DynamicInfo sensorDynamicInfoExt;
    IpcSensorStaticInfo_T ipcSensorStatic;
    int enabled;
    MUINT32 scenario;
    MINT32 pixelClokcFreq;
    MUINT32 frameSyncPixelLineNum;
    NSCam::SensorVCInfo sensorVCInfo;
    MUINT32 tline;
    MINT32 verticalBlanking;
    MINT32 AeStable;
  } arg1;

  union {
    NSCam::SensorCropWinInfo sensorCropWinInfo;
    SET_PD_BLOCK_INFO_T sensorPdafInfo;
    MBOOL sensorPdafCapacity;
    MUINT32 scenario;
    MUINT32 defaultFrameRate;
    MUINT32 vsize;
    int enabled;
  } arg2;
};

struct hal3a_getsensorparam_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  union {
    NS3Av3::IPC_SensorParam_T ipcSensorParam;
    int enabled;
  } arg1;

  union {
    uint32_t timeoutMs;
  } arg2;
};

struct hal3a_notifycallback_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  uint32_t pu4CapType;
  uint32_t pmagicnum;
  MINT32 pRvNumberSet;
  MBOOL pRfgKeep;
  MBOOL pRfgDisableP1;
  MUINT32 pCu4CapType;
  MINT64 pCi8ExposureTime;
  int callback_ret;
  char pCmetadata[MAX_CB_HAL_META_SIZE];
  union {
    int enabled;
  } arg1;

  union {
    NS3Av3::IPC_P1NotifyCb_T ipcP1NofiyCb;
  } arg2;
};

struct hal3a_tuningpipe_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  MINT32 p1tuningbuf_handle;
  /* VA should be filled by IPC server via search shared memory map table. IPC
   * client is forbidden to fill it */
  MUINTPTR p1tuningbuf_va;
  int flag;
  union {
    int cmd;
  } arg1;

  union {
    NS3Av3::IPC_IspTuningMgr_T ipcIspTuningMgr;
  } arg2;
};

struct hal3a_sttpipe_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  union {
    NS3Av3::IPC_Metabuf1_T ipcMetaBuf;
  } arg1;

  union {
    int enabled;
  } arg2;
};

struct hal3a_stt2pipe_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  union {
    NS3Av3::IPC_Metabuf2_T ipcMetaBuf2;
  } arg1;

  union {
    int enabled;
  } arg2;
};

struct hal3a_hwevent_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  union {
    v4l2::P1Event evt;
  } arg1;

  union {
    int enabled;
  } arg2;
};

struct hal3a_plinelimit_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  NS3Av3::AE_Pline_Limitation_T ipcLimitParams;
};

struct hal3a_lensconfig_params {
  hal3a_common_params common;
  NS3Av3::E3ACtrl_T e3ACtrl;
  NS3Av3::IPC_LensConfig_T lensConfig;
};

struct hal3a_notify_p1_pwr_on_params {
  hal3a_common_params common;
};

struct hal3a_notify_p1_pwr_off_params {
  hal3a_common_params common;
};

struct hal3a_notify_p1_pwr_done_params {
  hal3a_common_params common;
  MUINT32 u4MagicNum;
};

struct hal3a_set_sensor_mode_params {
  hal3a_common_params common;
  MINT32 i4SensorMode;
};

struct hal3a_callback_params {
  MINTPTR _ext1;
  MINTPTR _ext2;
  MINTPTR _ext3;
};

struct hal3a_attach_cb_params {
  hal3a_common_params common;
  NS3Av3::IHal3ACb::ECb_T eId;
  hal3a_callback_params cb_result[NS3Av3::IHal3ACb::eID_MSGTYPE_NUM];
};

struct hal3a_detach_cb_params {
  hal3a_common_params common;
  NS3Av3::IHal3ACb::ECb_T eId;
  NS3Av3::IHal3ACb* pCb;
};

struct hal3a_get_params {
  hal3a_common_params common;
  MUINT32 frmId;
  NS3Av3::MetaSet_T result;
  int get_ret;
  char appMetaBuffer[MAX_APP_META_SIZE];
  char halMetaBuffer[MAX_GET_HAL_META_SIZE];
};

struct hal3a_get_cur_params {
  hal3a_common_params common;
  MUINT32 frmId;
  NS3Av3::MetaSet_T result;
  int get_cur_ret;
  char appMetaBuffer[MAX_APP_META_SIZE];
  char halMetaBuffer[MAX_GET_HAL_META_SIZE];
};

struct hal3a_set_fdInfo_params {
  hal3a_common_params common;
  MtkCameraFaceMetadata detectFace;
  MtkCameraFace faceDetectInfo[15];
  MtkFaceInfo facePoseInfo[15];
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCHAL3A_H_
