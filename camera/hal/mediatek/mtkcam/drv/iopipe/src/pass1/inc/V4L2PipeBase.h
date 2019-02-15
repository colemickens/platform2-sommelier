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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2PIPEBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2PIPEBASE_H_

#include <condition_variable>
#include <cutils/compiler.h>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "V4L2IIOPipe.h"
#include "V4L2PipeMgr.h"
#include "PollerThread.h"
#include "V4L2StreamNode.h"
#include "MediaCtrlConfig.h"
#include "mtkcam/drv/IHalSensor.h"
#include "mtkcam/drv/iopipe/Port.h"
#include "mtkcam/drv/iopipe/PortMap.h"

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

/* set deque cmd timeout interval to 3000 ms */
constexpr int DQ_TIME_OUT_MS = 3000;

struct SensorInfo {
  MUINT32 idx;
  MUINT32 typeformw;
  MUINT32 dev_id;  // main/sub/main0/...
  IHalSensor::ConfigParam config;
  SensorStaticInfo stt_info;       // static
  SensorDynamicInfo dynamic_info;  // Dynamic
  MUINTPTR occupied_owner;
};

struct PlatSensorsInfo {
  MUINT32 existed_sensor_cnt;
  SensorInfo sensor_info[IOPIPE_MAX_SENSOR_CNT];
};

class V4L2PipeBase : public V4L2IIOPipe,
                     public NSCam::v4l2::IPollEventListener {
  friend class V4L2PipeFactory;

 public:
  V4L2PipeBase(const V4L2PipeBase&) = delete;
  V4L2PipeBase& operator=(const V4L2PipeBase&) = delete;
  MBOOL init(const PipeTag pipe_tag) override;
  MBOOL uninit() override;
  MBOOL enque(const QBufInfo& r_qbuf) override;
  MBOOL deque(const QPortID& q_qport,
              QBufInfo* p_qbuf,
              MUINT32 u4TimeoutMs = DQ_TIME_OUT_MS) override;
  MBOOL configPipe(const QInitParam& init_param,
                   std::map<int, std::vector<std::shared_ptr<IImageBuffer>>>*
                       map_vbuffers = nullptr) override;
  MBOOL sendCommand(const int cmd,
                    MINTPTR arg1,
                    MINTPTR arg2,
                    MINTPTR arg3) override;
  MBOOL start() override;
  MBOOL stop() override;
  status_t notifyPollEvent(PollEventMessage* poll_msg) override;

 protected:
  V4L2PipeBase(IspPipeType pipe_type,
               MUINT32 sensor_idx,
               const char* sz_caller_name)
      : mp_pipe_factory(nullptr),
        m_pipe_type(pipe_type),
        m_sensor_idx(sensor_idx),
        m_sensor_config_params{},
        m_name(sz_caller_name),
        m_fsm_state(kState_Uninit) {}

  virtual ~V4L2PipeBase() = default;

  struct BurstFrameQ {
    BurstFrameQ() = default;
    explicit BurstFrameQ(BufInfo vec_buf) : mv_buf{vec_buf} {}
    std::vector<BufInfo> mv_buf;
  };

  /* state */
  enum {
    kState_Uninit = 0,
    kState_Init,
    kState_Config,
    kState_Streaming,
    kState_Streamoff,
    kState_Num
  };

  /* action */
  enum {
    kOp_Init = 0,  // related to init
    kOp_Config,
    kOp_Enque,
    kOp_Start,
    kOp_Deque,
    kOp_Stop,
    kOp_Uninit,
    kOp_Num
  };

  // state machine: [action][current]
  const int kPipeFsmTable[kOp_Num][kState_Num] = {
      // |---Uninit---|---Init---|---Config---|---Streaming---|---Streamoff---|
      {kState_Init, -1, -1, -1, -1},                          // op init
      {-1, kState_Config, -1, -1, -1},                        // op config
      {-1, -1, kState_Config, kState_Streaming, -1},          // op enque
      {-1, -1, kState_Streaming, -1, -1},                     // op start
      {-1, -1, -1, kState_Streaming, -1},                     // op deque
      {-1, -1, -1, kState_Streamoff, -1},                     // op stop
      {-1, kState_Uninit, kState_Uninit - 1, kState_Uninit},  // op uninit
  };

  /* state controls */
  inline void updateFsm(int current, int action) {
    m_fsm_state.store(kPipeFsmTable[action][current],
                      std::memory_order_relaxed);
  }

  inline bool checkFsm(int current, int action) {
    return (0 <= kPipeFsmTable[action][current]);
  }

  /* arg1 is dma port index, arg2 is frame package of each enque  */
  std::unordered_map<int, std::deque<BurstFrameQ>> m_map_enq_ctnr;
  std::unordered_map<int, std::deque<BurstFrameQ>> m_map_deq_ctnr;

  /* available video node queried from pipe mgr, each node is general purpose */
  std::vector<std::shared_ptr<NSCam::v4l2::V4L2StreamNode>> mv_active_node;

  /* mapping stream node to dma port index */
  std::map<int, std::shared_ptr<NSCam::v4l2::V4L2StreamNode>> m_map_node;

  /* for v4l2 topology query usage */
  std::shared_ptr<V4L2PipeMgr> msp_pipev4l2mgr;

  std::unique_ptr<NSCam::v4l2::PollerThread> mp_poller;
  V4L2PipeFactory* mp_pipe_factory;
  IspPipeType m_pipe_type;
  MUINT32 m_sensor_idx;
  IHalSensor::ConfigParam m_sensor_config_params;
  std::string m_name;
  std::mutex m_enq_ctnr_lock;
  std::mutex m_deq_ctnr_lock;
  std::atomic<int> m_fsm_state;
  std::mutex m_op_lock;
  std::mutex m_op_enq_lock;
  std::condition_variable m_dequeue_cv;
  std::vector<int> mv_enq_req;

  MBOOL disableLink(int port_index);

 private:
  MBOOL passBufInfo(int port_index, BufInfo* buf_info);
};

class V4L2PipeFactory : public IV4L2PipeFactory {
  friend class V4L2NormalPipe;
  friend class V4L2StatisticPipe;
  friend class V4L2TuningPipe;
  friend class V4L2EventPipe;
  friend class V4L2PipeBase;

 public:
  V4L2PipeFactory(const V4L2PipeFactory&) = delete;
  V4L2PipeFactory& operator=(const V4L2PipeFactory&) = delete;
  V4L2PipeFactory();
  ~V4L2PipeFactory() = default;

  std::shared_ptr<V4L2IIOPipe> getSubModule(IspPipeType pipe_type,
                                            MUINT32 sensorIndex,
                                            const char* szCallerName,
                                            MUINT32 apiVersion = 0) override;

  std::shared_ptr<V4L2IEventPipe> getEventPipe(
      MUINT32 sensorIndex,
      const char* szCallerName,
      MUINT32 /* apiVersion = 0 */) override;

  MBOOL query(MUINT32 port_idx,
              MUINT32 cmd,
              const NormalPipe_QueryIn& input,
              NormalPipe_QueryInfo* query_info) const override;
  MBOOL query(MUINT32 portIdx,
              MUINT32 eCmd,
              MINT imgFmt,
              NormalPipe_QueryIn const& input,
              NormalPipe_QueryInfo* queryInfo) const override;
  MBOOL query(MUINT32 cmd, MUINTPTR IO_struct) const override;

 private:
  std::shared_ptr<V4L2PipeMgr> getV4L2PipeMgr(
      MUINT32 sensor_idx, PipeTag pipe_tag = kPipeTag_Unknown);

  struct MemInfo {
    int npipe_alloc_mem_sum;
    int npipe_freed_mem_sum;
  } m_pipe_meminfo;

  PlatSensorsInfo m_plat_sensor_info;
  mutable std::mutex m_pipemgr_lock;
  std::mutex m_pipe_module_lock;
  std::weak_ptr<V4L2IIOPipe> mwp_normalpipe[kPipe_Sensor_RSVD];
  std::weak_ptr<V4L2IIOPipe> mwp_sttpipe[kPipe_Sensor_RSVD];
  std::weak_ptr<V4L2IIOPipe> mwp_sttpipe2[kPipe_Sensor_RSVD];
  std::weak_ptr<V4L2IIOPipe> mwp_tuningpipe[kPipe_Sensor_RSVD];
  std::weak_ptr<V4L2PipeMgr> mwp_v4l2pipemgr[kPipe_Sensor_RSVD];
  mutable std::mutex m_eventpipe_lock;
  std::weak_ptr<V4L2IEventPipe> mwp_eventpipe[kPipe_Sensor_RSVD];
};

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2PIPEBASE_H_
