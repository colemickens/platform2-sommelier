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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ICAM_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ICAM_TYPE_H_

/*This namedspace for old enum use, new enum no need declare in here*/
namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

typedef enum {
  CAM_UNKNOWN = 0x00000000,
  CAM_BAYER8 = 0x00000001,
  CAM_BAYER10 = 0x00000002,
  CAM_BAYER12 = 0x00000004,
  CAM_BAYER14 = 0x00000008,
  CAM_MIPI10_BAYER8 = 0x00000010,
  CAM_MIPI10_BAYER10 = 0x00000020,
  CAM_MIPI10_BAYER12 = 0x00000040,
  CAM_MIPI10_BAYER14 = 0x00000080,
  CAM_FG_BAYER8 = 0x00000100,
  CAM_FG_BAYER10 = 0x00000200,
  CAM_FG_BAYER12 = 0x00000400,
  CAM_FG_BAYER14 = 0x00000800,
  CAM_UFEO_BAYER8 = 0x00001000,
  CAM_UFEO_BAYER10 = 0x00002000,
  CAM_UFEO_BAYER12 = 0x00004000,
  CAM_UFEO_BAYER14 = 0x00008000
} E_CAM_FORMAT;

/******************************************************************************
 * @enum E_CAM_PipelineBitDepth_SEL
 *
 * @Pipeline bit depth format
 *
 ******************************************************************************/
typedef enum {
  CAM_Pipeline_10BITS = 0x0,
  CAM_Pipeline_12BITS = 0x1,
  CAM_Pipeline_14BITS = 0x2,
  CAM_Pipeline_16BITS = 0x4,
} E_CAM_PipelineBitDepth_SEL;

/**
    type for p1hwcfg module.
    note: Data type for ISP3.0
*/
enum EModule {
  // raw
  EModule_OB = 00,
  EModule_BNR = 05,
  EModule_LSC = 10,
  EModule_RPG = 15,
  EModule_AE = 20,
  EModule_AWB = 25,
  EModule_SGG1 = 30,
  EModule_FLK = 35,
  EModule_AF = 40,
  EModule_SGG2 = 45,
  EModule_SGG3 = 46,
  EModule_EIS = 50,
  EModule_LCS = 55,
  EModule_BPCI = 60,
  EModule_LSCI = 65,
  EModule_AAO = 70,
  EModule_ESFKO = 75,
  EModule_AFO = 80,
  EModule_EISO = 85,
  EModule_LCSO = 90,
  EModule_vHDR = 95,
  EModule_CAMSV_IMGO = 100,
  EModule_CAMSV2_IMGO = 101,
  // raw_d
  EModule_OB_D = 1000,
  EModule_BNR_D = 1005,
  EModule_LSC_D = 1010,
  EModule_RPG_D = 1015,
  EModule_BPCI_D = 1020,
  EModule_LSCI_D = 1025,
  EModule_AE_D = 1030,
  EModule_AWB_D = 1035,
  EModule_SGG1_D = 1040,
  EModule_AF_D = 1045,
  EModule_LCS_D = 1050,
  EModule_AAO_D = 1055,
  EModule_AFO_D = 1060,
  EModule_LCSO_D = 1065,
  EModule_vHDR_D = 1070
};

/******************************************************************************
 * @enum E_CamPixelMode
 *
 * @Pixel mode:
 *
 ******************************************************************************/
typedef enum {
  ePixMode_NONE = 0,
  ePixMode_1,
  ePixMode_2,
  ePixMode_4,
  ePixMode_MAX,
  _UNKNOWN_PIX_MODE = ePixMode_NONE,
  _1_PIX_MODE = ePixMode_1,
  _2_PIX_MODE = ePixMode_2,
  _4_PIX_MODE = ePixMode_4,
  _MAX_PIX_MODE = ePixMode_MAX,
} E_CamPixelMode;

typedef E_CamPixelMode Normalpipe_PIXMODE;
}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam

/******************************************************************************
 * @enum E_INPUT
 *
 * @TG input index
 *
 ******************************************************************************/
typedef enum {
  TG_A = 0,  // mapping to hw module CAM_A      0
  TG_B = 1,  // mapping to hw module CAM_B      1
  TG_CAM_MAX,
  TG_CAMSV_0 = 10,  // mapping to hw module CAMSV_0  2
  TG_CAMSV_1 = 11,  // mapping to hw module CAMSV_1  3
  TG_CAMSV_2 = 12,  // mapping to hw module CAMSV_2  4
  TG_CAMSV_3 = 13,  // mapping to hw module CAMSV_3  5
  TG_CAMSV_4 = 14,  // mapping to hw module CAMSV_4  6
  TG_CAMSV_5 = 15,  // mapping to hw module CAMSV_5  7
  TG_CAMSV_MAX,
  RAWI,
} E_INPUT;

/******************************************************************************
 * @enum E_CamPattern
 *
 * @input data pattern
 *
 ******************************************************************************/
typedef enum {
  eCAM_NORMAL = 0,
  eCAM_DUAL_PIX,  // dual pd's pattern
  eCAM_QuadCode,  // for MT6763
  eCAM_4CELL,
  eCAM_MONO,
  eCAM_IVHDR,
  eCAM_ZVHDR,
  eCAM_4CELL_IVHDR,
  eCAM_4CELL_ZVHDR,
  eCAM_DUAL_PIX_IVHDR,
  eCAM_DUAL_PIX_ZVHDR,
  eCAM_YUV,  // driver internal use
} E_CamPattern;

/******************************************************************************
 * @enum E_CamIQLevel
 *
 * @Image Quality level for dynamic bin
 *
 ******************************************************************************/
typedef enum {
  eCamIQ_L = 0,
  eCamIQ_H,
  eCamIQ_MAX,
} E_CamIQLevel;

/******************************************************************************
 * @enum E_CamRMBSel
 *
 * @RMB select path, please reference block diagram.
 *
 ******************************************************************************/
typedef enum {
  eRMBSel_0 = 0,
  eRMBSel_1,
  eRMBSel_2,
  eRMBSel_3,
} E_CamRMBSel;

/******************************************************************************
 * @enum E_SuspendLevel
 *
 * @Type1: suspend with sensor is on
 *    Type2: suspend with sensor is off
 *
 ******************************************************************************/
typedef enum {
  eSuspend_Unknow = 0,
  eSuspend_Type1,
  eSuspend_Type2,
} E_SUSPEND_TPYE;

/******************************************************************************
 * @struct _UFDG_META_INFO
 *
 * @UF format meta info
 *
 ******************************************************************************/
typedef struct _UFDG_META_INFO {
  MUINT32 bUF;
  MUINT32 UFDG_BITSTREAM_OFST_ADDR;
  MUINT32 UFDG_BS_AU_START;
  MUINT32 UFDG_AU2_SIZE;
  MUINT32 UFDG_BOND_MODE;
} UFDG_META_INFO;

using NSCam::NSIoPipe::NSCamIOPipe::E_CamPixelMode;
using NSCam::NSIoPipe::NSCamIOPipe::E_CamPixelMode::ePixMode_1;
using NSCam::NSIoPipe::NSCamIOPipe::E_CamPixelMode::ePixMode_2;
using NSCam::NSIoPipe::NSCamIOPipe::E_CamPixelMode::ePixMode_4;
using NSCam::NSIoPipe::NSCamIOPipe::E_CamPixelMode::ePixMode_MAX;
using NSCam::NSIoPipe::NSCamIOPipe::E_CamPixelMode::ePixMode_NONE;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ICAM_TYPE_H_
