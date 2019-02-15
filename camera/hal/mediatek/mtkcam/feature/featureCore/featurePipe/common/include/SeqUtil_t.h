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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SEQUTIL_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SEQUTIL_T_H_

#include "MtkHeader.h"
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class VISIBILITY_PUBLIC SequentialQueueBase {
 public:
  SequentialQueueBase();
  virtual ~SequentialQueueBase();
};

template <typename T, class SeqConverter>
class SequentialQueue : public SequentialQueueBase {
 public:
  SequentialQueue();
  explicit SequentialQueue(unsigned seq);
  virtual ~SequentialQueue();

 public:  // queue member
  bool empty() const;
  size_t size() const;
  void enque(const T& val);
  bool deque(T* val);

 private:
  class DataGreater {
   public:
    bool operator()(const T& lhs, const T& rhs) const;

   private:
    SeqConverter mSeqConverter;
  };

 private:
  std::priority_queue<T, std::vector<T>, DataGreater> mQueue;
  SeqConverter mSeqConverter;
  unsigned mSeq;
};

typedef std::shared_ptr<SequentialQueueBase> SeqQueuePtr;

template <typename Handler_T, class Enable = void>  // default
class SequentialHandler {
 public:
  typedef typename Handler_T::DataID DataID_T;

 public:
  SequentialHandler();
  explicit SequentialHandler(unsigned seq);
  virtual ~SequentialHandler();

  template <typename DATA_T>
  MBOOL onData(DataID_T id, const DATA_T& data, Handler_T* handler);

  template <typename DATA_T>
  MBOOL onData(DataID_T id, DATA_T* data, Handler_T* handler);

  MVOID clear();
};

template <typename Handler_T>
class SequentialHandler<Handler_T,
                        typename std::enable_if<Handler_T::supportSeq>::type> {
 public:
  typedef typename Handler_T::DataID DataID_T;

 public:
  SequentialHandler();
  explicit SequentialHandler(unsigned seq);
  virtual ~SequentialHandler();

  // not thread safe
  template <typename DATA_T>
  MBOOL onData(DataID_T id, const DATA_T& data, Handler_T* handler);

  // not thread safe
  template <typename DATA_T>
  MBOOL onData(DataID_T id, DATA_T* data, Handler_T* handler);

  MVOID clear();

 private:
  class HandlerSeqConverter {
   public:
    template <typename DATA_T>
    unsigned operator()(const DATA_T& data) const;
  };

  template <typename DATA_T, class SeqConverter>
  SequentialQueue<DATA_T, SeqConverter>* getSeqQueue(DataID_T id);

  typedef std::map<std::string, SeqQueuePtr> CONTAINER;
  CONTAINER mQueueMap;
  const unsigned mSeq;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SEQUTIL_T_H_
