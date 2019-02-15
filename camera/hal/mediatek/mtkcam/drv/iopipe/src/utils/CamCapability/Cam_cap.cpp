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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "cam_capi"

#include <memory>
#include <string>

#include <mtkcam/utils/std/Log.h>
#include "CamCapability/cam_capability.h"
#include "CamCapability/ICam_capability.h"

#ifndef NULL
#define NULL 0
#endif

CAM_CAPABILITY::CAM_CAPABILITY()
    : m_hwModule(NSCam::NSIoPipe::NSCamIOPipe::ENPipe_UNKNOWN) {}

CAM_CAPABILITY* CAM_CAPABILITY::getInstance(
    std::string szCallerName, NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM module) {
  MUINT32 num;
  static CAM_CAPABILITY QueryObj[NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_MAX];

  switch (module) {
    case NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_A:
      num = 1;
      break;
    case NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_B:
      num = 2;
      break;
    case NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_C:
      num = 3;
      break;
    case NSCam::NSIoPipe::NSCamIOPipe::ENPipe_UNKNOWN:
      num = 0;
      break;
    default:
      ALOGE(LOG_TAG "[%s]ERR(%5d):user:%s out of module range,%d\n",
            __FUNCTION__, __LINE__, szCallerName.c_str(), module);
      return NULL;
  }
  if (num >= NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_MAX) {
    ALOGE(LOG_TAG "[%s]ERR(%5d):user:%s out of module range,%d\n", __FUNCTION__,
          __LINE__, szCallerName.c_str(), module);
    return NULL;
  }

  QueryObj[num].m_hwModule = module;
  QueryObj[num].m_Name.assign(szCallerName);

  return &QueryObj[num];
}

MBOOL CAM_CAPABILITY::GetCapability(
    MUINT32 portId,
    NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd e_Op,
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_InputInfo inputInfo,
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo* p_QueryRst) {
  NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo& QueryRst = *p_QueryRst;
  MBOOL rst = MTRUE;
  CamQueryOut camrst;
  std::unique_ptr<capability> up_capability;

  if (m_hwModule == NSCam::NSIoPipe::NSCamIOPipe::ENPipe_UNKNOWN) {
    up_capability = std::make_unique<capability>();
  } else {
// share  this compile flag - SUPPORTED_SEN_NUM for ISP50
#ifdef SUPPORTED_SEN_NUM
    up_capability = std::make_unique<capability>(m_hwModule);
#else
    up_capability = std::make_unique<capability>();
#endif
  }
  rst = up_capability->GetCapability(portId, e_Op, inputInfo, &camrst);

  if (rst == MTRUE) {
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BS_RATIO) {
      QueryRst.bs_ratio = camrst.ratio;
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_QUERY_FMT) {
      QueryRst.query_fmt = camrst.Queue_fmt;
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_PIPELINE_BITDEPTH) {
      QueryRst.pipelinebitdepth = camrst.pipelinebitdepth;
    }

    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_MAX_SEN_NUM) {
// share  this compile flag - SUPPORTED_SEN_NUM for ISP50
#ifdef SUPPORTED_SEN_NUM
      QueryRst.sen_num = camrst.Sen_Num;
      QueryRst.function.Bits.SensorNum = camrst.Sen_Num;
#endif
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BURST_NUM) {
      QueryRst.burstNum = camrst.burstNum;
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_SUPPORT_PATTERN) {
      QueryRst.pattern = camrst.pattern;
    }
    if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX |
                NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_PIX)) {
      QueryRst.x_pix = camrst.x_pix;
    }
    if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_BYTE |
                NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_BYTE)) {
// share  this compile flag - SUPPORTED_SEN_NUM for ISP50
#ifdef SUPPORTED_SEN_NUM
      QueryRst.xsize_byte = camrst.xsize_byte[0];
#else
      QueryRst.xsize_byte = camrst.xsize_byte;
#endif
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_START_X) {
      QueryRst.crop_x = camrst.crop_x;
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_PIX) {
      QueryRst.stride_pix = camrst.stride_pix;
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE) {
// share  this compile flag - SUPPORTED_SEN_NUM for ISP50
#ifdef SUPPORTED_SEN_NUM
      QueryRst.stride_byte = camrst.stride_byte[0];
      QueryRst.stride_B[0] = camrst.stride_byte[0];
      QueryRst.stride_B[1] = camrst.stride_byte[1];
      QueryRst.stride_B[2] = camrst.stride_byte[2];
#else
      QueryRst.stride_byte = camrst.stride_byte;
      QueryRst.stride_B[0] = camrst.stride_byte;
#endif
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_D_Twin) {
      QueryRst.D_TWIN = camrst.D_TWIN;
      QueryRst.function.Bits.D_TWIN = camrst.D_TWIN;
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_IQ_LEVEL) {
#ifdef SUPPORTED_IQ_LV
      QueryRst.IQlv = camrst.IQlv;
#else
      QueryRst.IQlv = MFALSE;
#endif
    }

    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_D_BayerEnc) {
// share  this compile flag - SUPPORTED_SEN_NUM for ISP50
#ifdef SUPPORTED_SEN_NUM
      QueryRst.function.Bits.D_BayerENC = camrst.D_UF;
#endif
    }
    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_UNI_NUM) {
      QueryRst.uni_num = camrst.uni_num;
    }

    if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_DYNAMIC_PAK) {
// share  this compile flag - SUPPORT_DYNAMIC_PAK for ISP50
#ifdef SUPPORTED_DYNAMIC_PAK
      QueryRst.D_Pak = camrst.D_Pak;
#else
      QueryRst.D_Pak = MFALSE;
#endif
    }

    if ((e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_FUNC) ==
        NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_FUNC) {
// if no simple raw c -> this cmd is not supported  -> all functions below will
// be set to 0. flk/lmv/rss r not use this cmd at lagecy IC
#ifdef SUPPORTED_SIMPLE_RAW_C
      QueryRst.function.Bits.isFLK = camrst.bSupportedModule.bFlk;
      QueryRst.function.Bits.isLMV = camrst.bSupportedModule.bLMV;
      QueryRst.function.Bits.isRSS = camrst.bSupportedModule.bRSS;
      QueryRst.function.Bits.isFull_dir_YUV = camrst.bSupportedModule.bFullYUV;
      QueryRst.function.Bits.isScaled_Y = camrst.bSupportedModule.bScaledY;
      QueryRst.function.Bits.isScaled_YUV = camrst.bSupportedModule.bScaledYUV;
      QueryRst.function.Bits.isG_Scaled_YUV =
          camrst.bSupportedModule.bGScaledYUV;
#endif
    }
  } else {
    ALOGE(LOG_TAG "[%s]ERR(%5d):user:%s query fail(module:%d)\n", __FUNCTION__,
          __LINE__, m_Name.c_str(), m_hwModule);
  }

  return rst;
}
