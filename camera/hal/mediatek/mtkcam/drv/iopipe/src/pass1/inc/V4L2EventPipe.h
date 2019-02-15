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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2EVENTPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2EVENTPIPE_H_

#include "V4L2IHalCamIO.h"
#include "V4L2IIOPipe.h"
#include "V4L2PipeBase.h"

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

class V4L2EventPipe : public V4L2PipeBase, public V4L2IEventPipe {
 public:
  const int LISTENED_NODE_ID = v4l2::V4L2StreamNode::ID_P1_SUBDEV;

  /* implementations of the interface V4L2IEventPipe */
 public:
  MBOOL init() override;
  int signal(EPipeSignal eType) override;
  int wait(EPipeSignal eType, size_t timed_out_ms = 0xFFFFFFFF) override;

  /* implementations of the interface V4L2IIOPipe */
 public:
  MBOOL uninit() override;
  MBOOL start() override;
  MBOOL stop() override;

  /* implementations of the interface V4L2IIOPipe but no need */
 private:
  MBOOL init(const PipeTag pipe_tag) override { return MFALSE; }
  MBOOL enque(QBufInfo const&) override { return MFALSE; }
  MBOOL deque(QPortID const&, QBufInfo*, MUINT32) override { return MFALSE; }
  MBOOL configPipe(
      QInitParam const&,
      std::map<int, std::vector<std::shared_ptr<IImageBuffer> > >*) override {
    return MFALSE;
  };
  MBOOL sendCommand(int, MINTPTR, MINTPTR, MINTPTR) override { return MFALSE; };
  status_t notifyPollEvent(PollEventMessage* poll_msg) override;

 public:
  explicit V4L2EventPipe(MUINT32 sensor_idx, const char* sz_caller_name);
  V4L2EventPipe() = delete;
  V4L2EventPipe(const V4L2EventPipe&) = delete;
  V4L2EventPipe& operator=(const V4L2EventPipe&) = delete;
  ~V4L2EventPipe();

 private:
  /* events subscriptions */
  int subscribeEvents_locked();
  int unsubscribeEvents_locked();

  /**
   * Deque an event from device.
   *  @param[out] r_dequed_event_type     The type of dequed event.
   *  @return 0 for dequed successfully.
   */
  status_t dequeEvents_locked(uint32_t* r_dequed_event_type);

  /* state machine */
 private:
  /* state */
  enum {
    STATE_UNINITED = 0,
    STATE_INITED,
    STATE_SUBSCRIB,
    //
    STATE_NUM,
  };

  /* action */
  enum {
    ACT_INIT = 0,  // related to init
    ACT_START,
    ACT_LISTEN,
    ACT_STOP,
    ACT_UNINIT,
    //
    ACT_NUM,
  };

  // state machine: [action][current]
  const int STATE_MACHINE[ACT_NUM][STATE_NUM] = {
      //|- uninited ----|--- inited  ----|---- subscrib ----|
      {
          STATE_INITED,
          -1,
          -1,
      },  // init
      {
          -1,
          STATE_SUBSCRIB,
          -1,
      },  // start
      {
          -1,
          -1,
          STATE_SUBSCRIB,
      },  // listen
      {
          -1,
          -1,
          STATE_INITED,
      },  // stop
      {
          -1,
          STATE_UNINITED,
          -1,
      }  // uninit
  };

  /* state controls */
  inline void transitState_locked(int current, int action) {
    m_state.store(STATE_MACHINE[action][current], std::memory_order_relaxed);
  }

  inline bool isTransitable_locked(int current, int action) {
    return (0 <= STATE_MACHINE[action][current]);
  }

 private:
  MUINT32 m_sensorIdx;

  /* saves node name */
  std::string m_node_name;
  std::atomic<int> m_state;
  std::mutex m_opLock;
  std::shared_ptr<V4L2VideoNode> m_p1_subdev;

  std::mutex m_pollerLock;

  struct ___S {
    std::mutex mx;
    std::condition_variable cond;
    std::atomic<bool> invalidated;
    ___S() : invalidated(false) {}
  } m_eventsCond[EPipeSignal_NUM];
};  // class V4L2EventPipe
}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2EVENTPIPE_H_
