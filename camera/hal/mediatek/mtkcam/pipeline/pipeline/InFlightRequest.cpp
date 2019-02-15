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

#define LOG_TAG "MtkCam/inFlightRequest"
//
#include "MyUtils.h"
#include <mtkcam/utils/std/Trace.h>

#include <memory>
#include <mtkcam/pipeline/pipeline/InFlightRequest.h>
#include <property_lib.h>
#include <string>
#include <utility>
#include <vector>
//
using NSCam::v3::InFlightRequest;
/******************************************************************************
 *
 ******************************************************************************/
InFlightRequest::InFlightRequest() : IPipelineFrameListener() {}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::dumpState(const std::vector<std::string>& options) const {
  {
    mLock.lock();
    for (auto const& v : mRequest) {
      auto pFrame = v.second.lock();
      if (pFrame != nullptr) {
        pFrame->dumpState(options);
      }
    }

    mLock.unlock();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::registerRequest(
    std::shared_ptr<IPipelineFrame> const& pFrame) {
  MY_LOGD("+");
  std::lock_guard<std::mutex> _l(mLock);
  // add request
  mRequest.push_back(std::make_pair(pFrame->getFrameNo(), pFrame));

  // add node
  std::vector<IPipelineDAG::NodeObj_T> const& vNode =
      pFrame->getPipelineDAG().getToposort();
  for (size_t s = 0; s < vNode.size(); s++) {
    NodeId_T node = vNode[s].id;

    std::shared_ptr<IStreamInfoSet const> in, out;
    if (CC_UNLIKELY(OK != pFrame->queryIOStreamInfoSet(node, &in, &out))) {
      MY_LOGE("queryIOStreamInfoSet failed");
      break;
    }
    //
    if (out->getImageInfoNum() > 0 || out->getMetaInfoNum() > 0) {
      auto it = mRequestMap_image.find(node);
      if (it == mRequestMap_image.end()) {
        RequestList frameL;
        frameL.push_back(pFrame->getFrameNo());
        mRequestMap_image.emplace(node, frameL);
      } else {
        it->second.push_back(pFrame->getFrameNo());
      }

      auto it2 = mRequestMap_meta.find(node);
      if (it2 == mRequestMap_meta.end()) {
        RequestList frameL;
        frameL.push_back(pFrame->getFrameNo());
        mRequestMap_meta.emplace(node, frameL);
      } else {
        it2->second.push_back(pFrame->getFrameNo());
      }
    }
  }
  // register
  pFrame->attachListener(shared_from_this(), nullptr);
  MY_LOGD("-");
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::waitUntilDrained() {
  CAM_TRACE_CALL();

  MY_LOGD("+");
  std::unique_lock<std::mutex> _l(mLock);
  while (!mRequest.empty()) {
    MY_LOGD_IF(1, "frameNo:%u in the front", (*(mRequest.begin())).first);
    mRequestCond.wait(_l);
  }
  MY_LOGD("-");
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::waitUntilNodeDrained(NodeId_T id) {
  CAM_TRACE_CALL();

  waitUntilNodeMetaDrained(id);
  waitUntilNodeImageDrained(id);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::waitUntilNodeMetaDrained(NodeId_T id) {
  std::unique_lock<std::mutex> _l(mLock);
  do {
    auto it = mRequestMap_meta.find(id);
    if (it == mRequestMap_meta.end() || it->second.empty()) {
      break;
    }

    MY_LOGD("Node: %" PRIdPTR " has frameNo: %d in the front of meta list", id,
            *(it->second.begin()));
    mRequestCond.wait(_l);
  } while (1);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::waitUntilNodeImageDrained(NodeId_T id) {
  std::unique_lock<std::mutex> _l(mLock);
  do {
    auto it = mRequestMap_image.find(id);
    if (it == mRequestMap_image.end() || it->second.empty()) {
      break;
    }

    MY_LOGD("Node: %" PRIdPTR " has frameNo: %d in the front of image list", id,
            *(it->second.begin()));
    mRequestCond.wait(_l);
  } while (1);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::onPipelineFrame(MUINT32 const frameNo,
                                 MUINT32 const message,
                                 MVOID* const /*pCookie*/
) {
  MY_LOGD("frame: %d message: %#x", frameNo, message);
  std::lock_guard<std::mutex> _l(mLock);
  if (message == eMSG_FRAME_RELEASED) {
    for (FrameListT::iterator it = mRequest.begin(); it != mRequest.end();
         it++) {
      if ((*it).first == frameNo) {
        mRequest.erase(it);
        mRequestCond.notify_all();
        break;
      }
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::onPipelineFrame(MUINT32 const frameNo,
                                 NodeId_T const nodeId,
                                 MUINT32 const message,
                                 MVOID* const /*pCookie*/
) {
  MY_LOGD("frame: %d, nodeId: %" PRIdPTR ", msg: %d", frameNo, nodeId, message);

  std::lock_guard<std::mutex> _l(mLock);
  auto it_meta = mRequestMap_meta.find(nodeId);
  auto it_image = mRequestMap_image.find(nodeId);
  if (CC_UNLIKELY(it_meta == mRequestMap_meta.end() &&
                  it_image == mRequestMap_image.end())) {
    MY_LOGE("no node in meta/image mapper: %" PRIdPTR "", nodeId);
    return;
  }

  switch (message) {
    case eMSG_ALL_OUT_META_BUFFERS_RELEASED: {
      RequestList& list = it_meta->second;
      for (RequestList::iterator it = list.begin(); it != list.end(); it++) {
        if (*it == frameNo) {
          list.erase(it);
          break;
        }
      }
      mRequestCond.notify_all();
    } break;

    case eMSG_ALL_OUT_IMAGE_BUFFERS_RELEASED: {
      RequestList& list = it_image->second;
      for (RequestList::iterator it = list.begin(); it != list.end(); it++) {
        if (*it == frameNo) {
          list.erase(it);
          break;
        }
      }
      mRequestCond.notify_all();
    } break;

    default:
      break;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
InFlightRequest::clear() {
  std::lock_guard<std::mutex> _l(mLock);
  mRequestMap_meta.clear();
  mRequestMap_image.clear();
  mRequest.clear();
}
