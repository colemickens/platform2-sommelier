/*
 * Copyright (C) 2019 Mediatek Corporation.
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

#define LOG_TAG "IPCHalSensor"

#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/v4l2/IPCIHalSensor.h>

// MTK
#include <mtkcam/utils/std/compiler.h>  // CC_LIKELY, CC_UNLIKELY
#include <mtkcam/utils/std/Log.h>       // CAM_LOGD, CAM_LOGI ... etc

// from kernel space
#include <kd_imgsensor_define.h>  // SET_PD_BLOCK_INFO_T

// STL
#include <atomic>
#include <memory>

using NS3Av3::IHal3A;
using NSCam::IHalSensor;
using NSCam::IIPCHalSensor;
using NSCam::IIPCHalSensorListProv;
using NSCam::IMetadata;
using NSCam::SensorCropWinInfo;
using NSCam::SensorDynamicInfo;
using NSCam::SensorStaticInfo;
using NSCam::SensorVCInfo;

template <class T>
class ScenarioInfo {
 public:
  inline T& get(int scenario) {
    switch (scenario) {
      case NSCam::SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        return m_preview;
      case NSCam::SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        return m_capture;
      case NSCam::SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        return m_video;
      default:
        CAM_LOGE("get unsupported scenario info, gives a default one");
        return m_default;
    }
    return m_default;
  }

 private:
  T m_default;
  T m_preview;
  T m_capture;
  T m_video;
};

static std::shared_ptr<IHal3A> createHal3AInstance(uint32_t index) {
  std::shared_ptr<IHal3A> hal3a;
  MAKE_Hal3A(
      hal3a, [](IHal3A* p) { p->destroyInstance(LOG_TAG); }, index, LOG_TAG);

  return hal3a;
}

// Implementation of IIPCHalSensor.
class IPCHalSensorImp : public IIPCHalSensor {
 public:
  explicit IPCHalSensorImp(int index);
  virtual ~IPCHalSensorImp();

  // Implementation of IIPCHalSensor
 public:
  void ipcSetDynamicInfo(const SensorDynamicInfo& info) override;
  void ipcSetDynamicInfoEx(const DynamicInfo& info) override;
  IIPCHalSensor::DynamicInfo getDynamicInfoEx() const override;

  void updateCommand(MUINT indexDual,
                     MUINTPTR cmd,
                     MUINTPTR arg1,
                     MUINTPTR arg2,
                     MUINTPTR arg3) override;

  // Implementation of IHalSensor
 public:
  MVOID destroyInstance(const char* szCallerName /* = "" */) override;

  MBOOL powerOn(char const* szCallerName,
                MUINT const uCountOfIndex,
                MUINT const* pArrayOfIndex) override;

  MBOOL powerOff(char const* szCallerName,
                 MUINT const uCountOfIndex,
                 MUINT const* pArrayOfIndex) override;

  MBOOL configure(MUINT const uCountOfParam,
                  ConfigParam const* pConfigParam) override;

  MINT sendCommand(MUINT indexDual,
                   MUINTPTR cmd,
                   MUINTPTR arg1,
                   MUINT arg1_size,
                   MUINTPTR arg2,
                   MUINT arg2_size,
                   MUINTPTR arg3,
                   MUINT arg3_size) override;

  MBOOL querySensorDynamicInfo(MUINT32 indexDual,
                               SensorDynamicInfo* pSensorDynamicInfo) override;

  MINT32 setDebugInfo(IBaseCamExif* pIBaseCamExif) override;
  MINT32 reset() override;

  // attributes
 private:
  mutable std::mutex m_opLock;

  SensorDynamicInfo m_sensorDynamicInfo;
  uint32_t m_sensorIdx;
  uint32_t m_powerOnState;

  // information that need to be stored
  // SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO's data
  ScenarioInfo<SensorCropWinInfo> m_sensorCropWinInfo;

  // SENSOR_CMD_GET_PIXEL_CLOCK_FREQ
  int32_t m_pixelClokcFreq;

  // SENSOR_CMD_GET_FRAME_SYNC_PIXEL_LINE_NUM
  uint32_t m_frameSyncPixelLineNum;

  // SENSOR_CMD_GET_SENSOR_PDAF_INFO
  ScenarioInfo<SET_PD_BLOCK_INFO_T> m_sensorPdafInfo;

  // SENSOR_CMD_GET_SENSOR_PDAF_CAPACITY
  ScenarioInfo<MBOOL> m_sensorPdafCapacity;

  // SENSOR_CMD_GET_SENSOR_VC_INFO
  ScenarioInfo<SensorVCInfo> m_sensorVCInfo;

  // SENSOR_CMD_GET_DEFAULT_FRAME_RATE_BY_SCENARIO
  ScenarioInfo<MUINT32> m_defaultFrameRate;

  // SENSOR_CMD_GET_SENSOR_ROLLING_SHUTTER
  // bits[0:31]: tline, bits[32:63]: vsize
  uint64_t m_sensorRollingShutter;

  // SENSOR_CMD_GET_VERTICAL_BLANKING
  MINT32 m_verticalBlanking;

  // extended dynamic information
  DynamicInfo m_extenedDynamicInfo;
};

IIPCHalSensor* IIPCHalSensor::createInstance(int index) {
  return new IPCHalSensorImp(index);
}

/******************************************************************************
   I M P L E M E N T A T I O N    O F    I P C H A L S E N S O R I M P
 ******************************************************************************/
IPCHalSensorImp::IPCHalSensorImp(int index)
    : m_sensorDynamicInfo{0},
      m_sensorIdx(index),
      m_powerOnState(0),
      m_pixelClokcFreq(0),
      m_frameSyncPixelLineNum(0),
      m_sensorRollingShutter(0),
      m_verticalBlanking(0) {}

IPCHalSensorImp::~IPCHalSensorImp() {}

void IPCHalSensorImp::ipcSetDynamicInfo(const SensorDynamicInfo& info) {
  std::lock_guard<std::mutex> lk(m_opLock);

  auto p3A = createHal3AInstance(m_sensorIdx);
  if (p3A.get() == nullptr) {
    CAM_LOGE("ipcSetDynamicInfo failed since no IHal3A instance");
    return;
  }

  p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_SetDynamicInfo,
                  reinterpret_cast<MINTPTR>(&info), 0);

  m_sensorDynamicInfo = info;

  std::atomic_thread_fence(std::memory_order_release);
}

void IPCHalSensorImp::ipcSetDynamicInfoEx(
    const IIPCHalSensor::DynamicInfo& info) {
  std::lock_guard<std::mutex> lk(m_opLock);

  auto p3A = createHal3AInstance(m_sensorIdx);
  if (p3A.get() == nullptr) {
    CAM_LOGE("ipcSetDynamicInfoEx failed since no IHal3A instance");
    return;
  }

  p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_SetDynamicInfoEx,
                  reinterpret_cast<MINTPTR>(&info), 0);

  m_extenedDynamicInfo = info;
  std::atomic_thread_fence(std::memory_order_release);
}

IIPCHalSensor::DynamicInfo IPCHalSensorImp::getDynamicInfoEx() const {
  std::lock_guard<std::mutex> lk(m_opLock);
  std::atomic_thread_fence(std::memory_order_acquire);
  return m_extenedDynamicInfo;
}

void IPCHalSensorImp::updateCommand(MUINT indexDual,
                                    MUINTPTR cmd,
                                    MUINTPTR arg1,
                                    MUINTPTR arg2,
                                    MUINTPTR arg3) {
  std::lock_guard<std::mutex> lk(m_opLock);

  auto p3A = createHal3AInstance(m_sensorIdx);
  if (p3A.get() == nullptr) {
    CAM_LOGE("updateCommand failed since no IHal3A instance");
    return;
  }

  switch (cmd) {
    case NSCam::SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO:
      if (arg1 == 0 || arg2 == 0) {
        CAM_LOGE(
            "update cmd SENSOR_CMD_GET_SENSOR_CROP_WIN_INFO failed "
            "since no arg1 or arg2");
      } else {
        // arg1 [in] : address of MUINT32, represents the scenario.
        // arg2 [out]: address of SensorCropWinInfo.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_CropWin, static_cast<MINTPTR>(arg1),
                        static_cast<MINTPTR>(arg2));

        const MUINT32 scenario = *reinterpret_cast<MUINT32*>(arg1);
        m_sensorCropWinInfo.get(scenario) =
            *reinterpret_cast<SensorCropWinInfo*>(arg2);
      }

      break;

    case NSCam::SENSOR_CMD_GET_PIXEL_CLOCK_FREQ:
      if (arg1 == 0) {
        CAM_LOGE(
            "update cmd SENSOR_CMD_GET_PIXEL_CLOCK_FREQ failed since "
            "no arg1");
      } else {
        // arg1 [out]: address of MINT32
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_PixelClock, (MINTPTR)arg1, 0);

        m_pixelClokcFreq = *reinterpret_cast<MINT32*>(arg1);
      }

      break;

    case NSCam::SENSOR_CMD_GET_FRAME_SYNC_PIXEL_LINE_NUM:
      if (arg1 == 0) {
        CAM_LOGE(
            "update cmd SENSOR_CMD_GET_FRAME_SYNC_PIXEL_LINE_NUM failed "
            "since no arg1");
      } else {
        // arg1 [out]: address of MUINT32
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_PixelLine, (MINTPTR)arg1, 0);

        m_frameSyncPixelLineNum = *reinterpret_cast<MUINT32*>(arg1);
      }
      break;

    case NSCam::SENSOR_CMD_GET_SENSOR_PDAF_INFO:
      if (arg1 == 0 || arg2 == 0) {
        CAM_LOGE(
            "update cmd SENSOR_CMD_GET_SENSOR_PDAF_INFO failed "
            "since no arg1 or arg2");
      } else {
        // arg1 [in] : address of MINT32, represents sensor scenario.
        // arg2 [out]: address of SET_PD_BLOCK_INFO_T.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_PdafInfo, (MINTPTR)arg1,
                        (MINTPTR)arg2);

        const MINT32 scenario = *reinterpret_cast<MINT32*>(arg1);
        m_sensorPdafInfo.get(scenario) =
            *reinterpret_cast<SET_PD_BLOCK_INFO_T*>(arg2);
      }
      break;

    case NSCam::SENSOR_CMD_GET_SENSOR_PDAF_CAPACITY:
      if (arg1 == 0 || arg2 == 0) {
        CAM_LOGE(
            "update cmd SENSOR_CMD_GET_SENSOR_PDAF_CAPACITY failed "
            "since no arg1 or arg2");
      } else {
        // arg1 [in] : address of MINT32, represents sensor scenario.
        // arg2 [out]: address of MBOOL.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_PdafCapacity, (MINTPTR)arg1,
                        (MINTPTR)arg2);

        const MINT32 scenario = *reinterpret_cast<MINT32*>(arg1);
        m_sensorPdafCapacity.get(scenario) = *reinterpret_cast<MBOOL*>(arg2);
      }
      break;

    case NSCam::SENSOR_CMD_GET_SENSOR_VC_INFO:
      if (arg1 == 0 || arg2 == 0) {
        CAM_LOGE(
            "update cmd SENSOR_CMD_GET_SENSOR_VC_INFO failed since "
            "no arg1 or arg2");
      } else {
        // arg1 [out]: address of SensorVCInfo.
        // arg2 [in] : address of MINT32, represents sensor scenario.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_SensorVCInfo, (MINTPTR)arg1,
                        (MINTPTR)arg2);

        const MINT32 c = *reinterpret_cast<MINT32*>(arg2);
        m_sensorVCInfo.get(c) = *reinterpret_cast<SensorVCInfo*>(arg1);
      }
      break;

    case NSCam::SENSOR_CMD_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
      if (arg1 == 0 || arg2 == 0) {
        CAM_LOGE(
            "update default frame rate by scenario failed since no arg1 "
            "or no arg2");
      } else {
        // arg1 [in] : address of MUINT32, represents sensor scenario.
        // arg2 [out]: address of MUINT32, represents the frame rate.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_DefFrameRate, (MINTPTR)arg1,
                        (MINTPTR)arg2);

        const MUINT32 c = *reinterpret_cast<MUINT32*>(arg1);
        m_defaultFrameRate.get(c) = *reinterpret_cast<MUINT32*>(arg2);
      }
      break;

    case NSCam::SENSOR_CMD_GET_SENSOR_ROLLING_SHUTTER:
      if (arg1 == 0 || arg2 == 0) {
        CAM_LOGE("update rolling shutter failed since no arg1 or arg2");
      } else {
        // arg1 [out]: address of MUINT32, represents tline.
        // arg2 [out]: address of MUINT32, represents vsize.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_RollingShutter, (MINTPTR)arg1,
                        (MINTPTR)arg2);

        const uint32_t tline = *reinterpret_cast<MUINT32*>(arg1);
        const uint32_t vsize = *reinterpret_cast<MUINT32*>(arg2);
        m_sensorRollingShutter = ((uint64_t)vsize << 32 | tline);
      }
      break;

    case NSCam::SENSOR_CMD_GET_VERTICAL_BLANKING:
      if (arg1 == 0) {
        CAM_LOGE("update vertical blanking failed since no arg1");
      } else {
        // arg1 [out]: address of MINT32, represents vertical blanking.
        p3A->send3ACtrl(NS3Av3::E3ACtrl_IPC_VerticalBlanking,
                        static_cast<MINTPTR>(arg1), 0);

        m_verticalBlanking = *reinterpret_cast<MINT32*>(arg1);
      }
      break;

    default:
      CAM_LOGE("unsupported cmd(%#x)", cmd);
      return;
  }

  std::atomic_thread_fence(std::memory_order_release);  // StoreStore
}
/******************************************************************************
   I M P L E M E N T A T I O N    O F    I P C H A L S E N S O R
 ******************************************************************************/
MVOID
IPCHalSensorImp::destroyInstance(char const* szCallerName) {}

MBOOL
IPCHalSensorImp::powerOn(char const* szCallerName,
                         MUINT const uCountOfIndex,
                         MUINT const* /* pArrayOfIndex */
) {
  std::lock_guard<std::mutex> lk(m_opLock);

  m_powerOnState = static_cast<uint32_t>(uCountOfIndex);

  std::atomic_thread_fence(std::memory_order_release);
  return MTRUE;
}

MBOOL
IPCHalSensorImp::powerOff(char const* szCallerName,
                          MUINT const uCountOfIndex,
                          MUINT const* /* pArrayOfIndex */
) {
  std::lock_guard<std::mutex> lk(m_opLock);

  m_powerOnState = 0;

  std::atomic_thread_fence(std::memory_order_release);
  return MTRUE;
}

MBOOL IPCHalSensorImp::querySensorDynamicInfo(
    MUINT32 indexDual, SensorDynamicInfo* pSensorDynamicInfo) {
  if (CC_UNLIKELY(pSensorDynamicInfo == nullptr)) {
    CAM_LOGE("pSensorDynamicInfo is null");
    return MFALSE;
  }

  std::lock_guard<std::mutex> lk(m_opLock);
  std::atomic_thread_fence(std::memory_order_acquire);

  *pSensorDynamicInfo = m_sensorDynamicInfo;

  return MTRUE;
}

MBOOL IPCHalSensorImp::configure(MUINT const uCountOfParam,
                                 ConfigParam const* pConfigParam) {
  return MFALSE;
}

MINT IPCHalSensorImp::sendCommand(MUINT indexDual,
                                  MUINTPTR cmd,
                                  MUINTPTR arg1,
                                  MUINT arg1_size,
                                  MUINTPTR arg2,
                                  MUINT arg2_size,
                                  MUINTPTR arg3,
                                  MUINT arg3_size) {
  return MFALSE;
}

MINT32 IPCHalSensorImp::setDebugInfo(IBaseCamExif* pIBaseCamExif) {
  (void)pIBaseCamExif;
  return 0;
}

MINT32 IPCHalSensorImp::reset() {
  // no need
  return 0;
}
