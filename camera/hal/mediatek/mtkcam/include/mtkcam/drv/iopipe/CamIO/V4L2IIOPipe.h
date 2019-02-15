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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_CAMIO_V4L2IIOPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_CAMIO_V4L2IIOPIPE_H_

#include <atomic>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
//
#include "V4L2IHalCamIO.h"
#include "mtkcam/drv/iopipe/PortMap.h"
#include "mtkcam/utils/module/module.h"

/******************************************************************************
 *
 ******************************************************************************/

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {
class V4L2NormalPipe;
class V4L2PipeFactory;
}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam

NSCam::NSIoPipe::NSCamIOPipe::V4L2PipeFactory* getV4L2PipeFactory();

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

#define MSG_LEN 128

class V4L2IIOPipe {
 public:
  V4L2IIOPipe(const V4L2IIOPipe&) = delete;
  V4L2IIOPipe& operator=(const V4L2IIOPipe&) = delete;
  virtual ~V4L2IIOPipe() = default;

  virtual MBOOL init(const PipeTag pipe_tag) = 0;
  virtual MBOOL uninit() = 0;
  virtual MBOOL enque(const QBufInfo& r_qbuf) = 0;
  virtual MBOOL deque(const QPortID& q_qport,
                      QBufInfo* p_qbuf,
                      MUINT32 u4TimeoutMs = 0xFFFFFFFF) = 0;
  virtual MBOOL configPipe(
      const QInitParam& v_ports,
      std::map<int, std::vector<std::shared_ptr<IImageBuffer> > >*
          map_vbuffers = nullptr) = 0;
  virtual MBOOL sendCommand(const int cmd,
                            MINTPTR arg1,
                            MINTPTR arg2,
                            MINTPTR arg3) = 0;
  virtual MBOOL start() = 0;
  virtual MBOOL stop() = 0;

 protected:
  V4L2IIOPipe() = default;
};

/* EventPip derived from V4L2IIOPipe but has only few methods */
class V4L2IEventPipe : public V4L2IIOPipe {
 public:
  virtual ~V4L2IEventPipe() = default;

 public:
  /**
   * Initialize V4L2IEventPipe. Notice that, V4L2IEventPipe is always be
   * initialized after any other V4L2IIOPipe has been initialized.
   *  @return     MTRUE for ok, MFALSE for false.
   */
  virtual MBOOL init() = 0;
  /**
   * Manually send the event, if caller has invoked wait (and the caller thread
   * was sleeping), it will be waked up immediately.
   *  @param[in] eType: The given event to signal.
   *  @return Return 0 for successes, otherwise check the error codes.
   */
  virtual int signal(EPipeSignal eType) = 0;

  /**
   * Wait the given event.
   *  @param[in] eType: The event to be waited.
   *  @param[in] timed_out_ms: Time out in milliseconds.
   *  @note If there are multiple callers are wait the given event, once
   *        hardware fires the given event, all threads of caller would be
   *        waked up.
   *  @return Return 0 for successes, otherwise, check the error codes.
   */
  virtual int wait(EPipeSignal eType, size_t timed_out_ms = 0xFFFFFFFF) = 0;

 private:
  using V4L2IIOPipe::configPipe;
  using V4L2IIOPipe::deque;
  using V4L2IIOPipe::enque;
  using V4L2IIOPipe::init;
  using V4L2IIOPipe::sendCommand;

 private:
  /* cannot be copied */
  V4L2IEventPipe(const V4L2IEventPipe&) = delete;
  V4L2IEventPipe& operator=(const V4L2IEventPipe&) = delete;

 protected:
  V4L2IEventPipe() = default;
};

/******************************************************************************
 *
 ******************************************************************************/
class IV4L2PipeFactory : public mtkcam_module {
 public:  ////                    Operations.
  /**
   * @brief Module ID
   */
  static MUINT32 moduleId() {
    return MTKCAM_MODULE_ID_DRV_IOPIPE_CAMIO_NORMALPIPE;
  }

  /**
   * @brief Return the singleton of this module.
   */
  static auto get() {
    return reinterpret_cast<IV4L2PipeFactory*>(
        GET_MTKCAM_MODULE_EXTENSION(moduleId()));
  }
  IV4L2PipeFactory(const IV4L2PipeFactory&) = delete;
  IV4L2PipeFactory& operator=(const IV4L2PipeFactory&) = delete;
  IV4L2PipeFactory() {
    get_module_api_version = nullptr;
    get_module_id = moduleId;
    get_module_extension = nullptr;
    get_sub_module_api_version = nullptr;
  }
  virtual ~IV4L2PipeFactory() = default;

  /**
   * @brief Create (or get) a sub-module instance.
   * @param[in] sensorIndex:  The sensor index.
   * @param[in] szCallerName: The caller name.
   * @param[in] apiVersion:   The sub-module API version.
   * @param[out] rpInstance:  The created sub-module instance. Callers have to
   *                          cast it to the real type based on the specified
   *                          sub-module api version.
   * @return A smart pointer holds the pointer of sub-module. Basically,
   *         this method always returned with a valid instance.
   */
  virtual std::shared_ptr<V4L2IIOPipe> getSubModule(IspPipeType pipe_type,
                                                    MUINT32 sensorIndex,
                                                    char const* szCallerName,
                                                    MUINT32 apiVersion = 0) = 0;
  virtual MBOOL query(MUINT32 port_idx,
                      MUINT32 cmd,
                      const NormalPipe_QueryIn& input,
                      NormalPipe_QueryInfo* query_info) const = 0;
  virtual MBOOL query(MUINT32 port_idx,
                      MUINT32 cmd,
                      MINT imgFmt,
                      NormalPipe_QueryIn const& input,
                      NormalPipe_QueryInfo* query_info) const = 0;
  virtual MBOOL query(MUINT32 cmd __unused, MUINTPTR IO_struct __unused) const {
    return MFALSE;
  }

 public:
  /**
   * @brief Create (or get) the V4L2IEventPipe instance.
   * @param[in] sensorIndex:  The sensor index.
   * @param[in] szCallerName: The caller name.
   * @param[out] rpInstance:  The created sub-module instance. Callers have to
   *                          cast it to the real type based on the specified
   *                          sub-module api version.
   * @return A smart pointer holds the pointer of V4L2IEventPipe. Basically,
   *         this method always returned with a valid instance.
   */
  virtual std::shared_ptr<V4L2IEventPipe> getEventPipe(
      MUINT32 sensorIndex,
      const char* szCallerName,
      MUINT32 apiVersion = 0) = 0;
};
/******************************************************************************
 *
 ******************************************************************************/
}  //  namespace NSCamIOPipe
}  //  namespace NSIoPipe
}  //  namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_IOPIPE_CAMIO_V4L2IIOPIPE_H_
