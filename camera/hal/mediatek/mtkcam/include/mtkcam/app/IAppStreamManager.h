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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_APP_IAPPSTREAMMANAGER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_APP_IAPPSTREAMMANAGER_H_
//
#include <map>
#include <memory>
#include <vector>
//
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/def/common.h>
#include "Cam3ImageStreamInfo.h"
#include "AppStreamBuffers.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * An interface of App stream manager.
 */
class VISIBILITY_PUBLIC IAppStreamManager {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Definitions.
  typedef Cam3ImageStreamInfo AppImageStreamInfo;

  typedef NSCam::v3::Utils::MetaStreamInfo AppMetaStreamInfo;

  typedef NSCam::v3::AppImageStreamBuffer AppImageStreamBuffer;

  typedef NSCam::v3::AppMetaStreamBuffer AppMetaStreamBuffer;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Definitions.
  struct Request {
    /*****
     * Assigned by App Stream Manager (during createRequest).
     */

    /**
     * @param frame number.
     */
    MUINT32 frameNo;

    /**
     * @param input image stream buffers.
     */
    std::map<StreamId_T, std::shared_ptr<AppImageStreamBuffer>>
        vInputImageBuffers;

    /**
     * @param output image stream buffers.
     */
    std::map<StreamId_T, std::shared_ptr<AppImageStreamBuffer>>
        vOutputImageBuffers;

    /**
     * @param input meta stream buffers.
     */
    std::map<StreamId_T, std::shared_ptr<IMetaStreamBuffer>> vInputMetaBuffers;

    /*****
     * Assigned out of App Stream Manager.
     */

    /**
     * @param output meta stream buffers.
     *  Note that the number of output meta streams equals to the number of
     *  partial meta result callbacks.
     */
    std::map<StreamId_T, std::shared_ptr<IMetaStreamBuffer>> vOutputMetaBuffers;
  };

  struct ConfigAppStreams {
    /**
     * @param image streams.
     */
    std::map<StreamId_T, std::shared_ptr<IImageStreamInfo>> vImageStreams;

    /**
     * @param stream min frame duration.
     */
    std::map<StreamId_T, MINT64> vMinFrameDuration;

    /**
     * @param stream stall frame duration.
     */
    std::map<StreamId_T, MINT64> vStallFrameDuration;

    /**
     * @param meta streams.
     */
    std::map<StreamId_T, std::shared_ptr<IMetaStreamInfo>> vMetaStreams;
  };

  /**
   * Update a given result frame.
   *
   */
  struct UpdateResultParams {
    /**
     * @param[in] the frame number to update.
     */
    uint32_t frameNo = 0;

    /**
     * @param[in] user id.
     *  In fact, it is pipeline node id from the viewpoint of pipeline
     * implementation, but the pipeline users have no such knowledge.
     */
    intptr_t userId = 0;

    /**
     * @param[in] hasLastPartial: contain last partial metadata in result
     * partial metadata vector.
     */
    bool hasLastPartial = false;

    /**
     * @param[in] result partial metadata to update.
     */
    std::vector<std::shared_ptr<IMetaStreamBuffer>> resultMeta;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  /**
   * Create an instance.
   */
  static std::shared_ptr<IAppStreamManager> create(
      MINT32 openId,
      camera3_callback_ops const* callback_ops,
      std::shared_ptr<IMetadataProvider> pMetadataProvider,
      MBOOL isDumpOutputInfo = false);

  /**
   * Destroy the instance.
   */
  virtual MVOID destroy() = 0;

  /**
   * Configure streams.
   *
   * @param[in] stream_list
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR configureStreams(
      camera3_stream_configuration_t* stream_list) = 0;

  /**
   * Query configured streams.
   * This call is valid only after streams are configured.
   *
   * @param[out] rStreams: contains all configured streams.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR queryConfiguredStreams(ConfigAppStreams* rStreams) const = 0;

  /**
   * Create a request based-on camera3_capture_request.
   * This call is valid only after streams are configured.
   *
   * @param[in] request: a given camera3_capture_request.
   *
   * @param[out] rRequest: a newly-created request.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR createRequest(camera3_capture_request_t* request,
                               Request* rRequest) = 0;

  /**
   * Register a request, which is created from createRequest.
   * This call is valid only after streams are configured.
   *
   * @param[in] rRequest: a request to register.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR registerRequest(Request const& rRequest) = 0;

  /**
   * Update a given result frame.
   *
   * @param[in] frameNo: the frame number to update.
   *
   * @param[in] resultMeta: result partial metadata to update.
   *
   * @param[in] hasLastPartial: contain last partial metadata in result partial
   * metadata vector.
   */
  virtual MVOID updateResult(
      MUINT32 const frameNo,
      MINTPTR const userId,
      std::vector<std::shared_ptr<IMetaStreamBuffer>> resultMeta,
      bool hasLastPartial) = 0;

  /**
   * Wait until all the registered requests have finished returning.
   *
   * @param[in] timeout
   */
  virtual MERROR waitUntilDrained(int64_t const timeout) = 0;

  virtual MERROR queryOldestRequestNumber(MUINT32* /*ReqNo*/) { return OK; }

  virtual ~IAppStreamManager() {}
};

/**************************************************************************
 *
 **************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_APP_IAPPSTREAMMANAGER_H_
