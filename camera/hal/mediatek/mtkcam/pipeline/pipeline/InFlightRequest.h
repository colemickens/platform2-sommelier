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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_INFLIGHTREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_INFLIGHTREQUEST_H_
//

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <condition_variable>
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class InFlightRequest : public IPipelineFrameListener,
                        public std::enable_shared_from_this<InFlightRequest> {
 public:
  InFlightRequest();
  ~InFlightRequest() {}

 public:
  /**
   * Dump debugging state.
   */
  virtual MVOID dumpState(const std::vector<std::string>& options) const;

  virtual MVOID clear();

  /* register for a new request */
  virtual MVOID registerRequest(std::shared_ptr<IPipelineFrame> const& pFrame);

  /* wait until all inflight request done */
  virtual MVOID waitUntilDrained();

  /* wait until specified node has done all requests*/
  virtual MVOID waitUntilNodeDrained(NodeId_T id);

  /* wait until specified node has done all requests*/
  virtual MVOID waitUntilNodeImageDrained(NodeId_T id);

  /* wait until specified node has done all requests*/
  virtual MVOID waitUntilNodeMetaDrained(NodeId_T id);

  /****************************************************************************
   *   IPipelineFrameListener Interface
   ****************************************************************************/
  virtual MVOID onPipelineFrame(MUINT32 const frameNo,
                                MUINT32 const message,
                                MVOID* const pCookie);

  virtual MVOID onPipelineFrame(MUINT32 const frameNo,
                                NodeId_T const nodeId,
                                MUINT32 const message,
                                MVOID* const pCookie);

 protected:  /// data members
  mutable std::mutex mLock;
  std::condition_variable mRequestCond;
  using FrameListT =
      std::list<std::pair<MUINT32, std::weak_ptr<IPipelineFrame>>>;

  FrameListT mRequest;
  typedef std::list<MUINT32> RequestList;
  std::map<NodeId_T, RequestList>
      mRequestMap_meta;  // pair<Node, request_list> for meta
  std::map<NodeId_T, RequestList>
      mRequestMap_image;  // pair<Node, request_list> for image
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_INFLIGHTREQUEST_H_
