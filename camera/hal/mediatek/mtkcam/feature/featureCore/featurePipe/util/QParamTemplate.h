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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_QPARAMTEMPLATE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_QPARAMTEMPLATE_H_

#include <mtkcam/def/common.h>
#include <mtkcam/drv/iopipe/Port.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/std/Trace.h>
#include <INormalStream.h>

#include <memory>
#include <vector>

// using NSCam::NSIoPipe;
using NSCam::NSIoPipe::EPostProcCmdIndex;
using NSCam::NSIoPipe::FrameParams;
using NSCam::NSIoPipe::PortID;
using NSCam::NSIoPipe::QParams;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

/*******************************************************************************
 * Enum Definition
 *****************************************************************************/
enum eCropGroup { eCROP_CRZ = 1, eCROP_WDMA = 2, eCROP_WROT = 3 };

/*******************************************************************************
 * Class Definition
 *****************************************************************************/

/**
 * @class QParamTemplateGenerator
 * @brief Store the location information of mvIn, mvOut, mvCropRsInfo for each
 *frame
 **/
class VISIBILITY_PUBLIC QParamTemplateGenerator {
 private:
  QParamTemplateGenerator();
  MBOOL checkValid();
  MVOID debug(QParams const& rQParam);

 public:
  QParamTemplateGenerator(MUINT32 frameID,
                          MUINT32 iSensorIdx,
                          v4l2::ENormalStreamTag streamTag);
  QParamTemplateGenerator& addCrop(eCropGroup groupID,
                                   MPoint startLoc,
                                   MSize cropSize,
                                   MSize resizeDst,
                                   bool isMDPCrop = false);
  QParamTemplateGenerator& addInput(PortID portID);
  QParamTemplateGenerator& addOutput(PortID portID, MINT32 transform = 0);
  QParamTemplateGenerator& addModuleInfo(MUINT32 moduleTag,
                                         MVOID* moduleStruct);
  QParamTemplateGenerator& addExtraParam(EPostProcCmdIndex cmdIdx,
                                         MVOID* param);
  MBOOL generate(QParams* rQParamTmpl);

 public:
  MUINT32 miFrameID;
  FrameParams mPass2EnqueFrame;
};

/**
 * @class QParamTemplateFiller
 * @brief Fill the corresponding input/output/tuning buffer and configure crop
 *information
 **/
class VISIBILITY_PUBLIC QParamTemplateFiller {
 public:
  explicit QParamTemplateFiller(QParams* target);
  QParamTemplateFiller& insertTuningBuf(MUINT frameID, MVOID* pTuningBuf);
  QParamTemplateFiller& insertInputBuf(MUINT frameID,
                                       PortID portID,
                                       IImageBuffer* pImggBuf);
  QParamTemplateFiller& insertOutputBuf(MUINT frameID,
                                        PortID portID,
                                        IImageBuffer* pImggBuf);
  QParamTemplateFiller& setCrop(MUINT frameID,
                                eCropGroup groupID,
                                MPoint startLoc,
                                MSize cropSize,
                                MSize resizeDst,
                                bool isMDPCrop = false);
  QParamTemplateFiller& setCropResize(MUINT frameID,
                                      eCropGroup groupID,
                                      MSize resizeDst);
  QParamTemplateFiller& setExtOffset(MUINT frameID,
                                     PortID portID,
                                     MINT32 offsetInBytes);
  QParamTemplateFiller& setInfo(MUINT frameID,
                                MUINT32 frameNo,
                                MUINT32 requestNo,
                                MUINT32 timestamp);
  QParamTemplateFiller& delOutputPort(MUINT frameID,
                                      PortID portID,
                                      eCropGroup cropGID);
  QParamTemplateFiller& delInputPort(MUINT frameID, PortID portID);
  /**
   * Validate the template filler status
   *
   * @return
   *  -  true if successful; otherwise false.
   **/
  MBOOL validate();

 public:
  QParams& mTargetQParam;
  MBOOL mbSuccess;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_QPARAMTEMPLATE_H_
