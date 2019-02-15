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

#include "DebugControl.h"
#define PIPE_CLASS_TAG "P2A_3DNR"
#define PIPE_TRACE 0
#include <featurePipe/common/include/PipeLog.h>
#include "P2ANode.h"
#include "P2CamContext.h"
#include "hal/inc/camera_custom_3dnr.h"
#include <memory>
#include <mtkcam/aaa/IIspMgr.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/feature/3dnr/3dnr_defs.h>

using NSCam::NSIoPipe::Input;
using NSCam::NSIoPipe::Output;
using NSCam::NSIoPipe::NSPostProc::hal3dnrBase;
using NSCam::NSIoPipe::NSPostProc::NR3DHALParam;
using NSImageio::NSIspio::EPortIndex_IMG3O;
using NSImageio::NSIspio::EPortIndex_VIPI;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

MBOOL P2ANode::do3dnrFlow(NSCam::NSIoPipe::QParams* enqueParams,
                          const RequestPtr& request,
                          const MRect& dst_resizer_rect,
                          const MSize& resize_size,
                          const eis_region& eisInfo,
                          MINT32 iso,
                          MINT32 isoThreshold,
                          MUINT32 requestNo,
                          const P2ATuningIndex& tuningIndex) {
  TRACE_FUNC_ENTER();
  CAM_LOGD("do3dnrFlow+");

  // TODO(MTK) currently only support Master output
  MINT32 masterIndex = tuningIndex.isGenMasterValid() ? tuningIndex.mGenMaster
                                                      : tuningIndex.mPhyMaster;
  if (masterIndex < 0) {
    CAM_LOGE("No Master Gen or Phy tuning index exist! Can not do3dnrFlow.");
    return MFALSE;
  }
  const MUINT32 sensorID = request->mMasterID;

  MBOOL ret = MFALSE;
  MBOOL isSupportV2Flow = MTRUE;  // default: support V2 flow

  std::shared_ptr<P2CamContext> p2CamContext = getP2CamContext(sensorID);
  IImageBuffer* pIMGBufferVIPI = NULL;
  ImgBuffer prevFullImg = p2CamContext->getPrevFullImg();
  if (prevFullImg != NULL) {
    pIMGBufferVIPI = prevFullImg->getImageBufferPtr();
  }

  std::shared_ptr<hal3dnrBase> p3dnr = p2CamContext->get3dnr();
  if (NULL == p3dnr) {
    CAM_LOGW("no hal3dnr!");
    return ret;
  }

  if (mPipeUsage.is3DNRModeMaskEnable(
          NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT)) {
    isSupportV2Flow =
        ::property_get_int32("vendor.debug.3dnr.hal.v2", isSupportV2Flow);
  }

  if (isSupportV2Flow) {
    NR3DHALParam nr3dHalParam;

    // 3a related
#if (MTKCAM_ENABLE_IPC == 1)
    nr3dHalParam.pTuningData = reinterpret_cast<void*>(
        enqueParams->mvFrameParams[(MUINT32)masterIndex].mTuningData_fd);
#else
    nr3dHalParam.pTuningData =
        enqueParams->mvFrameParams[(MUINT32)masterIndex].mTuningData;
#endif
    nr3dHalParam.p3A = mp3A;

    // generic
    nr3dHalParam.frameNo = requestNo;
    nr3dHalParam.iso = iso;
    nr3dHalParam.isoThreshold = isoThreshold;

    // imgi related
    nr3dHalParam.isCRZUsed = request->isP2ACRZMode();
    nr3dHalParam.isIMGO = request->getVar<MBOOL>(VAR_IMGO_2IMGI_ENABLE, MFALSE);
    nr3dHalParam.isBinning = MFALSE;  // TODO(MTK): how to pass BinningInfo??

    // lmv related info
    nr3dHalParam.GMVInfo.gmvX = eisInfo.gmvX;
    nr3dHalParam.GMVInfo.gmvY = eisInfo.gmvY;
    nr3dHalParam.GMVInfo.x_int = eisInfo.x_int;
    nr3dHalParam.GMVInfo.y_int = eisInfo.y_int;
    nr3dHalParam.GMVInfo.confX = eisInfo.confX;
    nr3dHalParam.GMVInfo.confY = eisInfo.confY;

    // vipi related
    nr3dHalParam.pIMGBufferVIPI = pIMGBufferVIPI;

    // output related, ex: img3o
    nr3dHalParam.dst_resizer_rect = dst_resizer_rect;

    // gyro
    nr3dHalParam.gyroData =
        request->getVar<NR3D::GyroData>(VAR_3DNR_GYRO, NR3D::GyroData());

    ret = p3dnr->do3dnrFlow_v2(nr3dHalParam);
  } else {
#if (MTKCAM_ENABLE_IPC == 1)
    void* pIspPhyReg = reinterpret_cast<void*>(
        enqueParams->mvFrameParams[(MUINT32)masterIndex].mTuningData_fd);
#else
    void* pIspPhyReg = reinterpret_cast<void*>(
        enqueParams->mvFrameParams[(MUINT32)masterIndex].mTuningData);
#endif

    MBOOL useCMV = request->isP2ACRZMode();
    NR3DMVInfo GMVInfo;
    GMVInfo.gmvX = eisInfo.gmvX;
    GMVInfo.gmvY = eisInfo.gmvY;
    GMVInfo.x_int = eisInfo.x_int;
    GMVInfo.y_int = eisInfo.y_int;
    GMVInfo.confX = eisInfo.confX;
    GMVInfo.confY = eisInfo.confY;

    ret = p3dnr->do3dnrFlow(pIspPhyReg, useCMV, dst_resizer_rect, GMVInfo,
                            pIMGBufferVIPI, iso, requestNo, mp3A);
  }

  TRACE_FUNC_EXIT();
  CAM_LOGD("do3dnrFlow-");

  return ret;
}

MVOID P2ANode::dump_Qparam(const QParams& rParams, const char* pSep) {
  if (mPipeUsage.is3DNRModeMaskEnable(
          NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) == 0) {
    return;
  }

  // start dump process
  MINT32 enableOption =
      ::property_get_int32("vendor.camera.3dnr.dump.qparam", 0);
  if (enableOption) {
    return;
  }
  if (m3dnrLogLevel == 0) {
    return;
  }

  TRACE_FUNC_ENTER();
  MUINT32 i = 0;

  CAM_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mDequeSuccess: %d", pSep,
              rParams.mDequeSuccess);
  if (pSep != NULL && pSep[0] == 'd' && pSep[1] == 'd' &&
      rParams.mDequeSuccess == 0) {
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "%s_!!! QPARAM DEQUE  FAIL !!!", pSep);
    return;
  }

  CAM_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mpfnCallback: %p", pSep,
              rParams.mpfnCallback);
  CAM_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mpCookie: %p", pSep,
              rParams.mpCookie);

  CAM_LOGD_IF(m3dnrLogLevel >= 2,
              "%s_: rParams.mvFrameParams[0].mTuningData: %p", pSep,
              rParams.mvFrameParams[0].mTuningData);

  // mvIn
  CAM_LOGD_IF(m3dnrLogLevel >= 2,
              "%s_: rParams.mvFrameParams[0].mvIn.size(): %zu", pSep,
              rParams.mvFrameParams[0].mvIn.size());
  for (i = 0; i < rParams.mvFrameParams[0].mvIn.size(); ++i) {
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: --- rParams.mvIn[#%d]: start --- ",
                pSep, i);
    Input tmp = rParams.mvFrameParams[0].mvIn[i];

    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.index: %d", pSep, i,
                tmp.mPortID.index);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.type: %d", pSep, i,
                tmp.mPortID.type);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.inout: %d", pSep, i,
                tmp.mPortID.inout);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.group: %d", pSep, i,
                tmp.mPortID.group);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.capbility: %d",
                pSep, i, tmp.mPortID.capbility);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.reserved: %d", pSep,
                i, tmp.mPortID.reserved);

    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: Input.mBuffer: %p", pSep,
                tmp.mBuffer);

    if (tmp.mBuffer != NULL) {
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvIn[%d].mBuffer.getImgFormat(): %d", pSep, i,
                  tmp.mBuffer->getImgFormat());
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvIn[%d].mBuffer.getImgSize(): w=%d, h=%d", pSep, i,
                  tmp.mBuffer->getImgSize().w, tmp.mBuffer->getImgSize().h);
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvIn[%d].mBuffer.getImgBitsPerPixel(): %zu", pSep, i,
                  tmp.mBuffer->getImgBitsPerPixel());

      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvIn[%d].mBuffer.getPlaneCount(): %zu", pSep, i,
                  tmp.mBuffer->getPlaneCount());
      for (size_t k = 0; k < tmp.mBuffer->getPlaneCount(); ++k) {
        CAM_LOGD_IF(m3dnrLogLevel >= 2,
                    "\t%s_: mvIn[%d].mBuffer.getPlaneBitsPerPixel(%zu): %zu",
                    pSep, i, k, tmp.mBuffer->getPlaneBitsPerPixel(k));
      }
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvIn[%d].mBuffer.getBitstreamSize(): %zu", pSep, i,
                  tmp.mBuffer->getBitstreamSize());
    }

    CAM_LOGD_IF(m3dnrLogLevel >= 2, "%s_: --- rParams.mvIn[#%d]: end --- ",
                pSep, i);
  }

  // mvOut
  CAM_LOGD_IF(m3dnrLogLevel >= 2,
              "%s_: rParams.mvFrameParams[0].mvOut.size(): %zu", pSep,
              rParams.mvFrameParams[0].mvOut.size());
  for (i = 0; i < rParams.mvFrameParams[0].mvOut.size(); ++i) {
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: --- rParams.mvOut[#%d]: start --- ",
                pSep, i);
    Output tmp = rParams.mvFrameParams[0].mvOut[i];

    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.index: %d", pSep,
                i, tmp.mPortID.index);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.type: %d", pSep, i,
                tmp.mPortID.type);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.inout: %d", pSep,
                i, tmp.mPortID.inout);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.group: %d", pSep,
                i, tmp.mPortID.group);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.capbility: %d",
                pSep, i, tmp.mPortID.capbility);
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.reserved: %d",
                pSep, i, tmp.mPortID.reserved);

    CAM_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: Input.mBuffer: %p", pSep,
                tmp.mBuffer);

    if (tmp.mBuffer != NULL) {
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvOut[%d].mBuffer.getImgFormat(): %d", pSep, i,
                  tmp.mBuffer->getImgFormat());
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvOut[%d].mBuffer.getImgSize(): w=%d, h=%d", pSep, i,
                  tmp.mBuffer->getImgSize().w, tmp.mBuffer->getImgSize().h);
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvOut[%d].mBuffer.getImgBitsPerPixel(): %zu", pSep, i,
                  tmp.mBuffer->getImgBitsPerPixel());

      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvOut[%d].mBuffer.getPlaneCount(): %zu", pSep, i,
                  tmp.mBuffer->getPlaneCount());
      for (size_t k = 0; k < tmp.mBuffer->getPlaneCount(); ++k) {
        CAM_LOGD_IF(m3dnrLogLevel >= 2,
                    "\t%s_: mvOut[%d].mBuffer.getPlaneBitsPerPixel(%zu): %zu",
                    pSep, i, k, tmp.mBuffer->getPlaneBitsPerPixel(k));
      }
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "\t%s_: mvOut[%d].mBuffer.getBitstreamSize(): %zu", pSep, i,
                  tmp.mBuffer->getBitstreamSize());
    }

    CAM_LOGD_IF(m3dnrLogLevel >= 2, "%s_: --- rParams.mvOut[#%d]: end --- ",
                pSep, i);
  }

  TRACE_FUNC_EXIT();
}

MVOID P2ANode::dump_vOutImageBuffer(const QParams& params) {
  // === default values initialized ===
  static int num_img3o_frame_to_dump = 0;
  static int dumped_frame_count = 0;
  static int is_dump_complete = 1;
  static int dump_round_count = 1;
  // ==================================

  // start dump process
  if (num_img3o_frame_to_dump == 0 ||
      dumped_frame_count == num_img3o_frame_to_dump) {
    num_img3o_frame_to_dump = 0;
    if (mPipeUsage.is3DNRModeMaskEnable(
            NSCam::NR3D::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) != 0) {
      num_img3o_frame_to_dump =
          ::property_get_int32("vendor.camera.3dnr.dump.img3o", 0);
    }
    return;
  }

  // debug: start
  char vOut0_frame_str[64];
  char vOutIMG3O_frame_str[64];

  // start from scratch
  if (is_dump_complete) {
    is_dump_complete = 0;
    dumped_frame_count = 0;
  }

  if (dumped_frame_count < num_img3o_frame_to_dump) {
    int is_img3o_dumped = 0;
    CAM_LOGD_IF(m3dnrLogLevel >= 2, "mvOut size = %zu",
                params.mvFrameParams[0].mvOut.size());

    for (size_t i = 0; i < params.mvFrameParams[0].mvOut.size(); ++i) {
      if (i == 0) {
        if (params.mDequeSuccess != 0) {
          snprintf(vOut0_frame_str, sizeof(vOut0_frame_str),
                   "%s/vOut0_frame-r%.2d_%.3d_%dx%d_OK.yuv", DUMP_PATH,
                   dump_round_count, dumped_frame_count,
                   params.mvFrameParams[0].mvOut[0].mBuffer->getImgSize().w,
                   params.mvFrameParams[0].mvOut[0].mBuffer->getImgSize().h);
        } else {
          snprintf(vOut0_frame_str, sizeof(vOut0_frame_str),
                   "%s/vOut0_frame-r%.2d_%.3d_%dx%d_NG.yuv", DUMP_PATH,
                   dump_round_count, dumped_frame_count,
                   params.mvFrameParams[0].mvOut[0].mBuffer->getImgSize().w,
                   params.mvFrameParams[0].mvOut[0].mBuffer->getImgSize().h);
        }
        params.mvFrameParams[0].mvOut[0].mBuffer->saveToFile(vOut0_frame_str);
        CAM_LOGD_IF(m3dnrLogLevel >= 2, "params.mvOut[0] saved: %p",
                    params.mvFrameParams[0].mvOut[0].mBuffer);
      }

      if (params.mvFrameParams[0].mvOut[i].mPortID ==
          PortID(EPortType_Memory, EPortIndex_IMG3O, PORTID_OUT)) {
        if (params.mDequeSuccess != 0) {
          snprintf(vOutIMG3O_frame_str, sizeof(vOutIMG3O_frame_str),
                   "%s/vOutIMG3O_frame-r%.2d_%.3d_%dx%d_OK.yuv", DUMP_PATH,
                   dump_round_count, dumped_frame_count,
                   params.mvFrameParams[0].mvOut[i].mBuffer->getImgSize().w,
                   params.mvFrameParams[0].mvOut[i].mBuffer->getImgSize().h);
        } else {
          snprintf(vOutIMG3O_frame_str, sizeof(vOutIMG3O_frame_str),
                   "%s/vOutIMG3O_frame-r%.2d_%.3d_%dx%d_NG.yuv", DUMP_PATH,
                   dump_round_count, dumped_frame_count,
                   params.mvFrameParams[0].mvOut[i].mBuffer->getImgSize().w,
                   params.mvFrameParams[0].mvOut[i].mBuffer->getImgSize().h);
        }
        params.mvFrameParams[0].mvOut[i].mBuffer->saveToFile(
            vOutIMG3O_frame_str);
        CAM_LOGD_IF(m3dnrLogLevel >= 2,
                    "params.mvOut[%zu] EPortIndex_IMG3O saved: %p", i,
                    params.mvFrameParams[0].mvOut[i].mBuffer);
        is_img3o_dumped = 1;
      }
    }

    if (is_img3o_dumped == 0) {
      CAM_LOGD_IF(m3dnrLogLevel >= 2, "mkdbg: !!err: no IMG3O buffer dumped");
      CAM_LOGD_IF(m3dnrLogLevel >= 2, "mkdbg: !!err: no IMG3O buffer dumped");
    }
    ++dumped_frame_count;

    if (dumped_frame_count >= num_img3o_frame_to_dump) {
      // when the dump is complete...
      is_dump_complete = 1;
      num_img3o_frame_to_dump = 0;
      CAM_LOGD_IF(m3dnrLogLevel >= 2,
                  "dump round %.2d finished ... (dumped_frame_count=%d, "
                  "num_img3o_frame_to_dump =%d)",
                  dump_round_count++, dumped_frame_count,
                  num_img3o_frame_to_dump);
    }
  }
}

MVOID P2ANode::dump_imgiImageBuffer(const QParams&) {
  // Not implemented
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
