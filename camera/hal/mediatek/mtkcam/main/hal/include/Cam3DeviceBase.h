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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_INCLUDE_CAM3DEVICEBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_INCLUDE_CAM3DEVICEBASE_H_
//
#include <mutex>
#include <string>
//
#include <mtkcam/main/hal/Cam3Device.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class Cam3DeviceBase : public Cam3Device {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:                ////                    Data Members.
  MINT32 mLogLevel;        //  log level.
  std::mutex mDevOpsLock;  //  device operations mutex.

 protected:  ////                    Device Info.
  NSCam::ICamDeviceManager* mpDeviceManager;  //  device manager.
  std::string const mDevName;                 //  device name.
  int32_t const mi4OpenId;                    //  device open ID.
  camera3_callback_ops const* mpCallbackOps;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Instantiation.
  virtual ~Cam3DeviceBase();
  Cam3DeviceBase(std::string const& rDevName, int32_t const i4OpenId);

  virtual MINT32 getLogLevel() const;

 protected:  ////                    Operations.
  virtual MERROR onInitializeLocked() = 0;
  virtual MERROR onUninitializeLocked() = 0;
  virtual MERROR onConfigureStreamsLocked(
      camera3_stream_configuration_t* stream_list) = 0;
  virtual MERROR onFlushLocked() = 0;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Cam3Device Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Initialization.
  virtual MERROR i_closeDevice();

  virtual MERROR i_initialize(camera3_callback_ops const* callback_ops);

  virtual MERROR i_uninitialize();

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 public:  ////                    Stream management
  virtual MERROR i_configure_streams(
      camera3_stream_configuration_t* stream_list);

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 public:  ////                    Miscellaneous methods
  virtual MERROR i_flush();

  virtual void i_dump(int fd);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ICamDevice Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual char const* getDevName() const { return mDevName.c_str(); }
  virtual int32_t getOpenId() const { return mi4OpenId; }

  virtual void setDeviceManager(ICamDeviceManager* manager);
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_INCLUDE_CAM3DEVICEBASE_H_
