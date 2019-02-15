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
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <mtkcam/pipeline/pipeline/IPipelineDAG.h>
#include <string>
#include <unordered_map>
#include <vector>
//
using NSCam::v3::IPipelineDAG;
using NSCam::v3::NodeSet;

/******************************************************************************
 *
 ******************************************************************************/
namespace {
class PipelineDAGImp : public IPipelineDAG {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  friend class IPipelineDAG;

 public:  ////                            Definitions.
  struct NodeWithAdj_T {
    NodeObj_T mNode;
    NodeIdSet_T mInAdj;    // In adjacent node_id set
    MUINT32 mInAdjReqCnt;  // In adjacent node send request count. After receive
                           // all request and then queue to nex node
    NodeIdSet_T mOutAdj;   // Out adjacent node_id set
    explicit NodeWithAdj_T(NodeObj_T const& node = NodeObj_T())
        : mNode(node), mInAdjReqCnt(0) {}
  };

  typedef std::map<NodeId_T, NodeWithAdj_T> Map_T;

 protected:  ////                            Data Members.
  mutable pthread_rwlock_t mRWLock;
  NodeSet mRootIds;
  Map_T mNodesVec;
  mutable std::vector<NodeObj_T> mToposort;  // topological sort

 protected:  ////                            Operations.
  PipelineDAGImp();
  ~PipelineDAGImp();

 private:  ////                            Operations.
  MERROR findPathBFS(NodeId_T id,
                     NodeIdSet_T* checkList,
                     std::shared_ptr<IPipelineDAG> newDAG) const;

  MERROR checkListDFS(NodeId_T id, bool* checkList) const;

  template <class T>
  static MERROR evaluateToposort(Map_T const& dag, T* rToposort);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineDAG Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                            Operations.
  virtual IPipelineDAG* clone() const;

  virtual IPipelineDAG* clone(NodeIdSet_T const& ids) const;

  virtual MERROR addNode(NodeId_T id, NodeVal_T const& val);

  virtual MERROR removeNode(NodeId_T id);

  virtual MERROR addEdge(NodeId_T id_src, NodeId_T id_dst);

  virtual MERROR removeEdge(NodeId_T id_src, NodeId_T id_dst);

  virtual MERROR setRootNode(NodeSet roots);

  virtual MERROR setNodeValue(NodeId_T id, NodeVal_T const& val);

  virtual MVOID dump() const;
  virtual MVOID dump(std::vector<std::string>* rLogs) const;

 public:  ////                            Attributes.
  virtual std::vector<NodeObj_T> getRootNode() const;

  virtual NodeObj_T getNode(NodeId_T id) const;

  virtual MERROR getEdges(std::vector<Edge>* result) const;

  virtual size_t getNumOfNodes() const;

  virtual MERROR getInAdjacentNodes(NodeId_T id, NodeObjSet_T* result) const;

  virtual MERROR getInAdjacentNodesReqCnt(NodeId_T id, MUINT32* count) const;

  virtual MERROR addInAdjacentNodesReqCnt(NodeId_T id);

  virtual MERROR getOutAdjacentNodes(NodeId_T id, NodeObjSet_T* result) const;

  virtual MERROR getNodesAndPathsForNewDAG(
      NodeIdSet_T* orphanNodes,
      NodeIdSet_T* checkList,
      std::shared_ptr<IPipelineDAG> newDAG) const;

  virtual MERROR getOrphanNodes(NodeIdSet_T* orphanNodes,
                                NodeIdSet_T* connectedNodes) const;

  virtual MERROR getTopological(std::list<NodeObj_T>* result) const;

  virtual std::vector<NodeObj_T> const& getToposort() const;
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
IPipelineDAG* IPipelineDAG::create() {
  return new PipelineDAGImp;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineDAGImp::PipelineDAGImp() {
  pthread_rwlock_init(&mRWLock, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineDAGImp::~PipelineDAGImp() {
  pthread_rwlock_destroy(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
IPipelineDAG* PipelineDAGImp::clone() const {
  pthread_rwlock_rdlock(&mRWLock);
  PipelineDAGImp* tmp = new PipelineDAGImp;
  tmp->mRootIds = mRootIds;
  tmp->mNodesVec = mNodesVec;
  pthread_rwlock_unlock(&mRWLock);
  return tmp;
}

/******************************************************************************
 *
 ******************************************************************************/
IPipelineDAG* PipelineDAGImp::clone(NodeIdSet_T const& ids) const {
  PipelineDAGImp* ndag;
  std::vector<NodeId_T> vDirty;
  {
    pthread_rwlock_rdlock(&mRWLock);
    //
    vDirty.reserve(mNodesVec.size() - ids.size());
    //
    //  Scan to determine the dirty set.
    for (auto& it : mNodesVec) {
      NodeId_T const nodeId = it.first;
      auto const index = std::find(ids.begin(), ids.end(), nodeId);
      if (ids.end() == index) {
        // Add this node to dirty if it is not specified within the given set.
        vDirty.emplace_back(nodeId);
        for (size_t i = 0; i < mRootIds.size(); i++) {
          if (mRootIds[i] == nodeId) {
            MY_LOGE("RootId:%" PRIxPTR " is not specified within the given set",
                    mRootIds[i]);
            pthread_rwlock_unlock(&mRWLock);
            return nullptr;
          }
        }
      }
    }
    //
    //  Check the given set of nodes is a subset of the original DAG.
    if (mNodesVec.size() != vDirty.size() + ids.size()) {
      MY_LOGE(
          "The given set is not a subset of the original DAG..."
          "#Original:%zu #Dirty:%zu #Given:%zu",
          mNodesVec.size(), vDirty.size(), ids.size());
      pthread_rwlock_unlock(&mRWLock);
      return nullptr;
    }
    ndag = new PipelineDAGImp;
    ndag->mRootIds = mRootIds;
    ndag->mNodesVec = mNodesVec;
    pthread_rwlock_unlock(&mRWLock);
  }
  //
  if (ndag) {
    //  Remove all of un-specified nodes from the newly-cloned dag.
    for (size_t i = 0; i < vDirty.size(); i++) {
      ndag->removeNode(vDirty[i]);
    }
  }
  return ndag;
}

/******************************************************************************
 *Add a node into DAG; if memory is not enough, return failure
 ******************************************************************************/
MERROR
PipelineDAGImp::addNode(NodeId_T id, NodeVal_T const& val) {
  pthread_rwlock_wrlock(&mRWLock);
  NodeObj_T tmpNode;
  tmpNode.id = id;
  tmpNode.val = val;
  mNodesVec.emplace(id, NodeWithAdj_T(tmpNode));
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *Remove a node from DAG; if remove a non exist node, return failure
 ******************************************************************************/
MERROR
PipelineDAGImp::removeNode(NodeId_T id) {
  pthread_rwlock_wrlock(&mRWLock);
  auto iter = mNodesVec.find(id);
  if (iter == mNodesVec.end()) {
    MY_LOGE("The node of id %" PRIxPTR " does not exist", id);
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  NodeSet::iterator it = mRootIds.begin();
  while (it != mRootIds.end()) {
    if (id == (*it)) {
      MY_LOGD("erase root node: id = %d" PRIxPTR " ", id);
      it = mRootIds.erase(it);
    } else {
      it++;
    }
  }
  //
  PipelineDAGImp::NodeIdSet_T* rmSet;
  // remove in adjacent nodes of deleted node
  rmSet = &(iter->second.mInAdj);
  size_t upper = rmSet->size();
  for (size_t i = 0; i < upper; i++) {
    auto index = std::find(mNodesVec.at(rmSet->at(i)).mOutAdj.begin(),
                           mNodesVec.at(rmSet->at(i)).mOutAdj.end(), id);
    mNodesVec.at(rmSet->at(i)).mOutAdj.erase(index);
  }
  // remove out adjacent nodes of deleted node
  rmSet = &(iter->second.mOutAdj);
  upper = rmSet->size();
  for (size_t i = 0; i < upper; i++) {
    auto index = std::find(mNodesVec.at(rmSet->at(i)).mInAdj.begin(),
                           mNodesVec.at(rmSet->at(i)).mInAdj.end(), id);
    mNodesVec.at(rmSet->at(i)).mInAdj.erase(index);
  }
  mNodesVec.erase(iter);

  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *Add an edge into DAG; if memory is not enough, return failure
 ******************************************************************************/
MERROR
PipelineDAGImp::addEdge(NodeId_T id_src, NodeId_T id_dst) {
  pthread_rwlock_wrlock(&mRWLock);
  // Ensure both nodes exist
  auto itsrcIdx = mNodesVec.find(id_src);
  auto itdstIdx = mNodesVec.find(id_dst);
  if (itsrcIdx == mNodesVec.end() || itdstIdx == mNodesVec.end()) {
    MY_LOGE("Node does not exist\nSrc ID:%" PRIxPTR "  Dst ID:%" PRIxPTR "\n",
            id_src, id_dst);
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }

  // Ensure that each edge only be added once
  auto srcEdgeIdx = std::find(itsrcIdx->second.mOutAdj.begin(),
                              itsrcIdx->second.mOutAdj.end(), id_dst);
  auto dstEdgeIdx = std::find(itdstIdx->second.mInAdj.begin(),
                              itdstIdx->second.mInAdj.end(), id_src);
  if (itsrcIdx->second.mOutAdj.end() == srcEdgeIdx ||
      itdstIdx->second.mInAdj.end() == dstEdgeIdx) {
    itsrcIdx->second.mOutAdj.push_back(id_dst);
    itdstIdx->second.mInAdj.push_back(id_src);
  }

  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *Remove an edge from DAG; if the edge does not exist, return failure
 ******************************************************************************/
MERROR
PipelineDAGImp::removeEdge(NodeId_T id_src, NodeId_T id_dst) {
  pthread_rwlock_wrlock(&mRWLock);
  auto itsrcIdx = mNodesVec.find(id_src);
  auto itdstIdx = mNodesVec.find(id_dst);
  if (itsrcIdx == mNodesVec.end() || itdstIdx == mNodesVec.end()) {
    MY_LOGE("Node does not exist\nSrc ID:%" PRIxPTR " Dst ID:%" PRIxPTR " \n",
            id_src, id_dst);
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }

  auto srcEdgeIdx = std::find(itsrcIdx->second.mOutAdj.begin(),
                              itsrcIdx->second.mOutAdj.end(), id_dst);
  auto dstEdgeIdx = std::find(itdstIdx->second.mInAdj.begin(),
                              itdstIdx->second.mInAdj.end(), id_src);
  if (itsrcIdx->second.mOutAdj.end() == srcEdgeIdx ||
      itdstIdx->second.mInAdj.end() == dstEdgeIdx) {
    MY_LOGE("Edge does not exist\nSrc ID:%" PRIxPTR " Dst ID:%" PRIxPTR " ",
            id_src, id_dst);
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }

  itsrcIdx->second.mOutAdj.erase(srcEdgeIdx);
  itdstIdx->second.mInAdj.erase(dstEdgeIdx);
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *set certain node to root node; if the node does not exist, return failure
 ******************************************************************************/
MERROR
PipelineDAGImp::setRootNode(NodeSet roots) {
  pthread_rwlock_wrlock(&mRWLock);
  if (roots.size() == 0) {
    MY_LOGE("Input error, roots.size() == %d", roots.size());
    pthread_rwlock_unlock(&mRWLock);
    return BAD_VALUE;
  }
  for (size_t i = 0; i < roots.size(); i++) {
    auto iter = mNodesVec.find(roots[i]);
    if (iter == mNodesVec.end()) {
      MY_LOGE("Node does not exist, RootNode[%d] ID:%" PRIxPTR "", i, roots[i]);
      pthread_rwlock_unlock(&mRWLock);
      return -1;
    }
  }
  mRootIds = roots;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDAGImp::setNodeValue(NodeId_T id, NodeVal_T const& val) {
  pthread_rwlock_wrlock(&mRWLock);
  auto const iter = mNodesVec.find(id);
  if (iter == mNodesVec.end()) {
    MY_LOGE("Node does not exist\nID:%" PRIxPTR "", id);
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  iter->second.mNode.val = val;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *get root node of a DAG; if node does not exist, return failure
 ******************************************************************************/
std::vector<PipelineDAGImp::NodeObj_T> PipelineDAGImp::getRootNode() const {
  pthread_rwlock_rdlock(&mRWLock);
  std::vector<NodeObj_T> node_set;
  // Check if mRootIds has been set or not
  if (mRootIds.size() == 0) {
    MY_LOGW("There is no root node (mRootIds.size() = %d)", mRootIds.size());
    pthread_rwlock_unlock(&mRWLock);
    return node_set;
  }

  Map_T::iterator idx;
  for (size_t i = 0; i < mRootIds.size(); i++) {
    auto idx = mNodesVec.find(mRootIds[i]);
    if (idx == mNodesVec.end()) {
      MY_LOGE("Node does not exist (ID[%d]:%" PRIxPTR ")", i, mRootIds[i]);
      pthread_rwlock_unlock(&mRWLock);
      return node_set;
    }
    node_set.push_back(idx->second.mNode);
  }
  pthread_rwlock_unlock(&mRWLock);
  return node_set;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDAGImp::getEdges(std::vector<PipelineDAGImp::Edge>* result) const {
  pthread_rwlock_rdlock(&mRWLock);

  result->clear();
  for (auto& it : mNodesVec) {
    NodeWithAdj_T currentNode = it.second;
    for (size_t j = 0; j < currentNode.mOutAdj.size(); j++) {
      result->emplace_back(
          Edge(currentNode.mNode.id, currentNode.mOutAdj.at(j)));
    }
  }
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}
/******************************************************************************
 *get a node by its ID; if no such node exist, return a node of -1,-1 as failure
 ******************************************************************************/
PipelineDAGImp::NodeObj_T PipelineDAGImp::getNode(NodeId_T id) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto idx = mNodesVec.find(id);
  if (idx == mNodesVec.end()) {
    MY_LOGE("Node does not exist\nID:%" PRIxPTR "", id);
    pthread_rwlock_unlock(&mRWLock);
    return NodeObj_T();
  }
  auto ret = idx->second.mNode;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t PipelineDAGImp::getNumOfNodes() const {
  pthread_rwlock_rdlock(&mRWLock);
  size_t ret = mNodesVec.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDAGImp::getInAdjacentNodes(NodeId_T id, NodeObjSet_T* result) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto idx = mNodesVec.find(id);
  if (mNodesVec.end() == idx) {
    MY_LOGE("Node does not exist\nID:%" PRIxPTR "", id);
    result->clear();
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }

  // let result be always empty before we put things inside
  result->clear();

  NodeWithAdj_T const& tmp = idx->second;
  NodeIdSet_T const& inAdj = tmp.mInAdj;
  size_t upperbound = inAdj.size();
  result->reserve(upperbound);
  for (size_t i = 0; i < upperbound; i++) {
    result->emplace_back(mNodesVec.at(inAdj.at(i)).mNode);
  }
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDAGImp::getInAdjacentNodesReqCnt(NodeId_T id, MUINT32* count) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto idx = mNodesVec.find(id);
  if (mNodesVec.end() == idx) {
    MY_LOGE("Node does not exist\nID:%" PRIxPTR "", id);
    *count = 0;
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }

  NodeWithAdj_T const& tmp = idx->second;
  *count = tmp.mInAdjReqCnt;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDAGImp::addInAdjacentNodesReqCnt(NodeId_T id) {
  pthread_rwlock_wrlock(&mRWLock);
  auto idx = mNodesVec.find(id);
  if (mNodesVec.end() == idx) {
    MY_LOGE("Node does not exist\nID:%" PRIxPTR "", id);
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }

  idx->second.mInAdjReqCnt += 1;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineDAGImp::getOutAdjacentNodes(NodeId_T id, NodeObjSet_T* result) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto idx = mNodesVec.find(id);
  if (mNodesVec.end() == idx) {
    MY_LOGE("Node does not exist\nID:%" PRIxPTR "", id);
    result->clear();
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }

  // let result be always empty before we put things inside
  result->clear();

  NodeWithAdj_T const& tmp = idx->second;
  NodeIdSet_T const& outAdj = tmp.mOutAdj;
  size_t upperbound = outAdj.size();
  result->reserve(upperbound);
  for (size_t i = 0; i < upperbound; i++) {
    result->emplace_back(mNodesVec.at(outAdj.at(i)).mNode);
  }
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class T>
MERROR PipelineDAGImp::evaluateToposort(Map_T const& dag, T* rToposort) {
  enum {
    NOT_VISIT,
    VISITED,
    SORTED,
  };

  struct DFS {
    static MERROR run(Map_T const& dag,
                      NodeId_T const id,
                      std::unordered_map<NodeId_T, MINT>* visit,
                      T* rToposort) {
      auto iter = visit->find(id);
      if (iter == visit->end()) {
        MY_LOGE("nodeId:%#" PRIxPTR " not found @ visit", id);
        return NO_INIT;
      }
      //
      switch (iter->second) {
        case SORTED:
          return OK;
        case VISITED:
          MY_LOGE("CYCLE EXIST");
          return UNKNOWN_ERROR;
        default:
          break;
      }
      //
      iter->second = VISITED;
      auto const indexDag = dag.find(id);
      NodeIdSet_T const& outAdj = indexDag->second.mOutAdj;
      for (size_t i = 0; i < outAdj.size(); i++) {
        if (OK != DFS::run(dag, outAdj[i], visit, rToposort)) {
          return UNKNOWN_ERROR;
        }
      }
      //
      iter->second = SORTED;
      rToposort->insert(rToposort->begin(), indexDag->second.mNode);
      return OK;
    }
  };

  std::unordered_map<NodeId_T, MINT> visit;
  visit.reserve(dag.size());
  for (auto& it : dag) {
    NodeId_T const nodeId = it.first;
    visit.emplace(nodeId, NOT_VISIT);
  }
  //
  for (auto& it : visit) {
    if (NOT_VISIT == it.second) {
      NodeId_T const nodeId = it.first;
      if (OK != DFS::run(dag, nodeId, &visit, rToposort)) {
        rToposort->clear();
        return UNKNOWN_ERROR;
      }
    }
  }
  //
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
std::vector<PipelineDAGImp::NodeObj_T> const& PipelineDAGImp::getToposort()
    const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (mToposort.empty()) {
    mToposort.reserve(mNodesVec.size());
    evaluateToposort(mNodesVec, &mToposort);
  }
  pthread_rwlock_unlock(&mRWLock);
  return mToposort;
}

/******************************************************************************
 *result: a topological list of nodeObj_T of DAG, return -1L if CYCLE EXIST
 ******************************************************************************/
MERROR
PipelineDAGImp::getTopological(
    std::list<PipelineDAGImp::NodeObj_T>* result) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  result->clear();
  auto ret = (NSCam::OK == evaluateToposort(mNodesVec, result)) ? NSCam::OK
                                                                : MERROR(-1L);
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *Get IDs of nodes that are not reachable from root, return 0 as normal
 ******************************************************************************/
MERROR
PipelineDAGImp::getOrphanNodes(NodeIdSet_T* orphanNodes,
                               NodeIdSet_T* connectedNodes) const {
  pthread_rwlock_rdlock(&mRWLock);
  std::vector<bool> checkList;
  checkList.reserve(mNodesVec.size());
  checkList.insert(checkList.begin(), mNodesVec.size(), 0);
  bool* arr = new bool[checkList.size()];
  std::copy(std::begin(checkList), std::end(checkList), arr);
  // check node exist from each root nodes
  for (size_t i = 0; i < mRootIds.size(); i++) {
    if (0 > checkListDFS(mRootIds[i], arr)) {
      orphanNodes->clear();
      MY_LOGE("Accessing ID that does not exist");
      delete[] arr;
      pthread_rwlock_unlock(&mRWLock);
      return -1L;
    }
  }
  int i = 0;
  for (auto& it : mNodesVec) {
    if (!arr[i]) {
      orphanNodes->push_back(it.first);
    } else {
      connectedNodes->push_back(it.first);
    }
    i++;
  }
  delete[] arr;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *DFS used only in getOrphanNodes()
 ******************************************************************************/
MERROR
PipelineDAGImp::checkListDFS(NodeId_T id, bool* checkList) const {
  auto idx = mNodesVec.find(id);
  if (mNodesVec.end() == idx) {
    MY_LOGE("Node ID=%x" PRIxPTR " does not exist", idx->first);
    return -1L;
  }
  checkList[std::distance(mNodesVec.begin(), idx)] = true;
  NodeWithAdj_T node = idx->second;
  for (size_t i = 0; i < node.mOutAdj.size(); i++) {
    if (0 > checkListDFS(node.mOutAdj.at(i), checkList)) {
      return -1L;
    }
  }
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
bool adjCompare(const PipelineDAGImp::NodeWithAdj_T& rhs,
                const PipelineDAGImp::NodeWithAdj_T& lhs) {
  if (rhs.mInAdj.size() > lhs.mInAdj.size()) {
    return true;
  }
  return false;
}

/******************************************************************************
 * Use BFS to find path for node id, return -1L if no path found
 ******************************************************************************/
MERROR
PipelineDAGImp::findPathBFS(NodeId_T id,
                            NodeIdSet_T* checkList,
                            std::shared_ptr<IPipelineDAG> newDAG) const {
  // Used for BFS
  std::vector<NodeId_T> queue;
  //
  // To keep track of the path
  std::unordered_map<NodeId_T, NodeId_T> parent;
  parent.reserve(mNodesVec.size());
  queue.reserve(mNodesVec.size());

  // insert initial node
  queue.emplace_back(id);
  // BFS
  while (!queue.empty()) {
    NodeIdSet_T const& inAdj = mNodesVec.at(queue.back()).mInAdj;
    for (size_t i = 0; i < inAdj.size(); i++) {
      // If it is inside checkList, it must be reachable from root
      auto index = std::find(checkList->begin(), checkList->end(), inAdj.at(i));
      if (checkList->end() != index) {
        NodeId_T src = inAdj.at(i);
        NodeId_T dst = queue.back();
        //
        // get path and node by traversing parant
        do {
          NodeObj_T const& node = mNodesVec.at(dst).mNode;
          newDAG->addNode(node.id, node.val);
          newDAG->addEdge(src, dst);
          checkList->push_back(dst);
          src = dst;
          //
          // test ending condition
          if (dst != id) {
            dst = parent.at(dst);
          }
        } while (src != id);
        return OK;
      }
      // If it is not in checkList, add into queue to do BFS
      parent.emplace(inAdj.at(i), queue.back());
      queue.insert(queue.begin(), inAdj.at(i));
    }
    // Finish this node
    queue.pop_back();
  }
  return -1L;
}

/******************************************************************************
 * Sort vector by inAdj and call BFS, return -1L if no path found
 ******************************************************************************/
MERROR
PipelineDAGImp::getNodesAndPathsForNewDAG(
    NodeIdSet_T* orphanNodes,
    NodeIdSet_T* checkList,
    std::shared_ptr<IPipelineDAG> newDAG) const {
  pthread_rwlock_rdlock(&mRWLock);
  std::vector<NodeWithAdj_T> nodesSortedByInAdj;
  nodesSortedByInAdj.reserve(orphanNodes->size());
  //
  // Sort orphanNodes by its in-coming nodes in ascending order.
  // In order to minimize number of nodes, we need to make all nodes
  // reachable from root in the new DAG.
  for (size_t i = 0; i < orphanNodes->size(); i++) {
    nodesSortedByInAdj.emplace_back(mNodesVec.at(orphanNodes->at(i)));
  }
  std::sort(nodesSortedByInAdj.begin(), nodesSortedByInAdj.end(), adjCompare);

  // Do the algorithm with BFS
  for (size_t i = 0; i < nodesSortedByInAdj.size(); i++) {
    if (0 > findPathBFS(nodesSortedByInAdj.at(i).mNode.id, checkList, newDAG)) {
      MY_LOGE("No path found for node ID:%" PRIxPTR,
              nodesSortedByInAdj.at(i).mNode.id);
      pthread_rwlock_unlock(&mRWLock);
      return -1L;
    }
  }
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineDAGImp::dump(std::vector<std::string>* rLogs) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  rLogs->resize(mNodesVec.size() + 2);
  std::vector<std::string>::iterator it = rLogs->begin();
  //
  {
    std::string& str = *it++;
    str += base::StringPrintf("Toposort:");
    for (size_t i = 0; i < mToposort.size(); i++) {
      str += base::StringPrintf(" %#" PRIxPTR "", mToposort[i].id);
    }
  }
  //
  for (size_t i = 0; i < mRootIds.size(); i++) {
    *it++ = base::StringPrintf("RootId:%#" PRIxPTR " Nodes:#%zu", mRootIds[i],
                               mNodesVec.size());
  }
  for (auto& iter : mNodesVec) {
    NodeId_T const nodeId = iter.first;
    NodeWithAdj_T const& node = iter.second;
    //
    std::string& str = *it++;
    str += base::StringPrintf("[%#" PRIxPTR "] inAdj: ", nodeId);
    for (size_t j = 0; j < node.mInAdj.size(); j++) {
      str += base::StringPrintf("%#" PRIxPTR " ", node.mInAdj[j]);
    }
    str += base::StringPrintf("outAdj: ");
    for (size_t j = 0; j < node.mOutAdj.size(); j++) {
      str += base::StringPrintf("%#" PRIxPTR " ", node.mOutAdj[j]);
    }
  }
  pthread_rwlock_unlock(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineDAGImp::dump() const {
  std::vector<std::string> logs;
  dump(&logs);
  for (size_t i = 0; i < logs.size(); i++) {
    CAM_LOGD("%s", logs[i].c_str());
  }
}
