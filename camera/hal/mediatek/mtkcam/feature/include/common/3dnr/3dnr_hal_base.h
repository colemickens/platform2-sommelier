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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_INCLUDE_COMMON_3DNR_3DNR_HAL_BASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_INCLUDE_COMMON_3DNR_3DNR_HAL_BASE_H_

#include <mtkcam/def/common.h>
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/metadata/IMetadata.h>

#include <memory>

#define NR3D_WORKING_BUFF_WIDTH \
  2752  // (1920 * 6 / 5)  , this is for EIS limitation
#define NR3D_WORKING_BUFF_HEIGHT \
  2752  // (1080 * 6 / 5)  , this is for EIS limitation

namespace NS3Av3 {

class IHal3A;

};

namespace NSCam {

class IImageBuffer;

};
/**************************************************************************
 *
 ***************************************************************************/
namespace NSCam {
namespace NSIoPipe {
namespace NSPostProc {  // for NR3D* &pNr3dParam

typedef enum {
  NR3D_STATE_STOP = 0x00,  // NR3D, IMG3O, VIPI all disabled.
  NR3D_STATE_PREPARING =
      0x01,  // IMG3O enabled, to output current frame for next frame use. NR3D,
             // VIPI disabled. (When NR3D is disable, IMG30 output original
             // image without any process.)
  NR3D_STATE_WORKING = 0x02,  // NR3D, IMG3O, VIPI all enabled.
} NR3D_STATE_ENUM;

typedef enum {
  NR3D_ERROR_NONE = 0x00,  // No error.
  NR3D_ERROR_INVALID_GMV =
      0x01,  // GMV is invalid due to drop frame or something.
  NR3D_ERROR_GMV_TOO_LARGE =
      0x02,                      // GMV X or Y is larger than a certain value.
  NR3D_ERROR_DROP_FRAME = 0x04,  // Drop frame.
  NR3D_ERROR_FRAME_SIZE_CHANGED = 0x08,  // Current frame size is not the same
                                         // as previous frame. E.g. during DZ.
  NR3D_ERROR_FORCE_SKIP = 0x10,  // Force skip by user, probably by adb command.
  NR3D_ERROR_UNDER_ISO_THRESHOLD =
      0x11,                            // must > iso threshold to turn on 3dnrs
  NR3D_ERROR_NOT_SUPPORT = 0x12,       // not supported function
  NR3D_ERROR_INVALID_PARAM = 0x14,     // invalid parameter
  NR3D_ERROR_INPUT_SRC_CHANGE = 0x18,  //  input src change
} NR3D_ERROR_ENUM;

/***************************************************************************
 *
 * @struct NR3DParam
 * @brief parameter for NR3D HW setting
 * @details
 *
 ***************************************************************************/
struct NR3DParam {
 public:
  MUINT32 ctrl_onEn;
  MUINT32 onOff_onOfStX;
  MUINT32 onOff_onOfStY;
  MUINT32 onSiz_onWd;
  MUINT32 onSiz_onHt;
  MUINT32 vipi_offst;  // in byte
  MUINT32 vipi_readW;  // in pixel
  MUINT32 vipi_readH;  // in pixel

  NR3DParam()
      : ctrl_onEn(0x0),
        onOff_onOfStX(0x0),
        onOff_onOfStY(0x0),
        onSiz_onWd(0x0),
        onSiz_onHt(0x0),
        vipi_offst(0x0),
        vipi_readW(0x0),
        vipi_readH(0x0) {}
};

struct NR3DRSCInfo {
  MINTPTR pMV;
  MINTPTR pBV;  // size is rssoSize
  MSize rrzoSize;
  MSize rssoSize;
  MUINT32 staGMV;  // gmv value of RSC
  MBOOL isValid;

  NR3DRSCInfo() : pMV(NULL), pBV(NULL), staGMV(0), isValid(MFALSE) {}
};

struct NR3DHALParam {
 public:
  // 3a tuning buffer
  void* pTuningData;
  std::shared_ptr<NS3Av3::IHal3A> p3A;

  // frame generic
  MUINT32 frameNo;
  MINT32 iso;
  MINT32 isoThreshold;

  // imgi related
  MBOOL isCRZUsed;
  MBOOL isIMGO;
  MBOOL isBinning;  // TODO(mtk): future use

  // lmv related info
  NR3DRSCInfo RSCInfo;
  NR3D::NR3DMVInfo GMVInfo;

  // vipi related
  IImageBuffer* pIMGBufferVIPI;

  // output related, ex: img3o
  MRect dst_resizer_rect;

  NR3D::GyroData gyroData;

  NR3DHALParam()
      : pTuningData(NULL),
        p3A(NULL),
        frameNo(0),
        iso(0),
        isoThreshold(0)
        // imgi related
        ,
        isCRZUsed(MFALSE),
        isIMGO(MFALSE),
        isBinning(MFALSE)
        // vipi related
        ,
        pIMGBufferVIPI(NULL)
        // output related, ex: img3o
        ,
        dst_resizer_rect(0, 0) {}
};

class VISIBILITY_PUBLIC hal3dnrBase
    : public std::enable_shared_from_this<hal3dnrBase> {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  //
  static std::shared_ptr<hal3dnrBase> createInstance();  // deprecated
  static std::shared_ptr<hal3dnrBase> createInstance(char const* userName,
                                                     const MUINT32 sensorIdx);
  virtual ~hal3dnrBase() {}

  virtual MBOOL init(MINT32 force3DNR = 0) = 0;
  virtual MBOOL uninit() = 0;
  virtual MBOOL prepare(MUINT32 frameNo, MINT32 iso) = 0;
  virtual MVOID setCMVMode(MBOOL useCMV) = 0;
  virtual MBOOL setGMV(MUINT32 frameNo,
                       MINT32 gmvX,
                       MINT32 gmvY,
                       MINT32 cmvX_Int,
                       MINT32 cmvY_Int) = 0;
  virtual MBOOL checkIMG3OSize(MUINT32 frameNo,
                               MUINT32 imgiW,
                               MUINT32 imgiH) = 0;
  virtual MBOOL setVipiParams(MBOOL isVIPIIn,
                              MUINT32 vipiW,
                              MUINT32 vipiH,
                              MINT imgFormat,
                              size_t stride) = 0;
  virtual MBOOL get3dnrParams(
      MUINT32 frameNo,
      MUINT32 imgiW,
      MUINT32 imgiH,
      std::shared_ptr<NR3DParam>* pNr3dParam) = 0;  // deprecated
  virtual MBOOL get3dnrParams(MUINT32 frameNo,
                              MUINT32 imgiW,
                              MUINT32 imgiH,
                              std::shared_ptr<NR3DParam> nr3dParam) = 0;
  virtual MBOOL checkStateMachine(NR3D_STATE_ENUM status) = 0;

  // warp several APIs into one simple API
  virtual MBOOL do3dnrFlow(void* pTuningData,
                           MBOOL useCMV,
                           MRect const& dst_resizer_rect,
                           NR3D::NR3DMVInfo const& GMVInfo,
                           NSCam::IImageBuffer* pIMGBufferVIPI,
                           MINT32 iso,
                           MUINT32 requestNo,
                           std::shared_ptr<NS3Av3::IHal3A> p3A) = 0;

  // warp several APIs into one simple API
  virtual MBOOL do3dnrFlow_v2(const NR3DHALParam& nr3dHalParam) = 0;

  virtual MBOOL updateISPMetadata(
      NSCam::IMetadata* pMeta_InHal,
      const NSCam::NR3D::NR3DTuningInfo& tuningInfo) = 0;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Variables.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
};
}  // namespace NSPostProc
}  // namespace NSIoPipe
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_INCLUDE_COMMON_3DNR_3DNR_HAL_BASE_H_
