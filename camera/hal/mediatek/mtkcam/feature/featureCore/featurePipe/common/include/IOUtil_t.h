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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IOUTIL_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IOUTIL_T_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "MtkHeader.h"
#include "BufferPool.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum IOPolicyType {
  IOPOLICY_BYPASS,
  IOPOLICY_INOUT,
  IOPOLICY_LOOPBACK,
  IOPOLICY_INOUT_EXCLUSIVE,
  IOPOLICY_INOUT_QUEUE,
  IOPOLICY_INPLACE,
  IOPOLICY_DINDOUT,
  IOPOLICY_DINSOUT,
  IOPOLICY_COUNT,
};

enum IOProcessorType {
  IOPROCESSOR_BYPASS,
  IOPROCESSOR_INPLACE,
  IOPROCESSOR_OUTPLACE,
};

enum OutputType {
  OUTPUT_INVALID,
  OUTPUT_FULL,
  OUTPUT_NEXT_FULL,
  OUTPUT_NEXT_EXCLUSIVE_FULL,
  OUTPUT_DUAL_FULL,
  OUTPUT_STREAM_PREVIEW,
  OUTPUT_STREAM_PREVIEW_CALLBACK,
  OUTPUT_STREAM_RECORD,
  OUTPUT_STREAM_PHYSICAL,
  OUTPUT_STREAM_FD,  // Must be the last one
};

enum StreamType {
  STREAMTYPE_PREVIEW,
  STREAMTYPE_PREVIEW_CALLBACK,
  STREAMTYPE_RECORD,
  STREAMTYPE_PHYSICAL,
  STREAMTYPE_FD,  // Must be the last one
};

template <typename Node_T>
class IONode {
 public:
  typedef typename std::set<Node_T*> NODE_SET;
  typedef typename std::set<Node_T*>::iterator NODE_SET_ITERATOR;
  typedef std::map<OutputType, NODE_SET> OUTPUT_MAP;

  IONode();
  ~IONode();

  Node_T* mNode;
  OUTPUT_MAP mOutputMap;
};

template <typename Node_T>
class IOGraph {
 public:
  IOGraph();
  ~IOGraph();

  Node_T* mRoot;
};

template <typename Node_T, typename ReqInfo_T>
class IOControl {
 public:
  typedef typename std::set<std::shared_ptr<Node_T>> NODE_SET;
  typedef
      typename std::set<std::shared_ptr<Node_T>>::iterator NODE_SET_ITERATOR;
  typedef typename std::list<std::shared_ptr<Node_T>> NODE_LIST;
  typedef
      typename std::list<std::shared_ptr<Node_T>>::iterator NODE_LIST_ITERATOR;
  typedef std::map<StreamType, NODE_LIST> STREAM_MAP;
  typedef typename STREAM_MAP::iterator STREAM_MAP_ITERATOR;
  typedef std::list<IOPolicyType> NODE_POLICY_LIST;
  typedef std::list<IOPolicyType>::iterator NODE_POLICY_LIST_ITERATOR;
  typedef std::map<OutputType, NODE_SET> OUTPUT_MAP;
  typedef typename OUTPUT_MAP::iterator OUTPUT_MAP_ITERATOR;
  typedef std::map<std::shared_ptr<Node_T>, OUTPUT_MAP> NODE_OUTPUT_MAP;
  typedef typename NODE_OUTPUT_MAP::iterator NODE_OUTPUT_MAP_ITERATOR;
  typedef std::map<std::shared_ptr<Node_T>, std::shared_ptr<IIBuffer>>
      NODE_BUFFER_MAP;
  typedef typename std::set<StreamType> STREAM_SET;
  typedef typename std::set<StreamType>::iterator STREAM_SET_ITERATOR;

  IOControl();
  ~IOControl();

  MBOOL init();
  MBOOL uninit();

  MVOID setRoot(std::shared_ptr<Node_T> root);
  MVOID addStream(StreamType stream, NODE_LIST list);
  MBOOL prepareMap(STREAM_SET streams,
                   const ReqInfo_T& reqInfo,
                   NODE_OUTPUT_MAP* outMap,
                   NODE_BUFFER_MAP* bufMap);

  MVOID printNode(std::shared_ptr<Node_T> node,
                  NODE_OUTPUT_MAP* outMap,
                  std::string depth,
                  std::string edge,
                  bool isLast,
                  std::set<std::shared_ptr<Node_T>>* visited);
  MVOID printMap(NODE_OUTPUT_MAP* outMap);
  MVOID dumpInfo(NODE_OUTPUT_MAP* outMap);
  MVOID dumpInfo(NODE_BUFFER_MAP* bufMap);
  MVOID dumpInfo(const char* name, OUTPUT_MAP* oMap);

 private:
  MBOOL prepareStreamMap(StreamType stream,
                         const ReqInfo_T& reqInfo,
                         NODE_LIST const& list,
                         NODE_OUTPUT_MAP* outMap);
  MVOID allocNextBuf(const ReqInfo_T& reqInfo,
                     NODE_OUTPUT_MAP* outMap,
                     NODE_BUFFER_MAP* bufMap);
  NODE_POLICY_LIST getStreamPolicy(const ReqInfo_T& reqInfo, StreamType stream);
  MBOOL forwardCheck(NODE_POLICY_LIST policys);
  MVOID backwardCalc(const ReqInfo_T& reqInfo,
                     StreamType stream,
                     NODE_LIST list,
                     NODE_OUTPUT_MAP* outMap);

  class Finder {
   public:
    Finder(const ReqInfo_T& reqInfo, StreamType stream);
    bool operator()(const std::shared_ptr<Node_T> node) const;

   private:
    const ReqInfo_T& mReqInfo;
    StreamType mStream;
  };

  std::shared_ptr<Node_T> mRoot;
  NODE_SET mNodes;
  STREAM_MAP mStreams;
};

template <typename Node_T, typename ReqInfo_T>
class IORequest {
 public:
  typedef
      typename IOControl<Node_T, ReqInfo_T>::NODE_OUTPUT_MAP NODE_OUTPUT_MAP;
  typedef
      typename IOControl<Node_T, ReqInfo_T>::NODE_BUFFER_MAP NODE_BUFFER_MAP;

  MBOOL needPreview(std::shared_ptr<Node_T> node);
  MBOOL needPreviewCallback(std::shared_ptr<Node_T> node);
  MBOOL needRecord(std::shared_ptr<Node_T> node);
  MBOOL needFD(std::shared_ptr<Node_T> node);
  MBOOL needPhysicalOut(std::shared_ptr<Node_T> node);
  MBOOL needFull(std::shared_ptr<Node_T> node);
  MBOOL needNextFull(std::shared_ptr<Node_T> node);
  std::shared_ptr<IIBuffer> getNextFullImg(std::shared_ptr<Node_T> node);

  MBOOL needOutputType(std::shared_ptr<Node_T> node, OutputType type);

  NODE_OUTPUT_MAP mOutMap;
  NODE_BUFFER_MAP mBufMap;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IOUTIL_T_H_
