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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_UTILS_CAMCAPABILITY_CAM_CAPABILITY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_UTILS_CAMCAPABILITY_CAM_CAPABILITY_H_

#include <vector>
#include <mtkcam/def/ImageFormat.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#include "ICam_capability.h"

typedef enum {
  CAM_A = 0,
  CAM_B,
  CAM_MAX,
  CAMSV_START = CAM_MAX,
  CAMSV_0 = CAMSV_START,
  CAMSV_1,
  CAMSV_2,
  CAMSV_3,
  CAMSV_4,
  CAMSV_5,
  CAMSV_MAX,
  DIP_START = CAMSV_MAX,
  DIP_A = DIP_START,
  DIP_MAX,
  MAX_ISP_HW_MODULE = DIP_MAX
} ISP_HW_MODULE;

typedef struct _minSize {
  MUINT32 w;
  MUINT32 h;
} minSize;

struct CamQueryOut {
  MUINT32 ratio;  // unit:%
  std::vector<NSCam::EImageFormat> Queue_fmt;
  MUINT32 pipelinebitdepth;
  MUINT32 pipeSize;
  union {
    MUINT32 bs_max_size;
    MUINT32 bs_alignment;
  } bs_info;
  MUINT32 HeaderSize;  // unit:byte

  MUINT32 x_pix;          // horizontal resolution, unit:pix
  MUINT32 xsize_byte[2];  // 2-plane xsize, uint:byte
  MUINT32 crop_x;         // crop start point-x , unit:pix
  MUINT32 stride_pix;  // stride, uint:pix. this is a approximative value under
                       // pak mode
  MUINT32 stride_byte[3];  // 3-plane stride, uint:byte

  MBOOL D_TWIN;     // 1: dynamic twin is ON, 0: dynamic twin is OFF.
  MBOOL IQlv;       // 1: suppourt IQ control,
                    // 0: not suppourt IQ control, use offbin.
  MUINT32 uni_num;  // the number of uni
  minSize pipelineMinSize;

  MUINT32 Sen_Num;
  MUINT32 D_UF;      // 1: dynamic uf. 0: static uf
  MUINT32 burstNum;  // burst number
  MUINT32 pattern;   // support pattern, bit field
  MBOOL D_Pak;       // 1: support Dynamic Pak. 0: not support Dynamic Pak.

  CamQueryOut(MUINT32 _ratio = 100,
              MUINT32 _pipelinebitdepth = 1,
              MUINT32 _pipeSize = 1)
      : ratio(_ratio),
        pipelinebitdepth(_pipelinebitdepth),
        pipeSize(_pipeSize),
        HeaderSize(0),
        x_pix(0),
        crop_x(0),
        stride_pix(0),
        D_TWIN(0),
        IQlv(0),
        uni_num(2),
        Sen_Num(0),
        D_UF(1),
        burstNum(0),
        pattern(0),
        D_Pak(1) {
    Queue_fmt.clear();
    pipelineMinSize.w = 0;
    pipelineMinSize.h = 0;
    bs_info.bs_max_size = 1;
    xsize_byte[0] = xsize_byte[1] = 0;
    stride_byte[0] = stride_byte[1] = stride_byte[2] = 0;
  }
};

// this compile option is for build pass on trunk which branch have 69
#define SUPPORTED_SEN_NUM
#define SUPPORTED_IQ_LV
#define SUPPORTED_DYNAMIC_PAK

typedef enum {
  E_CAM_UNKNOWN = 0x0,

  E_CAM_SRAM_DMX = 0x1,
  E_CAM_SRAM_BMX = 0x2,
  E_CAM_SRAM_AMX = 0x4,
  E_CAM_SRAM_RMX = 0x8,

  E_CAM_pipeline_size = 0x20,

  E_CAM_BS_Max_size = 0x100,   // can't or with BS_alginment
  E_CAM_BS_Alignment = 0x200,  // can't or with BS_max_size

  E_CAM_pipeline_min_size = 0x1000,  // query minimun width size that this
                                     // platform could be supported.
  E_CAM_DTWIN_ONOFF = 0x2000,        // query dynamic twin is turned on or off.
  E_CAM_HEADER_size = 0x4000,
} E_CAM_Query_OP;

class capability {
 public:
  capability() : m_hwModule(CAM_MAX) {}
  explicit capability(NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM module);
  ~capability() = default;

  MBOOL GetCapability(MUINT32, E_CAM_Query, CAM_Queryrst&) { return MFALSE; }

  MBOOL GetCapability(
      MUINT32 portId,
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd e_Op,
      NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_InputInfo inputInfo,
      CamQueryOut* p_query_output);

 private:
  MUINT32 GetFormat(MUINT32 portID, CamQueryOut* p_query_output);
  MUINT32 GetRatio(MUINT32 portID);
  MUINT32 GetPipelineBitdepth();
  MVOID GetPipelineMinSize(minSize* size, E_CamPixelMode pixMode);
  MUINT32 GetPipeSize();
  MUINT32 GetRRZSize();
  MUINT32 GetRLB_SRAM_Alignment();
  MUINT32 GetHeaderSize();
  MUINT32 GetConstrainedSize(
      MUINT32 portId,
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd e_Op,
      NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_InputInfo inputInfo,
      CamQueryOut* p_QueryRst);
  MUINT32 GetMaxSenNum();
  MUINT32 GetSupportBurstNum();
  MUINT32 GetSupportPattern();

 private:
  ISP_HW_MODULE m_hwModule;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_UTILS_CAMCAPABILITY_CAM_CAPABILITY_H_
