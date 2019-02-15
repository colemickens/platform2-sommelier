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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FACEDETECTION_FD_HAL_BASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FACEDETECTION_FD_HAL_BASE_H_

#include <faces.h>
#include <mtkcam/def/BuiltinTypes.h>
#include <memory>
#include <mtkcam/def/common.h>
/******************************************************************************
 *
 ******************************************************************************/

#define ENABLE_MTK_ASD 0
#define ENABLE_MTK_GD 0

/******************************************************************************
 *
 ******************************************************************************/
enum HalFDMode_e {
  HAL_FD_MODE_FD = 0,
  HAL_FD_MODE_SD,
  HAL_FD_MODE_VFB,
  HAL_FD_MODE_CFB,
  HAL_FD_MODE_MANUAL
};

enum HalFDObject_e {
  HAL_FD_OBJ_NONE = 0,
  HAL_FD_OBJ_SW,
  HAL_FD_OBJ_HW,
  HAL_FD_OBJ_FDFT_SW,
  HAL_FD_OBJ_UNKNOWN = 0xFF
};

enum HalFDVersion_e {
  HAL_FD_VER_NONE = 0,
  HAL_FD_VER_HW36,
  HAL_FD_VER_HW37,
  HAL_FD_VER_SW36,
  HAL_FD_VER_HW40,
  HAL_FD_VER_HW41,
  HAL_FD_VER_HW42,
  HAL_FD_VER_HW43,
  HAL_FD_VER_HW50
};

struct FD_RESULT {
  MINT32 rect[4];
  MINT32 score;
  MINT32 rop_dir;
  MINT32 rip_dir;
};

struct FD_Frame_Parameters {
  MUINT8* pScaleImages;
  MUINT8* pRGB565Image;
  MUINT8* pPureYImage;
  MUINT8* pImageBufferPhyP0;  // Plane 0 of preview image physical address
  MUINT8* pImageBufferPhyP1;  // Plane 1 of preview image physical address
  MUINT8* pImageBufferPhyP2;  // Plane 2 of preview image physical address
  MUINT8* pImageBufferVirtual;
  MINT32 Rotation_Info;
  MUINT8 SDEnable;
  MUINT8 AEStable;
  MUINT8 padding_w;
  MUINT8 padding_h;
  int mem_fd;
};

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC halFDBase {
 public:
  //
  static std::shared_ptr<halFDBase> createInstance(HalFDObject_e eobject,
                                                   int openId = 0);

  virtual ~halFDBase() {}
  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDInit () -
  //! \brief init face detection
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDInit(MUINT32 /*fdW*/,
                           MUINT32 /*fdH*/,
                           MBOOL /*SWResizerEnable*/,
                           MUINT8 /*Current_mode*/,
                           MINT32 FldNum = 1) {
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  //
  // halFDGetVersion () -
  //! \brief get FD version
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDGetVersion() { return HAL_FD_VER_NONE; }

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDDo () -
  //! \brief process face detection
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDDo(struct FD_Frame_Parameters const&) { return 0; }

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDUninit () -
  //! \brief fd uninit
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDUninit() { return 0; }

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDGetFaceInfo () -
  //! \brief get face detection result
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDGetFaceInfo(MtkCameraFaceMetadata* /*fd_info_result*/) {
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDGetFaceResult () -
  //! \brief get face detection result
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDGetFaceResult(MtkCameraFaceMetadata* /*fd_result*/,
                                    MINT32 ResultMode = 1) {
    (void)ResultMode;
    return 0;
  }

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalSDGetSmileResult () -
  //! \brief get smile detection result
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halSDGetSmileResult() { return 0; }

  /////////////////////////////////////////////////////////////////////////
  //
  // halFDYUYV2ExtractY () -
  //! \brief create Y Channel
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDYUYV2ExtractY(MUINT8* /*dstAddr*/,
                                    MUINT8* /*srcAddr*/,
                                    MUINT32 /*src_width*/,
                                    MUINT32 /*src_height*/) {
    return 0;
  }
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FACEDETECTION_FD_HAL_BASE_H_
