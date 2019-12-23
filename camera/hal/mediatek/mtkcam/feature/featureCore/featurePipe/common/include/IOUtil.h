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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IOUTIL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IOUTIL_H_

#include "IOUtil_t.h"

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_IO_UTIL
#define PIPE_CLASS_TAG "IOUtil"
#include "PipeLog.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

static inline const char* policyToName(IOPolicyType policy) {
  switch (policy) {
    case IOPOLICY_BYPASS:
      return "bypass";
    case IOPOLICY_INOUT:
      return "inout";
    case IOPOLICY_LOOPBACK:
      return "loopback";
    case IOPOLICY_INOUT_EXCLUSIVE:
      return "inout_e";
    case IOPOLICY_INOUT_QUEUE:
      return "inout_q";
    case IOPOLICY_INPLACE:
      return "inplace";
    case IOPOLICY_DINDOUT:
      return "din_dout";
    case IOPOLICY_DINSOUT:
      return "din_sout";
    default:
      return "unknown";
  }
}

static inline const char* typeToName(OutputType type) {
  switch (type) {
    case OUTPUT_STREAM_FD:
      return "fd";
    case OUTPUT_STREAM_PREVIEW:
      return "preview";
    case OUTPUT_STREAM_PREVIEW_CALLBACK:
      return "preview_callback";
    case OUTPUT_STREAM_RECORD:
      return "record";
    case OUTPUT_FULL:
      return "full";
    case OUTPUT_STREAM_PHYSICAL:
      return "phy_out";
    case OUTPUT_NEXT_FULL:
      return "next_full";
    case OUTPUT_NEXT_EXCLUSIVE_FULL:
      return "next_exclusive_full";
    case OUTPUT_DUAL_FULL:
      return "dual_full";
    case OUTPUT_INVALID:
      return "invalid";
    default:
      return "unknown";
  }
}

static inline const char* streamToName(StreamType stream) {
  switch (stream) {
    case STREAMTYPE_PREVIEW:
      return "preview";
    case STREAMTYPE_PREVIEW_CALLBACK:
      return "preview_callback";
    case STREAMTYPE_RECORD:
      return "record";
    case STREAMTYPE_PHYSICAL:
      return "phy";
    case STREAMTYPE_FD:
      return "fd";
    default:
      return "unknown";
  }
}

static inline OutputType streamToType(StreamType stream) {
  switch (stream) {
    case STREAMTYPE_PREVIEW:
      return OUTPUT_STREAM_PREVIEW;
    case STREAMTYPE_FD:
      return OUTPUT_STREAM_FD;
    case STREAMTYPE_PREVIEW_CALLBACK:
      return OUTPUT_STREAM_PREVIEW_CALLBACK;
    case STREAMTYPE_RECORD:
      return OUTPUT_STREAM_RECORD;
    case STREAMTYPE_PHYSICAL:
      return OUTPUT_STREAM_PHYSICAL;
    default:
      MY_LOGE("Unkown stream(%d)", stream);
      return OUTPUT_INVALID;
  }
}

template <typename Node_T, typename ReqInfo_T>
IOControl<Node_T, ReqInfo_T>::IOControl() : mRoot(NULL) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
IOControl<Node_T, ReqInfo_T>::~IOControl() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IOControl<Node_T, ReqInfo_T>::init() {}

template <typename Node_T, typename ReqInfo_T>
MBOOL IOControl<Node_T, ReqInfo_T>::uninit() {}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::setRoot(std::shared_ptr<Node_T> root) {
  mRoot = root;
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::addStream(StreamType stream,
                                              NODE_LIST list) {
  TRACE_FUNC_ENTER();
  NODE_LIST_ITERATOR it, end;
  for (it = list.begin(), end = list.end(); it != end; ++it) {
    mNodes.insert(*it);
  }
  mStreams[stream] = list;
  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IOControl<Node_T, ReqInfo_T>::prepareMap(STREAM_SET streams,
                                               const ReqInfo_T& reqInfo,
                                               NODE_OUTPUT_MAP* outMap,
                                               NODE_BUFFER_MAP* bufMap) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("{%s} stream(%zu)", reqInfo.dump(), streams.size());
  MBOOL ret = MTRUE;

  STREAM_SET_ITERATOR it, end;
  for (it = streams.begin(), end = streams.end(); it != end; ++it) {
    STREAM_MAP_ITERATOR it2 = mStreams.find(*it);
    if (it2 == mStreams.end()) {
      MY_LOGE("{%s} Cannot find stream(%s)", reqInfo.dump(), streamToName(*it));
    } else {
      ret &= prepareStreamMap(it2->first, reqInfo, it2->second, outMap);
    }
  }
  allocNextBuf(reqInfo, outMap, bufMap);

  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::printNode(
    std::shared_ptr<Node_T> node,
    NODE_OUTPUT_MAP* outMap,
    std::string depth,
    std::string edge,
    bool isLast,
    std::set<std::shared_ptr<Node_T>>* visited) {
  TRACE_FUNC_ENTER();
  if (node == NULL || outMap->find(node) == outMap->end()) {
    MY_LOGD("%s%s[]", depth.c_str(), edge.c_str());
    return;
  }

  MY_LOGD("%s%s[%s]", depth.c_str(), edge.c_str(), node->getName());

  if (visited->find(node) != visited->end()) {
    return;
  }
  visited->insert(node);

  OUTPUT_MAP& oMap = outMap->find(node)->second;

  OUTPUT_MAP_ITERATOR mapIter = oMap.begin();
  OUTPUT_MAP_ITERATOR mapEnd = oMap.end();

  int n = edge.size();

  std::vector<std::pair<OutputType, std::shared_ptr<Node_T>>> nodeVector;

  while (mapIter != mapEnd) {
    NODE_SET_ITERATOR setIter = (*mapIter).second.begin();
    NODE_SET_ITERATOR setEnd = (*mapIter).second.end();
    while (setIter != setEnd) {
      std::shared_ptr<Node_T> node = *setIter;
      nodeVector.push_back(std::make_pair((*mapIter).first, node));
      setIter++;
    }
    mapIter++;
  }

  for (unsigned i = 0, size = nodeVector.size(); i < size; ++i) {
    edge =
        std::string("`-") + typeToName(nodeVector[i].first) + std::string("-");
    if (n > 0) {
      depth.append(1, isLast ? ' ' : '|');
      depth.append(n - 1, ' ');
    }
    printNode(nodeVector[i].second, outMap, depth, edge, (i == (size - 1)),
              visited);
    depth.resize(depth.size() - n);
  }

  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::printMap(NODE_OUTPUT_MAP* outMap) {
  TRACE_FUNC_ENTER();

  std::set<std::shared_ptr<Node_T>> visited;
  std::string depth, edge;
  printNode(mRoot, outMap, depth, edge, true, &visited);
  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::dumpInfo(NODE_OUTPUT_MAP* outMap) {
  TRACE_FUNC_ENTER();

  STREAM_MAP_ITERATOR it, end;
  for (it = mStreams.begin(), end = mStreams.end(); it != end; ++it) {
    for (auto node : (*it).second) {
      dumpInfo(node->getName(), &((*outMap)[node]));
    }
  }

  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::dumpInfo(NODE_BUFFER_MAP* bufMap) {
  TRACE_FUNC_ENTER();

  STREAM_MAP_ITERATOR it, end;
  for (it = mStreams.begin(), end = mStreams.end(); it != end; ++it) {
    for (auto node : (*it).second) {
      MY_LOGD("node(%s) has buffer %p", node->getName(), (*bufMap)[node].get());
    }
  }

  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::dumpInfo(const char* name,
                                             OUTPUT_MAP* oMap) {
  TRACE_FUNC_ENTER();

  OUTPUT_MAP_ITERATOR it, end;
  for (it = oMap->begin(), end = oMap->end(); it != end; ++it) {
    std::string str;
    for (auto node : (*it).second) {
      str += str.empty() ? "" : ",";
      if (node) {
        str += node->getName();
      } else {
        str += "NULL";
      }
    }

    MY_LOGD("node(%s) has type(%s) output to {%s}", name,
            typeToName((*it).first), str.c_str());
  }

  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IOControl<Node_T, ReqInfo_T>::prepareStreamMap(StreamType stream,
                                                     const ReqInfo_T& reqInfo,
                                                     NODE_LIST const& nodes,
                                                     NODE_OUTPUT_MAP* outMap) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  NODE_POLICY_LIST policys = getStreamPolicy(reqInfo, stream);
  if (forwardCheck(policys)) {
    backwardCalc(reqInfo, stream, nodes, outMap);
  } else {
    MY_LOGW("{%s} forward check error, stream(%s) nodes(%zu)", reqInfo.dump(),
            streamToName(stream), nodes.size());
    ret = MFALSE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T, typename ReqInfo_T>
typename IOControl<Node_T, ReqInfo_T>::NODE_POLICY_LIST
IOControl<Node_T, ReqInfo_T>::getStreamPolicy(const ReqInfo_T& reqInfo,
                                              StreamType stream) {
  TRACE_FUNC_ENTER();

  NODE_POLICY_LIST policys;
  NODE_LIST& s = mStreams[stream];
  NODE_LIST_ITERATOR it, end;
  for (it = s.begin(), end = s.end(); it != end; ++it) {
    IOPolicyType p = (*it)->getIOPolicy(stream, reqInfo);
    policys.insert(policys.end(), p);
    TRACE_FUNC("{%s} node(%s) inser policy(%s)", reqInfo.dump(),
               (*it)->getName(), policyToName(p));
  }

  TRACE_FUNC_EXIT();
  return policys;
}

static bool lut[IOPOLICY_COUNT][IOPOLICY_COUNT] = {
    /*bypass inout   loopback    inout_e inout_q inplace dindout dinsout*/
    /*IOPOLICY_BYPASS*/ {true, true, true, true, true, true, true, true},
    /*IOPOLICY_INOUT*/ {true, true, true, true, true, true, false, false},
    /*IOPOLICY_LOOPBACK*/ {true, true, true, true, true, true, false, false},
    /*IOPOLICY_INOUT_EXCLUSIVE*/
    {true, true, true, false, false, false, false, false},
    /*IOPOLICY_INOUT_QUEUE*/
    {true, true, true, false, false, false, false, false},
    /*IOPOLICY_INPLACE*/ {true, true, true, false, false, false, false, false},
    /*IOPOLICY_DINDOUT*/ {true, false, false, false, false, false, true, true},
    /*IOPOLICY_DINSOUT*/ {true, true, true, true, true, true, false, false},
};

template <typename Node_T, typename ReqInfo_T>
MBOOL IOControl<Node_T, ReqInfo_T>::forwardCheck(NODE_POLICY_LIST policys) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  int count = 0;
  IOPolicyType prevPolicy = IOPOLICY_BYPASS;
  NODE_POLICY_LIST_ITERATOR it, end;
  for (it = policys.begin(), end = policys.end(); it != end; ++it) {
    IOPolicyType currPolicy = *it;
    if (currPolicy != IOPOLICY_BYPASS) {
      if (prevPolicy != IOPOLICY_BYPASS) {
        TRACE_FUNC("Lookup %s to %s", policyToName(prevPolicy),
                   policyToName(currPolicy));
        if (!lut[prevPolicy][currPolicy]) {
          MY_LOGW("%s to %s is invalid", policyToName(prevPolicy),
                  policyToName(currPolicy));
          ret = MFALSE;
          break;
        }
      }

      prevPolicy = currPolicy;
      count++;
    }
  }

  if (count == 0) {
    ret = MFALSE;
  }

  TRACE_FUNC_EXIT();

  return ret;
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::backwardCalc(const ReqInfo_T& reqInfo,
                                                 StreamType stream,
                                                 NODE_LIST list,
                                                 NODE_OUTPUT_MAP* outMap) {
  TRACE_FUNC_ENTER();

  int count = 0;
  OutputType type = streamToType(stream);
  OutputType nextType = type;

  TRACE_FUNC("{%s} list_size(%zu) type(%s)", reqInfo.dump(), list.size(),
             typeToName(type));

  Finder finder(reqInfo, stream);
  std::shared_ptr<Node_T> prev = nullptr;
  auto it = std::find_if(list.crbegin(), list.crend(), finder);

  while (it != list.rend()) {
    IOPolicyType currPolicy = (*it)->getIOPolicy(stream, reqInfo);

    if (currPolicy == IOPOLICY_INPLACE) {
      (*outMap)[*it][OUTPUT_FULL].insert(prev);
    } else {
      (*outMap)[*it][nextType].insert(prev);
      if (currPolicy == IOPOLICY_LOOPBACK) {
        (*outMap)[*it][OUTPUT_FULL].insert(*it);
      }
    }

    TRACE_FUNC("{%s} name(%s) add type(%s)", reqInfo.dump(), (*it)->getName(),
               typeToName(nextType));

    if (currPolicy == IOPOLICY_INOUT_QUEUE) {
      nextType = OUTPUT_NEXT_FULL;
    } else if (currPolicy == IOPOLICY_INOUT_EXCLUSIVE) {
      nextType = OUTPUT_NEXT_EXCLUSIVE_FULL;
    } else if (currPolicy == IOPOLICY_INPLACE) {
      // skip
    } else if (currPolicy == IOPOLICY_DINDOUT) {
      nextType = OUTPUT_DUAL_FULL;
    } else if (currPolicy == IOPOLICY_DINSOUT) {
      nextType = OUTPUT_DUAL_FULL;
    } else {
      nextType = OUTPUT_FULL;
    }

    prev = *it;
    it = std::find_if(++it, list.crend(), finder);
    ++count;
  }

  if (!count) {
    MY_LOGW("{%s} Can not found active policy in path!", reqInfo.dump());
  }

  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
MVOID IOControl<Node_T, ReqInfo_T>::allocNextBuf(const ReqInfo_T& reqInfo,
                                                 NODE_OUTPUT_MAP* outMap,
                                                 NODE_BUFFER_MAP* bufMap) {
  TRACE_FUNC_ENTER();

  NODE_OUTPUT_MAP_ITERATOR it, end;
  for (it = outMap->begin(), end = outMap->end(); it != end; ++it) {
    if (it->second.find(OUTPUT_NEXT_FULL) != it->second.end()) {
      NODE_SET s = it->second[OUTPUT_NEXT_FULL];
      NODE_SET_ITERATOR it2, end2;
      for (it2 = s.begin(), end2 = s.end(); it2 != end2; ++it2) {
        std::shared_ptr<IBufferPool> pool;
        if ((*it2)->getInputBufferPool(reqInfo, &pool) && pool != nullptr) {
          TRACE_FUNC(
              "{%s} name(%s) requst buffer type(%s) from name(%s) pool=(%d/%d)",
              reqInfo.dump(), (it->first)->getName(),
              typeToName(OUTPUT_NEXT_FULL), (*it2)->getName(),
              pool->peakAvailableSize(), pool->peakPoolSize());
          (*bufMap)[it->first] = pool->requestIIBuffer();
        } else {
          MY_LOGW(
              "{%s} name(%s) requst buffer type(%s) from name(%s), no input "
              "buffer pool",
              reqInfo.dump(), (it->first)->getName(),
              typeToName(OUTPUT_NEXT_FULL), (*it2)->getName());
        }
      }
    }

    if (it->second.find(OUTPUT_NEXT_EXCLUSIVE_FULL) != it->second.end()) {
      NODE_SET s = it->second[OUTPUT_NEXT_EXCLUSIVE_FULL];
      NODE_SET_ITERATOR it2, end2;
      for (it2 = s.begin(), end2 = s.end(); it2 != end2; ++it2) {
        std::shared_ptr<IBufferPool> pool;
        if ((*it2)->getInputBufferPool(reqInfo, &pool) && pool != nullptr) {
          TRACE_FUNC(
              "{%s} name(%s) requst buffer type(%s) from name(%s) pool=(%d/%d)",
              reqInfo.dump(), (it->first)->getName(),
              typeToName(OUTPUT_NEXT_EXCLUSIVE_FULL), (*it2)->getName(),
              pool->peakAvailableSize(), pool->peakPoolSize());
          (*bufMap)[it->first] = pool->requestIIBuffer();
        } else {
          MY_LOGW(
              "{%s} name(%s) requst buffer type(%s) from name(%s), no input "
              "buffer pool",
              reqInfo.dump(), (it->first)->getName(),
              typeToName(OUTPUT_NEXT_EXCLUSIVE_FULL), (*it2)->getName());
        }
      }
    }
  }

  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
IOControl<Node_T, ReqInfo_T>::Finder::Finder(const ReqInfo_T& reqInfo,
                                             StreamType stream)
    : mReqInfo(reqInfo), mStream(stream) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename Node_T, typename ReqInfo_T>
bool IOControl<Node_T, ReqInfo_T>::Finder::operator()(
    const std::shared_ptr<Node_T> node) const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return node->getIOPolicy(mStream, mReqInfo) != IOPOLICY_BYPASS;
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needPreview(std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_STREAM_PREVIEW);
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needPreviewCallback(
    std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_STREAM_PREVIEW_CALLBACK);
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needRecord(std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_STREAM_RECORD);
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needFD(std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_STREAM_FD);
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needPhysicalOut(
    std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_STREAM_PHYSICAL);
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needFull(std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_FULL) &&
         !needOutputType(node, OUTPUT_NEXT_FULL);
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needNextFull(std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return needOutputType(node, OUTPUT_NEXT_FULL) ||
         needOutputType(node, OUTPUT_NEXT_EXCLUSIVE_FULL);
}

template <typename Node_T, typename ReqInfo_T>
std::shared_ptr<IIBuffer> IORequest<Node_T, ReqInfo_T>::getNextFullImg(
    std::shared_ptr<Node_T> node) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  std::shared_ptr<IIBuffer> img = mBufMap[node];
  mBufMap[node] = nullptr;
  return img;
}

template <typename Node_T, typename ReqInfo_T>
MBOOL IORequest<Node_T, ReqInfo_T>::needOutputType(std::shared_ptr<Node_T> node,
                                                   OutputType type) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();

  return !mOutMap[node][type].empty();
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#include "PipeLogHeaderEnd.h"

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IOUTIL_H_
