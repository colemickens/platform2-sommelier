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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREPLUGIN_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREPLUGIN_H_

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file

// Module header file
#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
// Local header file
#include "CaptureFeatureRequest.h"
#include <memory>

/**********************************************************************
 * Namespace start.
 ***************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

/********************************************************************
 * Class Define
 *********************************************************************/
/**
 * @brief implemented class of the NSPipelinePlugin::BufferHandle
 */
class PluginBufferHandle final : public NSPipelinePlugin::BufferHandle {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  PluginBufferHandle(std::shared_ptr<CaptureFeatureNodeRequest> pNodeRequest,
                     BufferID_T bufId);

  ~PluginBufferHandle();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  NSPipelinePlugin::BufferHandle Public Operators.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  IImageBuffer* acquire(MINT usage) override;

  MVOID release() override;

  MVOID dump(std::ostream& os) const override;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  PluginBufferHandle Private Data Members.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  std::weak_ptr<CaptureFeatureNodeRequest> mpNodeRequest;
  IImageBuffer* mpImageBuffer;
  BufferID_T mBufferId;
};

/**
 * @brief implemented class of the NSPipelinePlugin::MetadataHandle
 */
class PluginMetadataHandle : public NSPipelinePlugin::MetadataHandle {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  PluginMetadataHandle(std::shared_ptr<CaptureFeatureNodeRequest> pNodeRequest,
                       MetadataID_T metaId);

  ~PluginMetadataHandle();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  NSPipelinePlugin::MetadataHandle Public Operators.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  IMetadata* acquire() override;

  MVOID release() override;

  MVOID dump(std::ostream& os) const override;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  PluginMetadataHandle Private Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  std::weak_ptr<CaptureFeatureNodeRequest> mpNodeRequest;
  IMetadata* mpMetadata;
  MetadataID_T mMetaId;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREPLUGIN_H_
