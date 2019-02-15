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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ICAPTUREFEATUREPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ICAPTUREFEATUREPIPE_H_

#include <memory>
//
#include <mtkcam/def/common.h>
#include <mtkcam/pipeline/hwnode/P2Common.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/metadata/IMetadata.h>

using NSCam::v3::P2Common::StreamConfigure;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class ICaptureFeatureRequest;
class RequestCallback;

class VISIBILITY_PUBLIC ICaptureFeaturePipe {
 public:
  enum eUsageMode {
    USAGE_TIME_SHARING,
    USAGE_FULL,
  };

  class UsageHint {
   public:
    UsageHint();
    explicit UsageHint(eUsageMode mode);

    eUsageMode mMode = USAGE_FULL;
  };

 public:
  // interface for PipelineNode

  static std::shared_ptr<ICaptureFeaturePipe> createInstance(
      MINT32 sensorIndex, const UsageHint& usageHint);

  virtual MVOID init() = 0;

  virtual MBOOL config(const StreamConfigure config) = 0;

  virtual MVOID uninit() = 0;

  virtual MERROR enque(std::shared_ptr<ICaptureFeatureRequest>) = 0;

  virtual MVOID setCallback(std::shared_ptr<RequestCallback>) = 0;

  virtual MBOOL flush() = 0;

  virtual std::shared_ptr<ICaptureFeatureRequest> acquireRequest() = 0;

  virtual MVOID releaseRequest(std::shared_ptr<ICaptureFeatureRequest>) = 0;

  virtual ~ICaptureFeaturePipe() {}

 protected:
  ICaptureFeaturePipe() {}
};

enum CaptureBufferID {
  BID_MAN_IN_FULL,
  BID_MAN_IN_RSZ,
  BID_MAN_IN_LCS,
  BID_MAN_IN_YUV,
  BID_MAN_OUT_YUV00,
  BID_MAN_OUT_YUV01,
  BID_MAN_OUT_JPEG,
  BID_MAN_OUT_POSTVIEW,
  BID_MAN_OUT_THUMBNAIL,
  BID_MAN_OUT_DEPTH,
  BID_MAN_OUT_CLEAN,
  BID_SUB_IN_FULL,
  BID_SUB_IN_RSZ,
  BID_SUB_IN_LCS,
  BID_SUB_OUT_YUV00,
  BID_SUB_OUT_YUV01,
  NUM_OF_BUFFER,
  NULL_BUFFER = 0xFF,
};

enum CaptureMetadataID {
  MID_MAN_IN_P1_DYNAMIC,
  MID_MAN_IN_APP,
  MID_MAN_IN_HAL,
  MID_MAN_OUT_APP,
  MID_MAN_OUT_HAL,
  MID_SUB_IN_P1_DYNAMIC,
  MID_SUB_IN_HAL,
  MID_SUB_OUT_APP,
  MID_SUB_OUT_HAL,
  NUM_OF_METADATA,
  NULL_METADATA = 0xFF,
};

enum CaptureFeatureFeatureID {
  FID_REMOSAIC,
  FID_ABF,
  FID_NR,
  FID_MFNR,
  FID_FB,
  FID_HDR,
  FID_DEPTH,
  FID_BOKEH,
  FID_FUSION,
  FID_CZ,
  FID_DRE,
  FID_FB_3RD_PARTY,
  FID_MFNR_3RD_PARTY,
  FID_HDR_3RD_PARTY,
  FID_HDR2_3RD_PARTY,
  FID_DEPTH_3RD_PARTY,
  FID_BOKEH_3RD_PARTY,
  FID_FUSION_3RD_PARTY,
  NUM_OF_FEATURE,
  NULL_FEATURE = 0xFF,
};

enum CaptureFeatureParameterID {
  PID_REQUEST_NUM,
  PID_FRAME_NUM,
  PID_FRAME_INDEX,
  PID_FRAME_COUNT,
  PID_MAIN_FRAME,
  PID_ENABLE_MFB,
  PID_ENABLE_HDR,
  PID_ENABLE_NEXT_CAPTURE,
  NUM_OF_PARAMETER,
};

typedef MUINT8 BufferID_T;
typedef MUINT8 MetadataID_T;
typedef MUINT8 FeatureID_T;
typedef MUINT8 ParameterID_T;

class VISIBILITY_PUBLIC MetadataHandle {
 public:
  /*
   * Acquire the pointer of locked metadata
   *
   * @return the pointer of metadata
   */
  virtual MERROR acquire() = 0;

  virtual NSCam::IMetadata* native() = 0;
  /*
   * Release the metadata to the caller
   */
  virtual void release() = 0;

  virtual ~MetadataHandle() {}
};

class VISIBILITY_PUBLIC BufferHandle {
 public:
  /*
   * Acquire the pointer of locked image buffer
   *
   * @param[in] usage: the buffer usage
   * @return the pointer of image buffer
   */
  virtual MERROR acquire(MINT usage = eBUFFER_USAGE_HW_CAMERA_READWRITE |
                                      eBUFFER_USAGE_SW_READ_OFTEN) = 0;

  virtual NSCam::IImageBuffer* native() = 0;
  /*
   * Release the metadata to the caller
   */
  virtual void release() = 0;

  virtual MUINT32 getTransform() = 0;

  virtual ~BufferHandle() {}
};

class ICaptureFeatureRequest {
 public:
  /*
   * Add a buffer handle into request
   *
   * @param[in] buffer Id: the named buffer
   * @param[in] buffer handle: the buffer handle
   */
  virtual MVOID addBuffer(BufferID_T, std::shared_ptr<BufferHandle>) = 0;

  virtual std::shared_ptr<BufferHandle> getBuffer(BufferID_T) = 0;

  virtual MVOID addParameter(ParameterID_T, MINT32) = 0;

  virtual MINT32 getParameter(ParameterID_T) = 0;

  /*
   * Add a metadata handle into request
   *
   * @param[in] metadata Id: the named metadata
   * @param[in] metadata handle: the metadata handle
   */
  virtual MVOID addMetadata(MetadataID_T, std::shared_ptr<MetadataHandle>) = 0;

  virtual std::shared_ptr<MetadataHandle> getMetadata(MetadataID_T) = 0;
  /*
   * Apply a feature into output result
   *
   * @param[in] feature Id: the named feature
   */
  virtual MVOID addFeature(FeatureID_T) = 0;
  virtual MVOID setFeatures(MUINT64) = 0;

  //    virtual MVOID                   setRequestNo(MINT32);
  virtual MINT32 getRequestNo() = 0;

 private:
};

class RequestCallback {
 public:
  /*
   * ready to accept the next request to process
   *
   * @param[in] req: the request pointer
   */
  virtual void onContinue(std::shared_ptr<ICaptureFeatureRequest>) = 0;

  /*
   * Cancel a request which have sent to plugin successfully
   *
   * @param[in] req: the request pointer
   */
  virtual void onAborted(std::shared_ptr<ICaptureFeatureRequest>) = 0;

  /*
   * Notify a completed result and request result
   *
   * @param[in] req: the request pointer
   * @param[in] err: result status
   */
  virtual void onCompleted(std::shared_ptr<ICaptureFeatureRequest>, MERROR) = 0;

  virtual ~RequestCallback() {}
};

};  // namespace NSCapture
};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#undef MAKE_FEATURE_MASK_FUNC

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ICAPTUREFEATUREPIPE_H_
