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

#define LOG_TAG "MtkCam/pipeline"
//
#include "MyUtils.h"
#include <map>
#include <memory>
#include <mtkcam/pipeline/pipeline/IPipelineBufferSetFrameControl.h>
//
using NSCam::v3::IPipelineFrameNodeMapControl;

/**
 * An Implementation of Pipeline Frame Node Map Control.
 */
namespace {
class PipelineFrameNodeMapControlImp : public IPipelineFrameNodeMapControl {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct NodeInfo : public INode {
    NodeId_T mNodeId;
    IStreamInfoSetPtr mIStreams;
    IStreamInfoSetPtr mOStreams;
    InfoIOMapSet mIOMapSet;

    explicit NodeInfo(NodeId_T nodeId) : mNodeId(nodeId) {}
    virtual ~NodeInfo() {}
    virtual NodeId_T getNodeId() const { return mNodeId; }

    virtual IStreamInfoSetPtr_CONST getIStreams() const { return mIStreams; }
    virtual MVOID setIStreams(IStreamInfoSetPtr p) { mIStreams = p; }

    virtual IStreamInfoSetPtr_CONST getOStreams() const { return mOStreams; }
    virtual MVOID setOStreams(IStreamInfoSetPtr p) { mOStreams = p; }

    virtual InfoIOMapSet const& getInfoIOMapSet() const { return mIOMapSet; }
    virtual InfoIOMapSet& editInfoIOMapSet() { return mIOMapSet; }
  };

  struct NodeInfoMap : public std::map<NodeId_T, std::shared_ptr<NodeInfo> > {};

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                Data Members.
  mutable pthread_rwlock_t mRWLock;
  NodeInfoMap mMap;

 public:  ////                Operations.
  PipelineFrameNodeMapControlImp();
  ~PipelineFrameNodeMapControlImp() { pthread_rwlock_destroy(&mRWLock); }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineFrameNodeMapControl Interface..
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  virtual MVOID clear();
  virtual ssize_t addNode(NodeId_T const nodeId);

 public:  ////                Operations.
  virtual MBOOL isEmpty() const;
  virtual size_t size() const;

  virtual std::shared_ptr<INode> getNodeFor(NodeId_T const nodeId) const;
  virtual std::shared_ptr<INode> getNodeAt(size_t index) const;
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineFrameNodeMapControl>
IPipelineFrameNodeMapControl::create() {
  auto ptr = std::make_shared<PipelineFrameNodeMapControlImp>();
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineFrameNodeMapControlImp::PipelineFrameNodeMapControlImp() {
  pthread_rwlock_init(&mRWLock, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineFrameNodeMapControlImp::clear() {
  pthread_rwlock_wrlock(&mRWLock);
  //
  mMap.clear();
  pthread_rwlock_unlock(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t PipelineFrameNodeMapControlImp::addNode(NodeId_T const nodeId) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto iter = mMap.emplace(nodeId, std::make_shared<NodeInfo>(nodeId));
  ssize_t ret = std::distance(mMap.begin(), iter.first);
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
PipelineFrameNodeMapControlImp::isEmpty() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  MBOOL ret = mMap.empty();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t PipelineFrameNodeMapControlImp::size() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  size_t ret = mMap.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<PipelineFrameNodeMapControlImp::INode>
PipelineFrameNodeMapControlImp::getNodeFor(NodeId_T const nodeId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  auto it = mMap.find(nodeId);
  if (it == mMap.end()) {
    MY_LOGW("NodeId:%#" PRIxPTR " does not belong to the map", nodeId);
    for (auto& i : mMap) {
      MY_LOGW("NodeId:%#" PRIxPTR, i.first);
    }
    pthread_rwlock_unlock(&mRWLock);
    return NULL;
  }
  //
  if (it->second == 0) {
    pthread_rwlock_unlock(&mRWLock);
    return NULL;
  }
  auto ret = it->second;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<PipelineFrameNodeMapControlImp::INode>
PipelineFrameNodeMapControlImp::getNodeAt(size_t index) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  auto it = mMap.begin();
  if (index >= mMap.size()) {
    pthread_rwlock_unlock(&mRWLock);
    return NULL;
  }

  std::advance(it, index);
  if (it->second == 0) {
    pthread_rwlock_unlock(&mRWLock);
    return NULL;
  }
  auto ret = it->second;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}
