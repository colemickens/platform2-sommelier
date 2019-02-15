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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_MDPOPERATOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_MDPOPERATOR_H_
//
/*****************************************************************************
 * MdpOperator is derived from ImageTransform with specilized MDP usage
 * such as:
 * 1.Bayer12_UNPAK/Bayer14_UNPAK format support
 * 2.customization of src/dst buffer size or stride,
 * 3.make mdp porcess to single plane among multi-plane image buffers, etc...
 *****************************************************************************/

// Standard C header file
#include <string>

// Android system/core header file
#include <memory>
#include <mutex>

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <DpIspStream.h>

// Module header file

// Local header file

namespace VSDOF {
namespace util {

const char MY_NAME[] = "MdpOperator";

struct customConfig {
  MBOOL custStride = MFALSE;
  MSize size = MSize(0, 0);
  MINT32 planeIdx = -1;
};

struct MdpConfig {
  std::shared_ptr<IImageBuffer> pSrcBuffer = nullptr;
  std::shared_ptr<IImageBuffer> pDstBuffer = nullptr;
  MUINT32 trans = 0;

  customConfig srcCust;
  customConfig dstCust;

  // PQ parameters
  MBOOL usePQParams = MFALSE;
  MINT32 featureId = 0;
  MINT32 processId = 0;
};

class MdpOperator {
 public:
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  MdpOperator Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MdpOperator(const char* creatorName, MINT32 openId);

  virtual ~MdpOperator();

  MBOOL execute(MdpConfig const& config);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  RefBase Interface.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  MBOOL convertTransform(MUINT32 const u4Transform,
                         MUINT32 const& u4Rotation,
                         MUINT32 const& u4Flip);

  MBOOL mapDpFormat(NSCam::EImageFormat const fmt, DpColorFormat* dp_fmt);

  MBOOL configPort(MUINT32 const port,
                   std::shared_ptr<IImageBuffer> const pBufInfo,
                   MINT32 width,
                   MINT32 height,
                   MINT32 stride,
                   MINT32 specifiedPlane = -1,
                   EImageFormat specifiedFmt = eImgFmt_UNKNOWN);

  MBOOL enqueBuffer(MUINT32 const port,
                    std::shared_ptr<IImageBuffer> const pBufInfo,
                    MINT32 specifiedPlane = -1);

  MBOOL dequeDstBuffer(MUINT32 const port,
                       std::shared_ptr<IImageBuffer> const pBufInfo,
                       MINT32 specifiedPlane = -1);

  MVOID setPQParameters(MUINT32 port, MINT32 featureID, MINT32 processId);

 private:
  const char* mCreatorName;
  MINT32 miOpenId = -1;

  mutable std::mutex mLock;

  DpIspStream* mpStream = nullptr;
};

};      // namespace util
};      // namespace VSDOF
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_MDPOPERATOR_H_
