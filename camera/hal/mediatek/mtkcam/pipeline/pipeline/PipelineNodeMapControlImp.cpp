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
#include <map>
#include <memory>
#include "MyUtils.h"
#include "IPipelineNodeMapControl.h"
//
using NSCam::v3::IPipelineNodeMapControl;
using NSCam::v3::Utils::IStreamInfoSetControl;

/******************************************************************************
 *
 ******************************************************************************/
namespace {
class PipelineNodeMapControlImp : public IPipelineNodeMapControl {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  struct MyNode : public INode {
   protected:  ////                Data Members.
    NodePtrT mpNode;
    std::shared_ptr<IStreamInfoSetControl> mpInStreams;
    std::shared_ptr<IStreamInfoSetControl> mpOutStreams;

   public:  ////                Operations.
    explicit MyNode(NodePtrT const& pNode = nullptr)
        : mpNode(pNode),
          mpInStreams(IStreamInfoSetControl::create()),
          mpOutStreams(IStreamInfoSetControl::create()) {}
    virtual ~MyNode() {}

   public:  ////                Operations.
    virtual NodePtrT const& getNode() const { return mpNode; }

   public:  ////                Operations.
    virtual IStreamSetPtr_CONST getInStreams() const { return mpInStreams; }
    virtual IStreamSetPtr_CONST getOutStreams() const { return mpOutStreams; }
    virtual IStreamSetPtr const& editInStreams() const { return mpInStreams; }
    virtual IStreamSetPtr const& editOutStreams() const { return mpOutStreams; }
  };

  typedef std::map<NodeId_T, std::shared_ptr<MyNode> > MapT;

 protected:  ////                    Data Members.
  mutable pthread_rwlock_t mRWLock;
  MapT mMap;

 public:  ////                    Operations.
  PipelineNodeMapControlImp();
  ~PipelineNodeMapControlImp() { pthread_rwlock_destroy(&mRWLock); }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNodeMapControl Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  virtual MVOID clear();

  virtual ssize_t add(NodeId_T const& id, NodePtrT const& node);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNodeMap Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  virtual MBOOL isEmpty() const;
  virtual size_t size() const;

  virtual NodePtrT nodeFor(NodeId_T const& id) const;
  virtual NodePtrT nodeAt(size_t index) const;

  virtual std::shared_ptr<INode> getNodeFor(NodeId_T const& id) const;
  virtual std::shared_ptr<INode> getNodeAt(size_t index) const;
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
IPipelineNodeMapControl* IPipelineNodeMapControl::create() {
  return new PipelineNodeMapControlImp;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineNodeMapControlImp::PipelineNodeMapControlImp() {
  pthread_rwlock_init(&mRWLock, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
PipelineNodeMapControlImp::isEmpty() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  MBOOL ret = mMap.empty();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t PipelineNodeMapControlImp::size() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  size_t ret = mMap.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineNodeMapControlImp::clear() {
  pthread_rwlock_wrlock(&mRWLock);
  //
  mMap.clear();
  pthread_rwlock_unlock(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t PipelineNodeMapControlImp::add(NodeId_T const& id,
                                       NodePtrT const& node) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto result = mMap.emplace(id, std::make_shared<MyNode>(node));
  ssize_t ret = std::distance(mMap.begin(), result.first);
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineNodeMapControlImp::NodePtrT PipelineNodeMapControlImp::nodeFor(
    NodeId_T const& id) const {
  std::shared_ptr<INode> p = getNodeFor(id);
  if (!p) {
    MY_LOGW("Bad NodeId:%" PRIxPTR, id);
    return nullptr;
  }
  //
  return p->getNode();
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineNodeMapControlImp::NodePtrT PipelineNodeMapControlImp::nodeAt(
    size_t index) const {
  std::shared_ptr<INode> p = getNodeAt(index);
  if (!p) {
    MY_LOGW("Bad index:%zu", index);
    return nullptr;
  }
  //
  return p->getNode();
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineNodeMapControl::INode>
PipelineNodeMapControlImp::getNodeFor(NodeId_T const& id) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  auto it = mMap.find(id);
  if (it == mMap.end()) {
    MY_LOGW("NodeId:%" PRIxPTR " does not belong to the map", id);
    for (auto& i : mMap) {
      MY_LOGW("NodeId:%" PRIxPTR, i.first);
    }
    pthread_rwlock_unlock(&mRWLock);
    return nullptr;
  }
  auto ret = mMap.at(id);
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineNodeMapControl::INode>
PipelineNodeMapControlImp::getNodeAt(size_t index) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  auto it = mMap.begin();
  if (index >= mMap.size()) {
    pthread_rwlock_unlock(&mRWLock);
    return NULL;
  }
  std::advance(it, index);
  auto ret = it->second;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}
