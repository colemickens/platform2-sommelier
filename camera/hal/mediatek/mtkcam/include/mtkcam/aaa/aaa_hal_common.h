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

/**
 * @file aaa_hal_common.h
 * @brief Declarations of Abstraction of 3A Hal Class and Top Data Structures
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_AAA_HAL_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_AAA_HAL_COMMON_H_

#include <utility>
#include <vector>

#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/hw/HwTransform.h>

namespace NS3Av3 {
struct TuningParam {
  /* Output Param.*/
  void* pRegBuf;
  /* Output Param.*/
  void* pMfbBuf;
  /* Output Param.*/
  void* pLsc2Buf;
  /* Input Param. Pass2 Node need to send LCSO buffer to ISP tuning after
   * LCE3.0. */
  void* pLcsBuf;
  /* Output Param.*/
  void* pBpc2Buf;
  /* fd of pRegBuf for IPC buffer mmap usage */
  int RegBuf_fd;
  /* constructor */
  TuningParam(void* pRegBuf = nullptr,
              void* pMfbBuf = nullptr,
              void* pLsc2Buf = nullptr,
              void* pLcsBuf = nullptr,
              void* pBpc2Buf = nullptr)
      : pRegBuf(pRegBuf),
        pMfbBuf(pMfbBuf),
        pLsc2Buf(pLsc2Buf),
        pLcsBuf(pLcsBuf),
        pBpc2Buf(pBpc2Buf),
        RegBuf_fd(-1) {}
};

struct FrameOutputParam_T {
  MUINT32 u4AEIndex;
  MUINT32 u4AEIndexF;
  MUINT32 u4FinerEVIdxBase;
  MUINT32 u4FRameRate_x10;            // 10 base frame rate
  MUINT32 u4PreviewShutterSpeed_us;   // micro second
  MUINT32 u4PreviewSensorGain_x1024;  // 1024 base
  MUINT32 u4PreviewISPGain_x1024;     // 1024 base
  MUINT32 u4RealISOValue;
  MUINT32 u4CapShutterSpeed_us;   // micro second
  MUINT32 u4CapSensorGain_x1024;  // 1024 base
  MUINT32 u4CapISPGain_x1024;     // 1024 base
  MINT32 i4BrightValue_x10;       // 10 base brightness value
  MINT32 i4ExposureValue_x10;     // 10 base exposure value
  MINT32 i4LightValue_x10;        // 10 base lumince value
  MINT16 i2FlareOffset;           // 12 bit domain
  MINT16 i2FlareGain;             // 9 base gain
};

/**
 * @brief 3A parameters for capture
 */
struct CaptureParam_T {
  MUINT32 u4ExposureMode;  // 0: exp. time, 1: exp. line
  MUINT32 u4Eposuretime;   //!<: Exposure time in us
  MUINT32 u4AfeGain;       //!<: sensor gain
  MUINT32 u4IspGain;       //!<: raw gain
  MUINT32 u4RealISO;       //!<: Real ISO speed
  MUINT32 u4FlareOffset;
  MUINT32 u4FlareGain;      // 512 is 1x
  MINT32 i4LightValue_x10;  // 10 base LV value
  MINT32 i4YuvEvIdx;
  CaptureParam_T()
      : u4ExposureMode(0),
        u4Eposuretime(0),
        u4AfeGain(0),
        u4IspGain(0),
        u4RealISO(0),
        u4FlareOffset(0),
        u4FlareGain(0),
        i4LightValue_x10(0),
        i4YuvEvIdx(0) {}
};

struct RequestSet_T {
  RequestSet_T() : fgKeep(0), fgDisableP1(0) {}
  std::vector<MINT32> vNumberSet;
  MBOOL fgKeep;
  MBOOL fgDisableP1;
};

struct FeatureParam_T {
  MBOOL bExposureLockSupported;
  MBOOL bAutoWhiteBalanceLockSupported;
  MUINT32 u4MaxFocusAreaNum;
  MUINT32 u4MaxMeterAreaNum;
  MBOOL bEnableDynamicFrameRate;
  MINT32 i4MaxLensPos;
  MINT32 i4MinLensPos;
  MINT32 i4AFBestPos;
  MUINT32 u4FocusLength_100x;
  MINT32 u4PureRawInterval;
};

struct ExpSettingParam_T {
  MINT32 u4AOEMode;
  MUINT32 u4MaxSensorAnalogGain;  // 1x=1024
  MUINT32 u4MaxAEExpTimeInUS;     // unit: us
  MUINT32 u4MinAEExpTimeInUS;     // unit: us
  MUINT32 u4ShutterLineTime;      // unit: 1/1000 us
  MUINT32 u4MaxAESensorGain;      // 1x=1024
  MUINT32 u4MinAESensorGain;      // 1x=1024
  MUINT32 u4ExpTimeInUS0EV;       // unit: us
  MUINT32 u4SensorGain0EV;        // 1x=1024
  MUINT32 u4ISOValue;             // real ISO value
  MUINT8 u1FlareOffset0EV;
  MINT32 i4GainBase0EV;  // AOE application for LE calculation
  MINT32 i4LE_LowAvg;    // AOE application for LE calculation, def: 0 ~ 39 avg
  MINT32 i4SEDeltaEVx100;  // AOE application for SE calculation
  MBOOL bDetectFace;       // detect face or not
  MUINT32 u4Histogram[128];
  MUINT32 u4FlareHistogram[128];
  MVOID* pPLineAETable;
  MINT32 i4aeTableCurrentIndex;
  MUINT32 u4LE_SERatio_x100;  // vhdr ratio (x100)
  MUINT32 u4ExpRatio;
};

// 3A ASD info
struct ASDInfo_T {
  MINT32 i4AELv_x10;         // AE Lv
  MBOOL bAEBacklit;          // AE backlit condition
  MBOOL bAEStable;           // AE stable
  MINT16 i2AEFaceDiffIndex;  // Face AE difference index with central weighting
  MINT32 i4AWBRgain_X128;    // AWB Rgain
  MINT32 i4AWBBgain_X128;    // AWB Bgain
  MINT32 i4AWBRgain_D65_X128;  // AWB Rgain (D65; golden sample)
  MINT32 i4AWBBgain_D65_X128;  // AWB Bgain (D65; golden sample)
  MINT32 i4AWBRgain_CWF_X128;  // AWB Rgain (CWF; golden sample)
  MINT32 i4AWBBgain_CWF_X128;  // AWB Bgain (CWF; golden sample)
  MBOOL bAWBStable;            // AWB stable
  MINT32 i4AFPos;              // AF position
  MVOID* pAFTable;             // Pointer to AF table
  MINT32 i4AFTableOffset;      // AF table offset
  MINT32 i4AFTableMacroIdx;    // AF table macro index
  MINT32 i4AFTableIdxNum;      // AF table total index number
  MBOOL bAFStable;             // AF stable
};

enum E_CAPTURE_TYPE { E_CAPTURE_NORMAL = 0, E_CAPTURE_HIGH_QUALITY_CAPTURE };

struct CapParam_T {
  MUINT32 u4CapType;
  MINT64 i8ExposureTime;
  NSCam::IMetadata metadata;
  CapParam_T() : u4CapType(E_CAPTURE_NORMAL), i8ExposureTime(0) {}
};

struct AE_Pline_Limitation_T {
  MBOOL bEnable;
  MBOOL bEquivalent;
  MUINT32 u4IncreaseISO_x100;
  MUINT32 u4IncreaseShutter_x100;
};

struct AE_EXP_SETTING_T {
  MINT32 u4ExposureTime;  // naro sec
  MINT32 u4Sensitivity;   // ISO value
};

/* frame information */
typedef struct AF_FRAME_INFO_t {
  MINT64 i8FocusValue;  // focusValue
  MINT32 i4LensPos;     // lens position
  MINT32 GyroValue[3];  // gyro value
  MINT32 AFROI[5];      // X,Y,W,H,Type
} AF_FRAME_INFO_T;

enum E_DAF_MODE {
  E_DAF_OFF = 0,
  E_DAF_RUN_STEREO = 1,      /* run stereo hal */
  E_DAF_RUN_DEPTH_ENGINE = 2 /* run depth engine */
};

// max frames to queue DAF information
#define DAF_TBL_QLEN 32
#define DIST_TBL_QLEN 16
typedef struct DAF_VEC_STRUCT_t {
  MUINT32 frm_mun;
  MINT32 af_mode;
  MINT32 af_roi_sel;
  MUINT8 is_learning;
  MUINT8 is_querying;
  MUINT8 af_valid;
  MUINT8 is_af_stable;
  MUINT16 af_dac_pos;
  MUINT16 af_dac_index;
  MUINT16 af_confidence;
  MUINT16 af_win_start_x;
  MUINT16 af_win_start_y;
  MUINT16 af_win_end_x;
  MUINT16 af_win_end_y;
  MUINT16 daf_dac_index;
  MUINT16 daf_confidence;
  MUINT16 daf_distance;
  MUINT16 thermal_lens_pos;
  MUINT16 posture_dac;
  MINT32 is_scene_stable;
} DAF_VEC_STRUCT;

typedef struct {
  MUINT8 is_daf_run;
  MUINT32 is_query_happen;
  MUINT32 curr_p1_frm_num;
  MUINT32 curr_p2_frm_num;
  MUINT16 af_dac_min;
  MUINT16 af_dac_max;
  MUINT16 af_dac_inf;
  MUINT16 af_dac_marco;
  MUINT16 af_distance_inf;
  MUINT16 af_distance_marco;
  MUINT16 af_dac_start;
  MUINT32 dac[DIST_TBL_QLEN];
  MUINT32 dist[DIST_TBL_QLEN];

  DAF_VEC_STRUCT daf_vec[DAF_TBL_QLEN];
} DAF_TBL_STRUCT;

#define OIS_DATA_NUM 8
struct OISInfo_T {
  int64_t TimeStamp[OIS_DATA_NUM];
  int i4OISHallPosX[OIS_DATA_NUM];
  int i4OISHallPosY[OIS_DATA_NUM];
};

enum ESTART_CAP_TYPE_T {
  ESTART_CAP_NORMAL = 0,
  ESTART_CAP_MANUAL,
  ESTART_CAP_SPECIAL
};

struct S3ACtrl_GetIspGamma {
  MUINT32* gamma_lut;      // in: pointer to a user-allocating buffer
  MUINT32 gamma_lut_size;  // out
  MBOOL enabled;           // out
};

enum E3ACtrl_T {
  E3ACtrl_Begin = 0,
  // shading
  E3ACtrl_SetShadingSdblkCfg = 0x0001,
  E3ACtrl_SetShadingEngMode,
  E3ACtrl_SetShadingByp123,
  E3ACtrl_SetShadingOnOff,
  E3ACtrl_SetShadingTSFOnOff,
  E3ACtrl_SetShadingDynamic,
  E3ACtrl_SetShadingColorTemp,
  //    E3ACtrl_SetShadingStrength,

  // AWB
  E3ACtrl_SetAwbBypCalibration = 0x0100,

  // AE set
  //    E3ACtrl_SetExposureParam        = 0x0200,
  E3ACtrl_SetHalHdr = 0x0200,
  E3ACtrl_SetAETargetMode,
  E3ACtrl_SetAEIsoSpeedMode,
  E3ACtrl_SetAELimiterMode,
  E3ACtrl_SetAECamMode,
  E3ACtrl_SetAEEISRecording,
  E3ACtrl_SetAEPlineLimitation,
  E3ACtrl_EnableDisableAE,
  E3ACtrl_SetAEVHDRratio,
  E3ACtrl_EnableAIS,
  E3ACtrl_EnableFlareInManualCtrl,
  E3ACtrl_SetMinMaxFps,
  E3ACtrl_SetCCUCB,
  E3ACtrl_EnableTgInt,
  E3ACtrl_EnableBMDN,
  E3ACtrl_EnableMFHR,
  E3ACtrl_GetIsStrobeBVTrigger,
  E3ACtrl_SetStereoDualAELock,
  E3ACtrl_SetStereoAFLock,
  E3ACtrl_SetStereoAfStop,
  E3ACtrl_ResetMvhdrRatio,

  // AE get
  E3ACtrl_GetCurrentEV = 0x0280,
  E3ACtrl_GetBVOffset,
  E3ACtrl_GetNVRAMParam,
  E3ACtrl_GetAEPLineTable,
  E3ACtrl_GetExposureInfo,
  E3ACtrl_GetExposureParam,
  E3ACtrl_GetInitExposureTime,
  E3ACtrl_GetAECapPLineTable,
  E3ACtrl_GetIsAEStable,
  E3ACtrl_GetRTParamsInfo,
  E3ACtrl_GetEvCapture,
  E3ACtrl_GetEvSetting,
  E3ACtrl_GetCaptureDelayFrame,
  E3ACtrl_GetSensorSyncInfo,
  E3ACtrl_GetSensorPreviewDelay,
  E3ACtrl_GetSensorDelayInfo,
  E3ACtrl_GetIsoSpeed,
  E3ACtrl_GetAEStereoDenoiseInfo,
  E3ACtrl_GetAEInitExpoSetting,
  E3ACtrl_GetAF2AEInfo,
  E3ACtrl_GetPrioritySetting,
  E3ACtrl_GetIsAEPlineIndexMaxStable,  // to be removed
  E3ACtrl_GetISOThresStatus,
  E3ACtrl_ResetGetISOThresStatus,
  E3ACtrl_EnableAEStereoManualPline,
  // ISP
  //    E3ACtrl_ConvertToIspGamma
  E3ACtrl_GetIspGamma = 0x0300,
  E3ACtrl_GetRwbInfo = 0x0301,
  E3ACtrl_SetLcsoParam = 0x0302,
  E3ACtrl_ValidatePass1 = 0x0303,
  E3ACtrl_SetIspProfile,
  E3ACtrl_GetOBOffset,

  // Flash
  E3ACtrl_GetQuickCalibration = 0x0400,
  E3ACtrl_EnableFlashQuickCalibration,
  E3ACtrl_SetIsFlashOnCapture,
  E3ACtrl_GetIsFlashOnCapture,
  E3ACtrl_ChkMainFlashOnCond4StartCapture,
  E3ACtrl_ChkMFNRFlash,

  // 3A misc set
  // 3A misc get
  E3ACtrl_GetAsdInfo = 0x0480,
  E3ACtrl_GetExifInfo,
  E3ACtrl_GetSupportedInfo,
  E3ACtrl_GetDualZoomInfo,
  E3ACtrl_GetCCUFrameSyncInfo,

  // AF set
  E3ACtrl_SetAFMode = 0x0500,
  E3ACtrl_SetAFMFPos,
  E3ACtrl_SetAFFullScanStep,
  E3ACtrl_SetAFCamMode,
  E3ACtrl_SetEnableOIS,
  E3ACtrl_SetEnablePBin,
  E3ACtrl_SetPureRawData,
  E3ACtrl_SetAF2AEInfo,

  // AF get
  E3ACtrl_GetAFDAFTable = 0x0600,
  E3ACtrl_GetOISPos,

  // Flow control set
  E3ACtrl_Enable3ASetParams = 0x1000,
  E3ACtrl_SetOperMode = 0x1001,
  E3ACtrl_SetStereoParams = 0x1002,
  E3ACtrl_EnableGyroSensor = 0x1003,
  E3ACtrl_SetIsZsdCapture = 0x1004,
  E3ACtrl_SwitchTo2D = 0x1005,
  E3ACtrl_SwitchTo3D = 0x1006,
  // Flow control get
  E3ACtrl_GetOperMode = 0x2001,
  // online-tuning get size of mfb
  E3ACtrl_GetMfbSize = 0x2002,
  E3ACtrl_SetCaptureMaxFPS,

  // IPC usage, started from 0x3000
  // {{{
  E3ACtrl_IPC_Start = 0x3000,

  // Sensor drive configurations that was acquired from V4L2SensorMgr, IHal3A
  // would enqueue sensor parameters.
  //  @arg1: pointer of IPC_SensorParam_T
  //  @arg2: acquire time out in MS.
  E3ACtrl_IPC_AE_GetSensorParam,

  // To disable/enable (invalidate/validate) IPC queue in IHal3A to stop
  // sending sensor parameters.
  //  @arg1: 0 to disable, 1 to enable.
  //  @arg2: this field is NULL.
  E3ACtrl_IPC_AE_GetSensorParamEnable,

  // To exchange lens configuration between IHal3A and middleware.
  //  @arg1: pointer of IPC_LensConfig_T.
  //  @arg2: don't care.
  E3ACtrl_IPC_AF_ExchangeLensConfig,

  // P1 metadata result from IHal3A. V4L2P13ACallback would acquires this data
  // from IHal3A's IPC queue.
  //  @arg1: 0, ask for IPC_P1NotifyCb_T data from IHal3A. 1: represents an ACK.
  //  @arg2: If arg1 is 0, this field is an address of IPC_P1NotifyCb_T.
  //         If arg1 os 1, this field is NULL.
  E3ACtrl_IPC_P1_NotifyCb,

  // To disable/enable IPC queue in IHal3A to stop sending P1 results.
  //  @arg1: 0 to disable, otherwise enable.
  //  @arg2: dont' care.
  E3ACtrl_IPC_P1_NotifyCbEnable,

  // HW singal, send from V4L2HwEventWorker to IHal3A.
  //  @arg1: an address of struct v4l2::P1Event.
  //  @arg2: this field is NULL.
  E3ACtrl_IPC_P1_HwSignal,

  // V4L2TuningPipe waits requests from IHal3A's IPC queue.
  //
  //  @arg1: int of IPC_IspTuningMgr_T::cmdXXXXXXX to represents the requests.
  //  @arg2: If arg1 == cmdWAIT_REQUEST, this field is an address of an
  //  IPC_IspTuningMgr_T
  //                                     which contains DMA buffer info (dir:
  //                                     open source -> IHal3A).
  //         If arg1 == cmdTERMINATED, this field is NULL.
  //         If arg1 == cmdREVIVE, this field is NULL.
  E3ACtrl_IPC_P1_WaitTuningReq,

  // V4L2TuningPipe exchanging tuning buffer with IHal3A.
  //  @arg1: int of IPC_IspTuningMgr_T::cmdXXXXXXX to represents the requests.
  //  @arg2: if arg1 == cmdACQUIRE_FROM_FMK, this arg is an address of an
  //  IPC_IspTuningMgr_T
  //                                         contains DMA buffer info. (dir:
  //                                         open source -> IHal3A).
  //         if arg1 == cmdRESULT_FROM_FMK, this arg is an address of an
  //         IPC_IspTuningMgr_T
  //                                        contains DMA buffer info. (dir:
  //                                        IHal3A -> open source).
  //
  E3ACtrl_IPC_P1_ExchangeTuningBuf,
  E3ACtrl_IPC_P1_SttControl,
  E3ACtrl_IPC_P1_Stt2Control,
  E3ACtrl_IPC_Set_MetaStaticInfo,
  /* Sensor Inform */
  E3ACtrl_IPC_SetStaticInfo,
  E3ACtrl_IPC_SetDynamicInfo,
  E3ACtrl_IPC_SetDynamicInfoEx,
  E3ACtrl_IPC_CropWin,
  E3ACtrl_IPC_PixelClock,
  E3ACtrl_IPC_PixelLine,
  E3ACtrl_IPC_PdafInfo,
  E3ACtrl_IPC_PdafCapacity,
  E3ACtrl_IPC_SensorVCInfo,
  E3ACtrl_IPC_DefFrameRate,
  E3ACtrl_IPC_RollingShutter,
  E3ACtrl_IPC_VerticalBlanking,
  /* acceleration sensor */
  E3ACtrl_IPC_SetPeriSensorData,
  // }}}
  E3ACtrl_IPC_End,
  //
  E3ACtrl_Num
};

enum EBitMode_T {
  EBitMode_10Bit = 0,
  EBitMode_12Bit,
  EBitMode_14Bit,
  EBitMode_16Bit
};

enum EHlrOption_T { EHlrOption_Auto = 0, EHlrOption_ForceOff };

enum EAFCtrl_T {
  EAFCtrl_GetPostureDAC = 0,
  EAFCtrl_GetCalibratedDistance = 1,
  EAFCtrl_Num
};

struct ConfigInfo_T {
  MINT32 i4SubsampleCount;
  EBitMode_T i4BitMode;
  EHlrOption_T i4HlrOption;
  NSCam::IMetadata CfgHalMeta;
  NSCam::IMetadata CfgAppMeta;
  NSCamHW::HwMatrix matFromAct;
  NSCamHW::HwMatrix matToAct;

  ConfigInfo_T()
      : i4SubsampleCount(1),
        i4BitMode(EBitMode_12Bit),
        i4HlrOption(EHlrOption_ForceOff),
        CfgHalMeta(),
        CfgAppMeta(),
        matFromAct(),
        matToAct() {}

  ~ConfigInfo_T() {}
};

// AE init exposure setting for camera launch
struct AEInitExpoSetting_T {
  MUINT32 u4SensorMode;    // input: sensor mode
  MUINT32 u4AETargetMode;  // input: AE target mode
  MUINT32 u4Eposuretime;   // output: AE sensor shutter (if HDR: long exposure)
  MUINT32 u4AfeGain;       // output: AE sensor gain (if HDR: long exposure)
  MUINT32 u4Eposuretime_se;   // output: AE short sensor shutter for HDR
  MUINT32 u4AfeGain_se;       // output: AE short sensor gain for HDR
  MUINT32 u4Eposuretime_me;   // output: AE middle sensor shutter for HDR
  MUINT32 u4AfeGain_me;       // output: AE middle sensor gain for HDR
  MUINT32 u4Eposuretime_vse;  // output: AE very short sensor shutter for HDR
  MUINT32 u4AfeGain_vse;      // output: AE very short sensor gain for HDR
};

struct shadingConfig_T {
  MUINT32 AAOstrideSize;
  MUINT32 AAOBlockW;
  MUINT32 AAOBlockH;
};

// -----------------------
// IPC usage
// -----------------------
//
// E3ACtrl_IPC_P1_NotifyCb
// {{{
struct IPC_P1NotifyCb_Proc_Finish_T {
  uint32_t magicnum;
  RequestSet_T* pRequestResult;  // <-- notice: this attribute is complicated to
                                 // flatten/unflatten.
  CapParam_T* pCapParam;         // <-- notice: this attribute is complicated to
                                 // flatten/unflatten.
};

struct IPC_P1NotifyCb_Vsync_Done_T {
  // nothing at all
};

struct IPC_P1NotifyCb_T {
  enum {
    WAIT_3A_PROC_FINISHED = 0,
    ACK = 1,
  };

  uint32_t u4CapType;  // represents IHal3ACb::ECb_T
  union {
    IPC_P1NotifyCb_Proc_Finish_T proc_finish;
    IPC_P1NotifyCb_Vsync_Done_T vsync_done;
  } u;

  IPC_P1NotifyCb_T() {
    memset(reinterpret_cast<void*>(this), 0x0, sizeof(IPC_P1NotifyCb_T));
  }

  IPC_P1NotifyCb_T(const IPC_P1NotifyCb_T& other) { *this = other; }

  IPC_P1NotifyCb_T(IPC_P1NotifyCb_T&& other) { *this = std::move(other); }

  IPC_P1NotifyCb_T& operator=(const IPC_P1NotifyCb_T& other) {
    memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(&other),
           sizeof(IPC_P1NotifyCb_T));
    return *this;
  }

  IPC_P1NotifyCb_T& operator=(IPC_P1NotifyCb_T&& other) {
    memcpy(reinterpret_cast<void*>(this), reinterpret_cast<const void*>(&other),
           sizeof(IPC_P1NotifyCb_T));
    return *this;
  }
};
// }}}

/* AE sensor setting */
struct IPC_SensorParam_T {
  // command
  int32_t cmd;

  // sensor idx;
  int32_t sensorIdx;

  // sensor device
  int32_t sensorDev;

  // parameter data
  union {
    uint32_t a_u4ExpTime;
    uint32_t a_u4SensorFrameRate;
    uint32_t a_u4ExpLine;
    uint32_t a_u4SensorGain;
    uint32_t a_u4FlickerInfo;
    uint32_t a_bEnableBufferMode;
    uint32_t a_bLockSensorOB;
    uint32_t a_sensorMode;
    //
    uint32_t field;
  } p1;

  union {
    uint32_t a_frameRate;
    //
    uint32_t field;
  } p2;

  union {
    //
    uint32_t field;
  } p3;
};

/*
 * AF setting.
 *  @enum UNDEFINED: this command is undefined.
 *  @enum ASK_FOR_A_CMD: caller ask for a command from 3A/ISP framework
 *  @enum ASK_TO_START: start communication
 *  @enum ASK_TO_STOP: stop communication
 *  @enum CMD_FOCUS_ABSOULTE: 3A/ISP framework ask for a lens position to
 * worker.
 *  @enum CMD_IS_SUPPORT_LENS: 3A/ISP framework ask for the result of lens
 * support.
 *  @enum ACK_IS_SUPPORT_LENS: worker responses if supported lens driver to
 * 3A/ISP framework.
 *  @param cmd: a command to worker and 3A/ISP framework.
 *  @param result: 1 to represent 3A/ISP has handled this command, 0 for not.
 *  @param val: union values, see belows:
 *  @param focus_pos: a value related to CMD_FOCUS_ABSOULTE.
 *  @param is_support: a value related to ACK_IS_SUPPORT_LENS.
 */
struct IPC_LensConfig_T {
  enum {
    UNDEFINED = 0,
    /* caller ask a command from 3A/ISP framework */
    ASK_FOR_A_CMD,
    ASK_TO_START,
    ASK_TO_STOP,

    /* commands */
    CMD_FOCUS_ABSOULTE,
    CMD_IS_SUPPORT_LENS,

    /* acknowledgements */
    ACK_IS_SUPPORT_LENS,
  };

  // command
  int32_t cmd;

  // result
  int32_t succeeded;

  // value
  union {
    int64_t focus_pos;  // CMD_FOCUS_ABSOULTE
    int is_support;     // ACK_IS_SUPPORT_LENS
  } val;
};

/* ISP */
struct IPC_IspTuningMgr_T {
  uint32_t magicnum;  // magic number
  uint32_t response;  // the responsed action from IHal3A
  uint64_t
      bufVa;  // virtual address of mtk_p1_metabuf_tuning* for CPU read/write
  uint64_t bufFd;  // FD of mtk_p1_metabuf_tuning for MMAP,
  //
  IPC_IspTuningMgr_T()
      : magicnum(0xFFFFFFFF), response(0), bufVa(0), bufFd(0) {}

  enum {
    cmdWAIT_REQUEST = 0,
    cmdACQUIRE_FROM_FMK,
    cmdRESULT_FROM_FMK,
    cmdTERMINATED,  // kill (terminate) IPCTuningMgr
    cmdREVIVE,      // revive IPCTuningMgr
  };
};

/* STT */
struct IPC_Metabuf1_T {
  uint32_t magicnum;
  uint32_t cmd;
  uint32_t response;
  uint64_t bufVa;
  uint64_t bufFd;
  //
  IPC_Metabuf1_T()
      : magicnum(0xFFFFFFFF), cmd(0), response(0), bufVa(0), bufFd(0) {}

  enum {
    cmdNONE = 0,
    cmdENQUE_FROM_DRV,
    cmdDEQUE_FROM_3A,
    cmdKILL_IPC_SERVER,
    cmdREVIVE_IPC_SERVER,
  };

  enum {
    responseOK = 0,
    responseENQUE_FAILED,
    responseDEQUE_EMPTY,
    responseIPC_SERVER_DEAD,
    responseTIMEOUT,
  };
};

struct IPC_Metabuf2_T {
  uint32_t magicnum;
  uint32_t cmd;
  uint32_t response;
  uint64_t bufVa;
  uint64_t bufFd;
  //
  IPC_Metabuf2_T()
      : magicnum(0xFFFFFFFF), cmd(0), response(0), bufVa(0), bufFd(0) {}

  enum {
    cmdNONE = 0,
    cmdENQUE_FROM_DRV,
    cmdDEQUE_FROM_3A,
    cmdKILL_IPC_SERVER,
    cmdREVIVE_IPC_SERVER,
  };

  enum {
    responseOK = 0,
    responseENQUE_FAILED,
    responseDEQUE_EMPTY,
    responseIPC_SERVER_DEAD,
    responseTIMEOUT,
  };
};

struct IpcMetaStaticInfo_T {
  MUINT8 availableSceneModes[100];
  MUINT8 availableSceneModesCount;
  MUINT8 sceneModeOverrides[100];
  MUINT8 sceneModeOverridesCount;
  NSCam::MRational aeCompensationStep;
  MINT32 maxRegions[3];
  NSCam::MRect activeArrayRegion;
  NSCam::MSize shadingMapSize;
  MINT32 availableResultKeys[100];
  MUINT8 availableResultKeysCount;
  MINT64 rollingShutterSkew;  // Not use
  MFLOAT availableFocalLengths;
  MFLOAT availableApertures;
  IpcMetaStaticInfo_T()
      : availableSceneModesCount(0),
        sceneModeOverridesCount(0),
        availableResultKeysCount(0),
        rollingShutterSkew(0),
        availableFocalLengths(0),
        availableApertures(0) {}
  ~IpcMetaStaticInfo_T() {}
};

struct IpcPeriSensorData_T {
  float acceleration[3];
};

}  // namespace NS3Av3
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_AAA_HAL_COMMON_H_
