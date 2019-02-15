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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINEDAG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINEDAG_H_
//
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
//
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * Type of Camera Pipeline Node Id.
 */
typedef MINTPTR Pipeline_NodeId_T;
//
typedef NSCam::v3::Pipeline_NodeId_T NodeId_T;

template <typename T>
struct Set : public std::vector<T> {
  typedef typename std::vector<T>::iterator iter;
  typedef typename std::vector<T>::const_iterator const_iter;
  //
  Set& add(T const& item) {
    if (indexOf(item) < 0) {  // remove redundancy
      this->push_back(item);
    }
    return *this;
  }
  Set& add(Set<T> const& set) {
    typename std::vector<T>::const_iterator iter = set.begin();
    while (iter != set.end()) {
      Set::add(*iter);
      iter++;
    }
    return *this;
  }
  ssize_t indexOf(T const& item) {
    for (size_t i = 0; i < this->size(); i++) {
      if (item == this->at(i)) {
        return i;
      }
    }
    return -1;
  }
};

typedef Set<NodeId_T> NodeSet;

/**
 * An interface of pipeline directed acyclic graph.
 */
class IPipelineDAG {
 public:  ////                            Definitions.
  template <class _NodeId_T_, class _NodeVal_T_>
  struct TNodeObj {
    _NodeId_T_ id;
    _NodeVal_T_ val;
    //
    TNodeObj() : id(-1L), val(-1L) {}
  };

  typedef ssize_t NodeVal_T;                        // Node Value
  typedef Pipeline_NodeId_T NodeId_T;               // Node Id
  typedef std::vector<NodeId_T> NodeIdSet_T;        // Node Id Set
  typedef TNodeObj<NodeId_T, NodeVal_T> NodeObj_T;  // Node Object
  typedef std::vector<NodeObj_T> NodeObjSet_T;      // Node Object Set

  struct Edge {
    NodeId_T src;
    NodeId_T dst;
    explicit Edge(NodeId_T src = -1L, NodeId_T dst = -1L)
        : src(src), dst(dst) {}
  };

 public:  ////                            Operations.
  virtual ~IPipelineDAG() {}

  /**
   * Create the graph.
   *
   * @return
   *      A pointer to a newly-created instance.
   */
  static IPipelineDAG* create();

  /**
   * Clone the graph.
   *
   * @return
   *      A pointer to a newly-cloned instance.
   */
  virtual IPipelineDAG* clone() const = 0;

  /**
   * Clone the graph, with a given set of nodes reserved.
   * Any node beyond the given set of nodes must be removed.
   *
   * @param[in] ids: a set of node ids.
   *
   * @return
   *      A pointer to a newly-cloned instance.
   */
  virtual IPipelineDAG* clone(NodeIdSet_T const& ids) const = 0;

  /**
   * Add a node to the graph.
   *
   * @param[in] id: node id
   *
   * @param[in] val: node value
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR addNode(NodeId_T id, NodeVal_T const& val = -1L) = 0;

  /**
   * Remove a node and its associated edge.
   *
   * @param[in] id: node id
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR removeNode(NodeId_T id) = 0;

  /**
   * Add a directed edge to the graph, where a directed edge is from a given
   * source node to a given destination node.
   *
   * @param[in] id_src: the node id of source.
   *
   * @param[in] id_dst: the node id of destination.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR addEdge(NodeId_T id_src, NodeId_T id_dst) = 0;

  /**
   * Remove a directed edge from the graph, where a directed edge is from a
   * given source node to a given destination node.
   *
   * @param[in] id_src: the node id of source.
   *
   * @param[in] id_dst: the node id of destination.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR removeEdge(NodeId_T id_src, NodeId_T id_dst) = 0;

  /**
   * Set a node as the root of the graph.
   *
   * @param[in] id: node id
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR setRootNode(NodeSet roots) = 0;

  /**
   * Set the value of a specified node.
   *
   * @param[in] id: node id
   *
   * @param[in] val: node value
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR setNodeValue(NodeId_T id, NodeVal_T const& val) = 0;

  /**
   * Dump.
   */
  virtual MVOID dump() const = 0;
  virtual MVOID dump(std::vector<std::string>* rLogs) const = 0;

 public:  ////                            Attributes.
  /**
   * Get nodes and paths that are needed to make nodes inside the new DAG
   * reachable from root.
   *
   * @param[in]  orphanNodes: IDs of nodes that are not reachable from root
   *
   * @param[in]  checkList: Nodes that are reachable from root
   *
   * @param[out] newDAG: DAG with nodes and edges inserted
   *
   * @return
   *      0 indicates success; otherwise, errors.
   */
  virtual MERROR getNodesAndPathsForNewDAG(
      NodeIdSet_T* orphanNodes,
      NodeIdSet_T* checkList,
      std::shared_ptr<IPipelineDAG> newDAG) const = 0;

  /**
   * Get nodes that are not reachable from root.
   *
   * @param[out] orphanNodes: IDs of nodes that are not reachable from root
   *
   * @param[out] connectedNodes: IDs of nodes that are reachable from root
   *
   * @return
   *      0 indicates success; otherwise, errors.
   *
   */
  virtual MERROR getOrphanNodes(NodeIdSet_T* orphanNodes,
                                NodeIdSet_T* connectedNodes) const = 0;

  /**
   * Get the topological order list of the graph.
   *
   * @param[out] result: list of node obj in topological order
   *
   * @return
   *      0 indicates success; -1L indicates cycle exists in graph.
   */

  virtual MERROR getTopological(std::list<NodeObj_T>* result) const = 0;

  /**
   * Get the topological sort of the graph.
   *
   * @return
   *      An empty vector indicates that the graph is cyclic.
   */
  virtual std::vector<NodeObj_T> const& getToposort() const = 0;

  /**
   * Get the root node of the graph.
   *
   * @return
   *      a node obj type indicates the root node
   */
  virtual std::vector<NodeObj_T> getRootNode() const = 0;

  /**
   * Get node of certain id of the graph.
   *
   * @param[in] id: node id
   *
   * @return
   *      a node obj type of id node of the graph.
   */
  virtual NodeObj_T getNode(NodeId_T id) const = 0;

  /**
   * Get edges of IDs of the graph.
   *
   * @param[in] ids: node id set
   *
   * @param[out] result: vector of edges related to those node
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR getEdges(std::vector<Edge>* result) const = 0;

  /**
   * Get the number of nodes of the graph.
   *
   * @return
   *      size of the graph in size_t.
   */
  virtual size_t getNumOfNodes() const = 0;

  /**
   * Get the in-coming nodes of node id.
   *
   * @param[in] id: node id
   *
   * @param[out] result: vector of node obj type of the in-coming nodes
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR getInAdjacentNodes(NodeId_T id,
                                    NodeObjSet_T* result) const = 0;

  /**
   * Get in-coming request counter of node id.
   *
   * @param[in] id: node id
   *
   * @param[out] result: in-coming request counter of node id
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR getInAdjacentNodesReqCnt(NodeId_T id,
                                          MUINT32* count) const = 0;

  /**
   * Add in-coming request counter of node id.
   *
   * @param[in] id: node id
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR addInAdjacentNodesReqCnt(NodeId_T id) = 0;

  /**
   * Get the out-going nodes of node id.
   *
   * @param[in] id: node id
   *
   * @param[out] result: vector of node obj type of the out-going nodes
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR getOutAdjacentNodes(NodeId_T id,
                                     NodeObjSet_T* result) const = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINEDAG_H_
