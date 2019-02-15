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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_WAITQUEUE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_WAITQUEUE_T_H_

#include "MtkHeader.h"

#include <condition_variable>
#include <mutex>

#include <functional>
#include <list>
#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "SyncUtil.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class WaitHub;

class IWaitQueue {
 public:
  class VISIBILITY_PUBLIC Index {
   public:
    unsigned mID;
    unsigned mPriority;

    Index();
    Index(unsigned id, unsigned priority);
    class Less {
     public:
      bool operator()(const Index& lhs, const Index& rhs) const;
    };
  };
  typedef std::multiset<unsigned> IDSet;
  typedef std::multiset<Index, Index::Less> IndexSet;

  virtual ~IWaitQueue() {}
  virtual MBOOL isReady() const = 0;
  virtual MVOID setWaitHub(WaitHub* hub) = 0;
  virtual MVOID clear() = 0;
  virtual size_t size() const = 0;
  virtual IDSet getIDSet() const = 0;
  virtual IndexSet getIndexSet() const = 0;
};

class VISIBILITY_PUBLIC WaitHub {
 public:
  enum Signal {
    SIGNAL_STOP = (1 << 0),
    SIGNAL_CB = (1 << 1),
    SIGNAL_DATA = (1 << 2),
    SIGNAL_IDLE = (1 << 3),
    SIGNAL_IDLE_CB = (1 << 4),
    SIGNAL_SYNC_CB = (1 << 5),
    SIGNAL_NEED_SYNC_BREAK = (1 << 6),
    SIGNAL_DRY_RUN = (1 << 7),
    SIGNAL_DRY_RUN_ONCE = (1 << 8),
  };
  explicit WaitHub(const char* name);
  virtual ~WaitHub();

  MVOID addWaitQueue(IWaitQueue* queue);
  MVOID flushQueues();

  MVOID triggerSignal(MUINT32 signal);
  MVOID resetSignal(MUINT32 signal);
  MVOID resetSignal();
  MVOID shiftSignal(MUINT32 src, MUINT32 dst);

  MVOID signalEnque();
  MVOID registerEnqueCB(const std::shared_ptr<NotifyCB>& cb);

  MBOOL waitAllQueue();
  MBOOL waitAnyQueue();
  MBOOL waitAllQueueSync(MUINT32* id);
  MBOOL peakAllQueue();
  MBOOL peakAnyQueue();

  template <typename F, typename T>
  MBOOL waitCondition(F func, T* data);
  template <typename F, typename T>
  MBOOL peakCondition(F func, T* data);

  MUINT32 waitSignal();
  MBOOL isAllQueueEmpty();

  MVOID dumpWaitQueueInfo();

 private:
  MBOOL isAllQueueReady();
  MBOOL isAnyQueueReady();
  MBOOL isAllQueueReadySync(MUINT32* id);

 protected:
  char mName[128];

 private:
  typedef std::vector<IWaitQueue*> CONTAINER;
  mutable std::mutex mMutex;
  std::condition_variable mCondition;
  CONTAINER mQueues;
  std::shared_ptr<NotifyCB> mEnqueCB;
  MUINT32 mSignal;
};

template <typename T>
class WaitQueue : public IWaitQueue {
 public:
  WaitQueue();
  virtual ~WaitQueue();

 public:  // queue member
  bool empty() const;
  void enque(const T& val);
  bool deque(T* val);

 public:  // IWaitQueue member
  virtual MBOOL isReady() const;
  virtual MVOID setWaitHub(WaitHub* hub);
  virtual size_t size() const;
  virtual MVOID clear();
  virtual IWaitQueue::IDSet getIDSet() const;
  virtual IWaitQueue::IndexSet getIndexSet() const;

 private:
  mutable std::mutex mMutex;
  std::queue<T> mQueue;
  WaitHub* mHub;
};  // class WaitQueue

template <typename T, class IndexConverter>
class PriorityWaitQueue : public IWaitQueue {
 public:
  typedef std::vector<T> CONTAINER_TYPE;

  PriorityWaitQueue();
  virtual ~PriorityWaitQueue();

 public:  // queue member
  bool empty() const;
  void enque(const T& val);
  bool deque(T* val);
  bool deque(unsigned id, T* val);
  CONTAINER_TYPE getContents() const;

 public:  // IWaitQueue member
  virtual MBOOL isReady() const;
  virtual MVOID setWaitHub(WaitHub* hub);
  virtual size_t size() const;
  virtual MVOID clear();
  virtual IWaitQueue::IDSet getIDSet() const;
  virtual IndexSet getIndexSet() const;

 private:
  class DataLess {
   public:
    bool operator()(const T& lhs, const T& rhs) const;
  };

 private:
  mutable std::mutex mMutex;
  WaitHub* mHub;
  typedef std::multiset<T, DataLess> DataSet;
  DataSet mDataSet;
  IDSet mIDSet;
  mutable bool mIndexSetValid;
  mutable IndexSet mIndexSet;
};  // class PriorityWaitQueue

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_WAITQUEUE_T_H_
