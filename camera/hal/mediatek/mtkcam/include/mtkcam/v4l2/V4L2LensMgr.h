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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2LENSMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2LENSMGR_H_

// MTKCAM
#include <mtkcam/aaa/IHal3A.h>
// V4L2
#include <mtkcam/v4l2/ipc_queue.h>
#include <mtkcam/v4l2/V4L2DriverWorker.h>
// STL
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace v4l2 {

// using namespace NS3Av3;  // IHal3A
using NS3Av3::E3ACtrl_IPC_AF_ExchangeLensConfig;
using NS3Av3::IHal3A;
using NS3Av3::IPC_LensConfig_T;

class VISIBILITY_PUBLIC V4L2LensMgr : public V4L2DriverWorker {
 public:
  explicit V4L2LensMgr(uint32_t sensorIdx);
  virtual ~V4L2LensMgr();

 public:
  void validate();
  void invalidate();

  // re-implementations of V4L2DriverWorker
 protected:
  void job() override;

 public:
  int start() override;
  int stop() override;

 public:
  /*
   * Checks if there's a related lens driver opened (is supported AF).
   *  @return true for yes, false for not.
   */
  inline bool isLensDriverOpened() const { return (m_fd_sdev != -1); }

  // access IPC_LensConfig_T container
 private:
  void enqueConfig(const IPC_LensConfig_T& param);
  int dequeConfig(IPC_LensConfig_T* p_result);

 private:
  bool openLensDriver();
  int moveMCU(int64_t pos);

  // lens driver related
 private:
  /*
   * Opens and returns lens subdev.
   *  @param[in ] i2c_idx         The target i2c index, for example,
   *                              main sensor is 2, sub sensor is 4 ... etc.
   *  @param[out] p_r_sdev_fd     The target file descriptor of lens subdev.
   *  @return 0 for succeed, otherwise checks GNU errorcode.
   */
  static int getSubDevice(size_t i2c_idx, int* p_r_sdev_fd);

  /*
   * To query the subdev name path according the major and minor number from
   * the given subdevice descriptor.
   *  @param[in]  major           The major number.
   *  @param[in]  minor           The minor number.
   *  @param[out] r_subdev_name   Subdevice path in character string.
   *  @return 0 for succeed, otherwise checks GNU errorcode.
   */
  static int getSubDevName(int major, int minor, char* r_subdev_name);

  /*
   * Parse i2c index according to the device name. For example, device name
   * is "ov02a10 4-003d", the i2c index is 2 (0x00100).
   *             ^--------(this value is bit 2)
   *  @param dev_name             The device name.
   *  @return if not found returns -1, otherwise it's i2c index.
   */
  static int getI2Cindex(std::string dev_name);

  /* file descriptors of lens subdev, and related media device */
 private:
  size_t m_sensorIdx;
  int m_fd_sdev;

 private:
  std::shared_ptr<IHal3A> m_pHal3A;

  /* lens configurations queue, from IPC */
 private:
  std::vector<IPC_LensConfig_T> m_lensCfgs;
  std::mutex m_lockLensCfg;
  std::condition_variable m_condLensCfg;

  /* sub thread to enqueu IPC_LensConfig_T */
 private:
  std::atomic<bool> m_bEnableQueuing;
  std::future<void> m_queuingThread;

  /* log level */
 private:
  static std::once_flag m_onceFlgLogLvl;
  static int m_logLevel;
};

};      // namespace v4l2
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2LENSMGR_H_
