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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURE_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURE_COMMON_H_

#include "MtkHeader.h"
#include <featurePipe/common/include/ImageBufferPool.h>

#include "StreamingFeatureData.h"
#include "DebugControl.h"
#include <system/graphics.h>
#include <vector>

#define SUPPORT_HAL3

#ifdef SUPPORT_HAL3
#define BOOL_SUPPORT_HAL3 (true)
#else
#define BOOL_SUPPORT_HAL3 (false)
#endif

#define UHD_VR_WIDTH 3840
#define UHD_VR_HEIGHT 2160
#define MAX_FULL_WIDTH 4608   // (3840x1.2)
#define MAX_FULL_HEIGHT 2612  // (2176x1.2)
#define DS_IMAGE_WIDTH 320
#define DS_IMAGE_HEIGHT 320
#define MAX_WARP_WIDTH 320
#define MAX_WARP_HEIGHT 320

#define MAX_FULL_SIZE (MSize(MAX_FULL_WIDTH, MAX_FULL_HEIGHT))
#define DS_IMAGE_SIZE (MSize(DS_IMAGE_WIDTH, DS_IMAGE_HEIGHT))
#define MAX_WARP_SIZE (MSize(MAX_WARP_WIDTH, MAX_WARP_HEIGHT))

#define IMG2O_CROP_GROUP 1
#define WDMAO_CROP_GROUP 2
#define WROTO_CROP_GROUP 3

#define PORTID_IN 0
#define PORTID_OUT 1

#define MPoint_STR "%d,%d"
#define MPoint_ARG(p) p.x, p.y
#define MSize_STR "%dx%d"
#define MSize_ARG(s) s.w, s.h
#define MCrpRsInfo_STR \
  "groupID=%d frameGroup=%d i(%dx%d) f(%dx%d) s(%dx%d) r(%dx%d)"
#define MCrpRsInfo_ARG(c)                                             \
  c.mGroupID, c.mFrameGroup, MPoint_ARG(c.mCropRect.p_integral),      \
      MPoint_ARG(c.mCropRect.p_fractional), MSize_ARG(c.mCropRect.s), \
      MSize_ARG(c.mResizeDst)
#define ModuleInfo_STR                  \
  "in(%dx%d) crop(%" PRIu32 "x%" PRIu32 \
  ")"                                   \
  "crop_start=(%d.%" PRIu64 ", %d.%" PRIu64 ") out(%dx%d)"
#define ModuleInfo_ARG(md)                                                 \
  md->in_w, md->in_h, md->crop_w, md->crop_h, md->crop_x, md->crop_floatX, \
      md->crop_y, md->crop_floatY, md->out_w, md->out_h
#define ExtraParam_FE_STR                                                    \
  "DSCR_SBIT=%d TH_C=%d TH_G=%d FLT_EN=%d PARAM=%d MODE=%d YIDX=%d XIDX=%d " \
  "START_X=%d START_Y=%d IN_HT=%d IN_WD=%d"
#define ExtraParam_FE_ARG(ext)                                   \
  ext->mFEDSCR_SBIT, ext->mFETH_C, ext->mFETH_G, ext->mFEFLT_EN, \
      ext->mFEPARAM, ext->mFEMODE, ext->mFEYIDX, ext->mFEXIDX,   \
      ext->mFESTART_X, ext->mFESTART_Y, ext->mFEIN_HT, ext->mFEIN_WD
#define ExtraParam_FM_STR                                                      \
  "HEIGHT=%d WIDTH=%d SR_TYPE=%d OFFSET_X=%d OFFSET_Y=%d RES_TH=%d SAD_TH=%d " \
  "MIN_RATIO=%d"
#define ExtraParam_FM_ARG(ext)                                      \
  ext->mFMHEIGHT, ext->mFMWIDTH, ext->mFMSR_TYPE, ext->mFMOFFSET_X, \
      ext->mFMOFFSET_Y, ext->mFMRES_TH, ext->mFMSAD_TH, ext->mFMMIN_RATIO
#define DpPqParam_ClearZoomParam_STR "cz.captureShot=%d cz.p_customSetting=%p "
#define DpPqParam_ClearZoomParam_ARG(cz) cz.captureShot, cz.p_customSetting
#define DpPqParam_DREParam_STR                                       \
  "dre.cmd=%d dre.userId=%llu dre.buffer=%p dre.p_customSetting=%p " \
  "dre.customIndex=%d "
#define DpPqParam_DREParam_ARG(dre) \
  dre.cmd, dre.userId, dre.buffer, dre.p_customSetting, dre.customIndex
#define DpPqParam_ISPParam_STR "isp.iso=%d isp.LV=%d isp.p_faceInfor=%p "
#define DpPqParam_ISPParam_ARG(isp) isp.iso, isp.LV, isp.p_faceInfor
#define DpPqParam_STR                                                          \
  "scenario=%d enable=%d " DpPqParam_ISPParam_STR DpPqParam_ClearZoomParam_STR \
      DpPqParam_DREParam_STR
#define DpPqParam_ARG(pq)                                      \
  pq->scenario, pq->enable, DpPqParam_ISPParam_ARG(pq->u.isp), \
      DpPqParam_ClearZoomParam_ARG(pq->u.isp.clearZoomParam),  \
      DpPqParam_DREParam_ARG(pq->u.isp.dpDREParam)
#define ExtraParam_PQ_STR "pqParam=%p pqParam.WDMA=%p pqParam.WROT=%p"
#define ExtraParam_PQ_ARG(ext) ext, ext->WDMAPQParam, ext->WROTPQParam
#define ExtraParam_CRSPINFO_STR "OFFSET_X=%d OFFSET_Y=%d WIDTH=%d HEIGHT=%d"
#define ExtraParam_CRSPINFO_ARG(ext)                          \
  ext->m_CrspInfo.p_integral.x, ext->m_CrspInfo.p_integral.y, \
      ext->m_CrspInfo.s.w, ext->m_CrspInfo.s.h

#define MCropRect_STR "(%dx%d)@(%d,%d)"
#define MCropRect_ARG(r) r.s.w, r.s.h, r.p_integral.x, r.p_integral.y

using NSCam::NSIoPipe::EPortType_Memory;
using NSCam::NSIoPipe::MCropRect;
using NSCam::NSIoPipe::MCrpRsInfo;
using NSCam::NSIoPipe::PortID;
using NSCam::NSIoPipe::QParams;

// check later
typedef void* NB_SPTR;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum IO_TYPE {
  IO_TYPE_DISPLAY,
  IO_TYPE_RECORD,
  IO_TYPE_EXTRA,
  IO_TYPE_FD,
  IO_TYPE_PHYSICAL,
};

class FrameInInfo {
 public:
  MSize inSize;
  MINT64 timestamp = 0;
};

const char* Fmt2Name(MINT fmt);
MBOOL useMDPHardware();
MINT32 getCropGroupID(const NSCam::NSIoPipe::PortID& portID);
MINT32 getCropGroupIDByIndex(MUINT32 port);
MBOOL getFrameInInfo(FrameInInfo* info,
                     const NSCam::NSIoPipe::FrameParams& frame,
                     MUINT32 port = NSImageio::NSIspio::EPortIndex_IMGI);

MBOOL isTargetOutput(MUINT32 target, const NSCam::NSIoPipe::Output& output);
MBOOL isDisplayOutput(const NSCam::NSIoPipe::Output& output);
MBOOL isRecordOutput(const NSCam::NSIoPipe::Output& output);
MBOOL isExtraOutput(const NSCam::NSIoPipe::Output& output);
MBOOL isFDOutput(const NSCam::NSIoPipe::Output& output);

MBOOL findUnusedMDPPort(const NSCam::NSIoPipe::QParams& qparam,
                        unsigned* index);
MBOOL findUnusedMDPCropGroup(const NSCam::NSIoPipe::QParams& qparam,
                             unsigned* cropGroup);

IImageBuffer* findInBuffer(const NSCam::NSIoPipe::QParams& qparam,
                           MUINT32 port = NSImageio::NSIspio::EPortIndex_IMGI);
IImageBuffer* findInBuffer(const NSCam::NSIoPipe::FrameParams& param,
                           MUINT32 port = NSImageio::NSIspio::EPortIndex_IMGI);
IImageBuffer* findOutBuffer(const NSCam::NSIoPipe::QParams& qparam,
                            unsigned skip);
IImageBuffer* findOutBuffer(const NSCam::NSIoPipe::FrameParams& param,
                            unsigned skip);

MSize toValid(const MSize& size, const MSize& def);
NSCam::EImageFormat toValid(const NSCam::EImageFormat fmt,
                            const NSCam::EImageFormat def);
const char* toName(NSCam::EImageFormat fmt);
#if MTK_DP_ENABLE
MBOOL toDpColorFormat(const NSCam::EImageFormat fmt, DpColorFormat* dpFmt);
#endif
int toPixelFormat(NSCam::EImageFormat fmt);

MBOOL copyImageBuffer(IImageBuffer* src, IImageBuffer* dst);

NB_SPTR getGraphicBufferAddr(IImageBuffer* imageBuffer);

MBOOL getOutBuffer(const NSCam::NSIoPipe::QParams& qparam,
                   IO_TYPE target,
                   NSCam::NSIoPipe::Output* output);

// Buffer Description by SFPIOMap
MBOOL getOutBuffer(const SFPIOMap& ioMap, IO_TYPE target, SFPOutput* output);
MBOOL getOutBuffer(const SFPIOMap& ioMap,
                   IO_TYPE target,
                   std::vector<SFPOutput>* outList);
MBOOL existOutBuffer(const std::vector<SFPIOMap>& sfpIOList, IO_TYPE target);
MBOOL existOutBuffer(const SFPIOMap& sfpIO, IO_TYPE target);
MBOOL isTypeMatch(PathType pathT, IO_TYPE ioType);
MBOOL isDisplayOutput(const SFPOutput& output);
MBOOL isRecordOutput(const SFPOutput& output);
MBOOL isExtraOutput(const SFPOutput& output);
MBOOL isFDOutput(const SFPOutput& output);
MBOOL isPhysicalOutput(const SFPOutput& output);

MBOOL parseIO(MUINT32 sensorID,
              const NSCam::NSIoPipe::FrameParams& param,
              const VarMap& varMap,
              SFPIOMap* ioMap,
              SFPIOManager* ioMgr);

MVOID pushSFPOutToMdp(NSCam::NSIoPipe::FrameParams* f,
                      const NSCam::NSIoPipe::PortID& portID,
                      const SFPOutput& output);
#if 1  // MTK_DP_ENABLE
MBOOL prepareMdpFrameParam(NSCam::NSIoPipe::QParams* qparam,
                           MUINT32 frameInd,
                           const std::vector<SFPOutput>& mdpOuts);
MBOOL prepareOneMdpFrameParam(NSCam::NSIoPipe::FrameParams* frame,
                              const std::vector<SFPOutput>& mdpOuts,
                              std::vector<SFPOutput>* remainList);
#endif
MSize calcDsImgSize(const MSize& src);

MBOOL dumpToFile(IImageBuffer* buffer, const char* fmt, ...);

MBOOL is4K2K(const MSize& size);

MUINT32 align(MUINT32 val, MUINT32 bits);

MVOID moveAppend(std::vector<SFPOutput>* source, std::vector<SFPOutput>* dest);

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURE_COMMON_H_
