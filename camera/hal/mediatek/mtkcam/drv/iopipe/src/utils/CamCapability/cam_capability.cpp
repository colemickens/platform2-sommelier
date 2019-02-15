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
#define LOG_TAG "cam_capibitiliy"

#include "CamCapability/cam_capability.h"

#include <mtkcam/def/ImageFormat.h>
#include <mtkcam/drv/def/ICam_type.h>
#include <mtkcam/drv/def/ispio_port_index.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>

#include <property_service/property.h>
#include <property_service/property_lib.h>
#undef DBG_LOG_TAG      // Decide a Log TAG for current file.
#ifndef USING_MTK_LDVT  // Not using LDVT.
#define DBG_LOG_TAG ""
#else
#define DBG_LOG_TAG LOG_TAG
#endif

#include "mtkcam/utils/std/Log.h"

capability::capability(NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM module) {
  switch (module) {
    case NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_A:
      m_hwModule = CAM_A;
      break;
    case NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM_B:
      m_hwModule = CAM_B;
      break;
    default:
      m_hwModule = CAM_MAX;
      MY_LOGE("unsupported module:%d", module);
      break;
  }
}

MBOOL capability::GetCapability(
    MUINT32 portId,
    NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd e_Op,
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_InputInfo inputInfo,
    CamQueryOut* p_query_output) {
  CamQueryOut& query_output = *p_query_output;
  MBOOL ret = MTRUE;

  MUINT32 valid_cmd_cap =
      (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BS_RATIO |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_QUERY_FMT |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_PIPELINE_BITDEPTH |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_D_Twin |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_MAX_SEN_NUM |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_D_BayerEnc |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_IQ_LEVEL |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BURST_NUM |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_SUPPORT_PATTERN |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_DYNAMIC_PAK);

  MUINT32 valid_cmd_constraint =
      (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_BYTE |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_PIX |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_BYTE |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_START_X |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_PIX |
       NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE);

  if ((e_Op & valid_cmd_cap) ==
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BS_RATIO) {
    if ((query_output.ratio = GetRatio(portId)) == 0)
      ret = MFALSE;
  }

  if ((e_Op & valid_cmd_cap) ==
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_QUERY_FMT) {
    if (GetFormat(portId, &query_output) == MFALSE)
      ret = MFALSE;
  }

  if ((e_Op & valid_cmd_cap) ==
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_PIPELINE_BITDEPTH) {
    query_output.pipelinebitdepth = GetPipelineBitdepth();
  }
  if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BURST_NUM) {
    query_output.burstNum = GetSupportBurstNum();
  }

  if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_SUPPORT_PATTERN) {
    query_output.pattern = GetSupportPattern();
  }

  if ((e_Op & valid_cmd_cap) ==
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_MAX_SEN_NUM) {
    query_output.Sen_Num = GetMaxSenNum();
  }

  if (e_Op & valid_cmd_constraint) {
    if (GetConstrainedSize(portId, e_Op, inputInfo, &query_output) == MFALSE) {
      ret = MFALSE;
    }
  }

  if (e_Op & (~(valid_cmd_cap | valid_cmd_constraint))) {
    ret = MFALSE;
    MY_LOGE("some query cmd(0x%x) is not supported. valid cmd(0x%x)\n", e_Op,
            (valid_cmd_cap | valid_cmd_constraint));
  }

  return ret;
}

MUINT32 capability::GetFormat(MUINT32 portID, CamQueryOut* p_query_output) {
  CamQueryOut& query_output = *p_query_output;
  if (query_output.Queue_fmt.size()) {
    MY_LOGE("current portID(0x%x) Queue_fmt need init\n", portID);
    return MFALSE;
  }

  switch (portID) {
    case NSImageio::NSIspio::EPortIndex_IMGO: {
      NSCam::EImageFormat formats[] = {
          NSCam::eImgFmt_BAYER8,        NSCam::eImgFmt_BAYER10,
          NSCam::eImgFmt_BAYER12,       NSCam::eImgFmt_BAYER10_MIPI,
          NSCam::eImgFmt_BAYER8_UNPAK,  NSCam::eImgFmt_BAYER10_UNPAK,
          NSCam::eImgFmt_BAYER12_UNPAK,
      };
      query_output.Queue_fmt.assign(
          formats, formats + (sizeof(formats) / sizeof(formats[0])));
      break;
    }
    case NSImageio::NSIspio::EPortIndex_RRZO: {
      NSCam::EImageFormat formats[] = {
          NSCam::eImgFmt_FG_BAYER8,
          NSCam::eImgFmt_FG_BAYER10,
          NSCam::eImgFmt_FG_BAYER12,
      };
      query_output.Queue_fmt.assign(
          formats, formats + (sizeof(formats) / sizeof(formats[0])));
      break;
    }
    default:
      MY_LOGE("current portID(0x%x) are not supported in query\n", portID);
      return MFALSE;
  }
  return MTRUE;
}

MUINT32 capability::GetRatio(MUINT32 portID) {
#define MAX_SCALING_DOWN_RATIO (6)  // unit:%

  switch (portID) {
    case NSImageio::NSIspio::EPortIndex_RRZO:
      return MAX_SCALING_DOWN_RATIO;
    default:
      MY_LOGE("curent portID(0x%x) are no scaler\n", portID);
      break;
  }
  return 100;
}

MUINT32 capability::GetPipelineBitdepth() {
  return (NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_12BITS);
}

MVOID capability::GetPipelineMinSize(minSize* size, E_CamPixelMode pixMode) {
#define CAM_PIPELINE_MIN_WIDTH_SIZE \
  (120 * 4)  // awb win_w is fixed in 120, 4 is for 1 block need 4 pixels.
#define CAM_PIPELINE_MIN_HEIGHT_SIZE \
  (90 * 2)  // awb win_h is fixed in 90, 2 is for 1 block need 2 pixels.

  if (pixMode == ePixMode_4) {  // 2 pixels for qbin_acc 1
    size->w = (CAM_PIPELINE_MIN_WIDTH_SIZE << 2);
    size->h = (CAM_PIPELINE_MIN_HEIGHT_SIZE << 2);
  } else if (pixMode == ePixMode_2) {  // 2 pixels for qbin_acc 1
    size->w = (CAM_PIPELINE_MIN_WIDTH_SIZE << 1);
    size->h = (CAM_PIPELINE_MIN_HEIGHT_SIZE << 1);
  } else {
    size->w = CAM_PIPELINE_MIN_WIDTH_SIZE;
    size->h = CAM_PIPELINE_MIN_HEIGHT_SIZE;
  }
}

MUINT32 capability::GetPipeSize() {
#define CAM_A_MAX_LINE_BUFFER_IN_PIXEL (3328)
#define CAM_B_MAX_LINE_BUFFER_IN_PIXEL (5376)
#define CAM_C_MAX_LINE_BUFFER_IN_PIXEL (3328)
  MUINT32 value;

  switch (m_hwModule) {
    case CAM_A:
      value = CAM_A_MAX_LINE_BUFFER_IN_PIXEL;
      break;
    case CAM_B:
      value = CAM_B_MAX_LINE_BUFFER_IN_PIXEL;
      break;
    default:
      MY_LOGE("need to assign a hw module, like CAM_A ...etc.\n");
      value = 0;
      break;
  }

  return value;
}

MUINT32 capability::GetRRZSize() {
#undef CAM_RRZ_MAX_LINE_BUFFER_IN_PIXEL
#define CAM_RRZ_MAX_LINE_BUFFER_IN_PIXEL ((8192))

  return CAM_RRZ_MAX_LINE_BUFFER_IN_PIXEL;
}

MUINT32 capability::GetRLB_SRAM_Alignment() {
#define RLB_ALIGNMENT (8)

  return RLB_ALIGNMENT;
}

MUINT32 capability::GetSupportBurstNum() {
  /*
  return support burst number
  0x0 : not support       // ISP 3.0
  0x2 : support 2
  0x6 : support 2|4
  0xE : support 2|4|8
  0x1E: support 2|4|8|16  // ISP 4.0 later
  */
  return 0x1E;
}

MUINT32 capability::GetSupportPattern() {
  MUINT32 pattern;
  pattern = 0x1 << eCAM_NORMAL | 0x1 << eCAM_DUAL_PIX | 0x1 << eCAM_QuadCode |
            0x1 << eCAM_4CELL | 0x1 << eCAM_MONO | 0x1 << eCAM_IVHDR |
            0x1 << eCAM_ZVHDR | 0x1 << eCAM_4CELL_IVHDR |
            0x1 << eCAM_4CELL_ZVHDR | 0x1 << eCAM_DUAL_PIX_IVHDR |
            0x1 << eCAM_DUAL_PIX_ZVHDR | 0x1 << eCAM_YUV;
  return pattern;
}

MUINT32 capability::GetHeaderSize() {
#define spare_num (13)
  return spare_num * 4;
}

MUINT32 capability::GetMaxSenNum() {
#define CAM_TG_NUM (2)
#define CAMSV_TG_NUM (6)
  switch (m_hwModule) {
    case CAM_A:
    case CAM_B:
      return CAM_TG_NUM;
      break;
    case CAMSV_0:
    case CAMSV_1:
    case CAMSV_2:
    case CAMSV_3:
    case CAMSV_4:
    case CAMSV_5:
      return CAMSV_TG_NUM;
      break;
    default:
      MY_LOGE("need to assign a hw module, like CAM_X,CAM_SV_X..etc.\n");
      break;
  }
  return 0;
}

static void _query_p2_stride_constraint(MUINT32* stride) {
  if (stride)
    *stride = (*stride + 0x3) & (~0x3);
}

static void query_rrzo_constraint(MUINT32* xsize,
                                  E_CamPixelMode pixMode,
                                  NSCam::EImageFormat imgFmt) {
  if (xsize) {
    switch (pixMode) {
      case E_CamPixelMode::ePixMode_NONE:
      case ePixMode_4:
        *xsize = (*xsize + 0xf) & (~0xf);
        break;
      case ePixMode_2:
        *xsize = (*xsize + 0x7) & (~0x7);
        break;
      case ePixMode_1:
        if (imgFmt == NSCam::eImgFmt_FG_BAYER10)
          *xsize = (*xsize + 0x3) & (~0x3);
        else
          *xsize = (*xsize + 0x7) & (~0x7);
        break;
      default:
        *xsize = (*xsize + 0xf) & (~0xf);
        break;
    }
  }
}

static MUINT32 _query_fg_constraint(MUINT32 size) {
  return (size & 0x3) ? 0 : 1;
}

static MUINT32 _query_fg_align_size(MUINT32 size) {
  return ((size + 0x3) & ~0x3);
}

static MUINT32 _queryBitPerPix(MUINT32 const imgFmt) {
  MUINT32 pixDepth;

  switch (imgFmt) {
    case NSCam::eImgFmt_BAYER8:  //= 0x0001,   //Bayer format, 8-bit
    case NSCam::eImgFmt_Y8:      // weighting table
    case NSCam::eImgFmt_FG_BAYER8:
      pixDepth = 8;
      break;
    case NSCam::eImgFmt_BAYER10:  //= 0x0002,   //Bayer format, 10-bit
    case NSCam::eImgFmt_FG_BAYER10:
      pixDepth = 10;
      break;
    case NSCam::eImgFmt_BAYER12:  //= 0x0004,   //Bayer format, 12-bit
    case NSCam::eImgFmt_FG_BAYER12:
      pixDepth = 12;
      break;
    case NSCam::eImgFmt_BAYER14:  //= 0x0008,   //Bayer format, 14-bit
      pixDepth = 16;              //?
      break;
    case NSCam::eImgFmt_BAYER8_UNPAK:
    case NSCam::eImgFmt_BAYER10_UNPAK:
    case NSCam::eImgFmt_BAYER12_UNPAK:
    case NSCam::eImgFmt_BAYER14_UNPAK:
      pixDepth = 16;
      break;
    case NSCam::eImgFmt_YUY2:  //= 0x0100,   //422 format, 1 plane (YUYV)
    case NSCam::eImgFmt_UYVY:  //= 0x0200,   //422 format, 1 plane (UYVY)
    case NSCam::eImgFmt_YVYU:  //= 0x080000,   //422 format, 1 plane (YVYU)
    case NSCam::eImgFmt_VYUY:  //= 0x100000,   //422 format, 1 plane (VYUY)
      pixDepth = 16;
      break;
    case NSCam::eImgFmt_NV16:  // user would set format_nv16 to all of 2 planes
                               // if multi-plane(img3o~img3bo)
    case NSCam::eImgFmt_NV12:
    case NSCam::eImgFmt_YV12:  // user would set format_yv12 to all of 3 planes
                               // if multi-plane(img3o~img3co)
    case NSCam::eImgFmt_I420:
      pixDepth = 8;
      break;
    case NSCam::eImgFmt_RGB565:
    case NSCam::eImgFmt_STA_2BYTE:
      pixDepth = 16;
      break;
    case NSCam::eImgFmt_RGB888:
      pixDepth = 24;
      break;
    case NSCam::eImgFmt_JPEG:
      pixDepth = 8;
      break;
    default:
      MY_LOGE("eImgFmt:[%d]NOT Support", imgFmt);
      return -1;
  }
  if (imgFmt == NSCam::eImgFmt_FG_BAYER8 ||
      imgFmt == NSCam::eImgFmt_FG_BAYER10 ||
      imgFmt == NSCam::eImgFmt_FG_BAYER12) {
    //  FG_BAYER has 50% pixel more than bayer in order to describe green pixel.
    pixDepth = (pixDepth * 3) / 2;
  }
  return pixDepth;
}

static MBOOL _queryCropStart(MUINT32 portId,
                             NSCam::EImageFormat imgFmt,
                             MUINT32 input,
                             CamQueryOut* p_query_output,
                             E_CamPixelMode e_PixMode) {
  CamQueryOut& query_output = *p_query_output;
  switch (portId) {
    case NSImageio::NSIspio::EPortIndex_IMGO:
      switch (imgFmt) {
        case NSCam::eImgFmt_BAYER10:
          switch (e_PixMode) {
            case ePixMode_1:
              // if bus size 16bit, the inputer must be 8 alignment,
              query_output.crop_x = (input >> 3) << 3;
              break;
            case ePixMode_2:
              // if bus size 16bit, the inputer must be 16 alignment,
              query_output.crop_x = (input >> 4) << 4;
              break;
            default:
            case ePixMode_4:
              // if bus size 16bit, the inputer must be 32 alignment,
              query_output.crop_x = (input >> 5) << 5;
              break;
          }
          break;
        case NSCam::eImgFmt_BAYER12:
          switch (e_PixMode) {
            case ePixMode_1:
              // if bus size 16bit, the inputer must be 4 alignment,
              query_output.crop_x = (input >> 2) << 2;
              break;
            case ePixMode_2:
              // if bus size 16bit, the inputer must be 8 alignment,
              query_output.crop_x = (input >> 3) << 3;
              break;
            default:
            case ePixMode_4:
              // if bus size 16bit, the inputer must be 16 alignment,
              query_output.crop_x = (input >> 4) << 4;
              break;
          }
          break;
        case NSCam::eImgFmt_BAYER8:
        case NSCam::eImgFmt_FG_BAYER8:
        case NSCam::eImgFmt_FG_BAYER10:
        case NSCam::eImgFmt_FG_BAYER12:
          switch (e_PixMode) {
            case ePixMode_1:
              // if bus size 16bit, the inputer must be 2 alignment,
              query_output.crop_x = (input >> 1) << 1;
              break;
            case ePixMode_2:
              // if bus size 16bit, the inputer must be 4 alignment,
              query_output.crop_x = (input >> 2) << 2;
              break;
            default:
            case ePixMode_4:
              // if bus size 16bit, the inputer must be 8 alignment,
              query_output.crop_x = (input >> 3) << 3;
              break;
          }
          break;
        case NSCam::eImgFmt_BAYER8_UNPAK:
        case NSCam::eImgFmt_BAYER10_UNPAK:
        case NSCam::eImgFmt_BAYER12_UNPAK:
        case NSCam::eImgFmt_BAYER14_UNPAK:
          query_output.crop_x = 0;
          break;
        default:
          query_output.crop_x = 0;
          MY_LOGE("NOT SUPPORT imgFmt(%d)", imgFmt);
          return MFALSE;
      }
      break;
    case NSImageio::NSIspio::EPortIndex_RRZO:  // rrz support only rrz_in crop,
                                               // not dma crop
      input -= (input & 0x1);
      query_output.crop_x = input;
      break;
    default:
      MY_LOGE("NOT SUPPORT port(%d)", portId);
      return MFALSE;
  }

  return MTRUE;
}

static MUINT32 _query_p2_constraint(MUINT32 size) {
  return (size & 0x1) ? 0 : 1;
}

static MUINT32 _query_pix_mode_constraint(MUINT32 size, MUINT32 pixMode) {
  /*
  1 pix mode => 2n
  2 pix mode => 4n
  but current flow , user have no pix mode information, so fix 4n for safety
  */
  MUINT32 divisor = 0x3;
  switch (pixMode) {
    case ePixMode_NONE:
    case ePixMode_4:
      divisor = 0x7;
      break;
    case ePixMode_2:
      divisor = 0x3;
      break;
    case ePixMode_1:
      divisor = 0x1;
      break;
    default:
      MY_LOGE("pix mode error(%d)", pixMode);
      break;
  }
  if (size & divisor)
    return 0;
  else
    return 1;
}
static MUINT32 _query_pix_mode_align_size(MUINT32 size, MUINT32 pixMode) {
  MUINT32 divisor = 0x7, align_size = size;

  switch (pixMode) {
    case ePixMode_NONE:
    case ePixMode_4:
      divisor = 0x7;
      break;
    case ePixMode_2:
      divisor = 0x3;
      break;
    case ePixMode_1:
      divisor = 0x1;
      break;
    default:
      MY_LOGE("pix mode error(%d)", pixMode);
      break;
  }

  align_size = ((align_size + divisor) & ~divisor);

  return align_size;
}

static MUINT32 _calculateAlignedXsize(MUINT32 xsize, E_CamPixelMode pix_mode) {
  // expand stride, instead of shrink width
  if (!(_query_pix_mode_constraint(xsize, pix_mode) &&
        _query_p2_constraint(xsize))) {
    MUINT32 aligned = _query_pix_mode_align_size(xsize, pix_mode);

    if (_query_p2_constraint(aligned)) {
      MY_LOGD("constraint: align xsize(%d/%d)\n", xsize, aligned);
    } else {
      MY_LOGW("constraint: cannot pass p2 constraint(%d)\n", xsize);
    }
    return aligned;
  } else {
    return xsize;
  }
}

static MBOOL _queryXsize_imgo(NSCam::EImageFormat imgFmt,
                              MUINT32 xsize[3],
                              MUINT32* p_outWidth,
                              E_CamPixelMode pixMode,
                              MBOOL bCrop) {
  MUINT32& outWidth = *p_outWidth;
  /* ensure output width is 4btye alignment */
  outWidth = (outWidth + 0x3) & ~0x3;

  switch (imgFmt) {
    case NSCam::eImgFmt_BAYER8:
    case NSCam::eImgFmt_JPEG:
      xsize[0] = outWidth;
      xsize[0] = _calculateAlignedXsize(xsize[0], pixMode);
      break;
    case NSCam::eImgFmt_BAYER10_MIPI:
    case NSCam::eImgFmt_BAYER10:
      xsize[0] = (outWidth * 10 + 7) / 8;
      xsize[0] += (xsize[0] & 0x1);
      xsize[0] = _calculateAlignedXsize(xsize[0], pixMode);
      break;
    case NSCam::eImgFmt_BAYER12:
      // no need to patch under cropping , because outWidth must be at leaset
      // 2-alignment
      xsize[0] = (outWidth * 12 + 7) / 8;
      xsize[0] += (xsize[0] & 0x1);
      xsize[0] = _calculateAlignedXsize(xsize[0], pixMode);
      break;
    case NSCam::eImgFmt_BAYER8_UNPAK:
    case NSCam::eImgFmt_BAYER10_UNPAK:
    case NSCam::eImgFmt_BAYER12_UNPAK:
      xsize[0] = outWidth * 2;  // fixed 16 bit
      xsize[0] += (xsize[0] & 0x1);
      xsize[0] = _calculateAlignedXsize(xsize[0], pixMode);
      break;
    case NSCam::eImgFmt_YUY2:
    case NSCam::eImgFmt_UYVY:
    case NSCam::eImgFmt_YVYU:
    case NSCam::eImgFmt_VYUY:
      xsize[0] = outWidth * 2;
      xsize[0] = _calculateAlignedXsize(xsize[0], pixMode);
      break;
    case NSCam::eImgFmt_BLOB:
      xsize[0] = outWidth;
      break;
    default:
      xsize[0] = outWidth = 0;
      MY_LOGE("NOT SUPPORT imgFmt(%d)", imgFmt);
      return MFALSE;
  }
  return MTRUE;
}

static MBOOL _queryXsize_rrzo(NSCam::EImageFormat imgFmt,
                              MUINT32 xsize[3],
                              MUINT32* p_outWidth,
                              E_CamPixelMode pixMode) {
  MUINT32& outWidth = *p_outWidth;
  /* ensure output width is 4btye alignment */
  outWidth = (outWidth + 0x3) & ~0x3;

  switch (imgFmt) {
    case NSCam::eImgFmt_FG_BAYER8:
      xsize[0] = outWidth * 3 >> 1;
      query_rrzo_constraint(&xsize[0], pixMode, imgFmt);

      // expand stride, instead of shrink width
      if (!_query_fg_constraint(xsize[0])) {
        MUINT32 aligned = _query_fg_align_size(xsize[0]);

        MY_LOGI("constraint: align fg xsize(%d/%d)\n", xsize[0], aligned);
        xsize[0] = aligned;
      }
      break;
    case NSCam::eImgFmt_FG_BAYER10:
      xsize[0] = outWidth * 3 >> 1;
      xsize[0] = (xsize[0] * 10 + 7) / 8;
      query_rrzo_constraint(&xsize[0], pixMode, imgFmt);

      // expand stride, instead of shrink width
      if (!_query_fg_constraint(xsize[0])) {
        MUINT32 aligned = _query_fg_align_size(xsize[0]);

        MY_LOGI("constraint: align fg xsize(%d/%d)\n", xsize[0], aligned);
        xsize[0] = aligned;
      }
      break;
    case NSCam::eImgFmt_FG_BAYER12:
      xsize[0] = outWidth * 3 >> 1;
      xsize[0] = (xsize[0] * 12 + 7) / 8;
      query_rrzo_constraint(&xsize[0], pixMode, imgFmt);

      // expand stride, instead of shrink width
      if (!_query_fg_constraint(xsize[0])) {
        MUINT32 aligned = _query_fg_align_size(xsize[0]);

        MY_LOGI("constraint: align fg xsize(%d/%d)\n", xsize[0], aligned);
        xsize[0] = aligned;
      }
      break;
    default:
      xsize[0] = outWidth = 0;
      MY_LOGE("rrzo NOT SUPPORT imgFmt(%d)", imgFmt);
      return MFALSE;
  }
  return MTRUE;
}

MUINT32 capability::GetConstrainedSize(
    MUINT32 portId,
    NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd e_Op,
    NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_InputInfo inputInfo,
    CamQueryOut* p_query_output) {
  CamQueryOut& query_output = *p_query_output;
  MUINT32 outWidth = inputInfo.width;
  MBOOL _bCrop = MFALSE;

  switch (portId) {
    case NSImageio::NSIspio::EPortIndex_IMGO:
    case NSImageio::NSIspio::EPortIndex_RRZO: {
      MUINT32 Xsize[3];
      if (portId == NSImageio::NSIspio::EPortIndex_IMGO) {
        if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_PIX |
                    NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_BYTE)) {
          _bCrop = MTRUE;
        } else {
          _bCrop = MFALSE;
        }

        if (_queryXsize_imgo(inputInfo.format, Xsize, &outWidth,
                             inputInfo.pixelMode, _bCrop) == MFALSE) {
          return MFALSE;
        }
      } else {  // rrzo
        if (_queryXsize_rrzo(inputInfo.format, Xsize, &outWidth,
                             inputInfo.pixelMode) == MFALSE) {
          return MFALSE;
        }
      }

      if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX |
                  NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_PIX)) {
        query_output.x_pix = outWidth;
      }
      if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_PIX) {
        query_output.stride_pix = Xsize[0] / _queryBitPerPix(inputInfo.format);
      }
      if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE) {
        query_output.stride_byte[0] = Xsize[0];
        _query_p2_stride_constraint(&query_output.stride_byte[0]);
      }
      if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_BYTE |
                  NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_BYTE)) {
        query_output.xsize_byte[0] = Xsize[0];
      }
      if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_START_X) {
        if (_queryCropStart(portId, inputInfo.format, inputInfo.width,
                            &query_output, inputInfo.pixelMode) == MFALSE) {
          MY_LOGE("unsupported format:0x%x\n", inputInfo.format);
        }
      }
    } break;
    case NSImageio::NSIspio::EPortIndex_CAMSV_IMGO: {
      MUINT32 xsize[3];
      if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_PIX |
                  NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_BYTE)) {
        _bCrop = MTRUE;
      } else {
        _bCrop = MFALSE;
      }

      if (_queryXsize_imgo(inputInfo.format, xsize, &outWidth,
                           inputInfo.pixelMode, _bCrop) == MFALSE) {
        return MFALSE;
      }

      if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_PIX |
                  NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_PIX)) {
        query_output.x_pix = outWidth;
      }
      if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_PIX) {
        query_output.stride_pix = xsize[0] / _queryBitPerPix(inputInfo.format);
      }
      if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE) {
        query_output.stride_byte[0] = xsize[0];
        _query_p2_stride_constraint(&query_output.stride_byte[0]);
      }
      if (e_Op & (NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_X_BYTE |
                  NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_X_BYTE)) {
        query_output.xsize_byte[0] = xsize[0];
      }
      if (e_Op & NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_CROP_START_X) {
        if (_queryCropStart(portId, inputInfo.format, inputInfo.width,
                            &query_output, inputInfo.pixelMode) == MFALSE) {
          MY_LOGE("unsupported format:0x%x\n", inputInfo.format);
        }
      }
    } break;
    default:
      MY_LOGE("current portID(0x%x) r not supported in query\n", portId);
      query_output.x_pix = query_output.stride_pix =
          query_output.stride_byte[0] = 0;
      return MFALSE;
  }

  return MTRUE;
}
