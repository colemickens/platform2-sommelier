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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_DEVICE3_DEPEND_DEFAULT_CAM3DEVICEIMP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_DEVICE3_DEPEND_DEFAULT_CAM3DEVICEIMP_H_
//
#include "Cam3DeviceBase.h"
#include <mtkcam/app/IAppStreamManager.h>
#include <mtkcam/pipeline/model/IPipelineModel.h>
#include <mtkcam/pipeline/model/IPipelineModelManager.h>
#include <mtkcam/pipeline/model/types.h>
#include "IErrorCallback.h"

//
#include <mtkcam/drv/IHalSensor.h>
//
#if '1' == MTKCAM_HAVE_3A_HAL
#include <mtkcam/aaa/IHal3A.h>
#endif
//
#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace NSDefaultCam3Device {

/******************************************************************************
 *  Camera3 Device Implementation for Default.
 ******************************************************************************/
class Cam3DeviceImp : public pipeline::model::IPipelineModelCallback,
                      public Cam3DeviceBase,
                      public IErrorCallback {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Data Members.
  struct AppContext {
    std::shared_ptr<IAppStreamManager> pAppStreamManager;
    std::shared_ptr<pipeline::model::IPipelineModel> pPipelineModel;
  };
  AppContext mAppContext;
  mutable std::mutex mAppContextLock;

 protected:  ////                    Data Members.
  mutable std::mutex mRequestingLock;
  std::atomic<int> mRequestingAllowed;

 protected:  ////                    Data Members.
  NSCam::IHalSensor* mpHalSensor;
#if '1' == MTKCAM_HAVE_3A_HAL
  std::shared_ptr<NS3Av3::IHal3A> mpHal3a;
#endif
  std::vector<std::future<MERROR> > mvFutures;

 protected:  ////                    duration information for debug
  MUINT64 mProcessRequestStart;
  MUINT64 mProcessRequestEnd;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Instantiation.
  virtual ~Cam3DeviceImp();
  Cam3DeviceImp(std::string const& rDevName, int32_t const i4OpenId);

 protected:  ////                    Operations.
  virtual AppContext getSafeAppContext() const;
  virtual MERROR flushAndWait(AppContext const& appContext);
  virtual MVOID flushRequest(camera3_capture_request_t* request) const;

 protected:  ////                    Operations.
  MVOID enableRequesting();
  MVOID disableRequesting();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Cam3DeviceCommon Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  virtual MINT32 getLogLevel() const { return Cam3DeviceBase::getLogLevel(); }
  std::shared_ptr<IAppStreamManager> getAppStreamManager() const {
    return getSafeAppContext().pAppStreamManager;
  }
  std::shared_ptr<pipeline::model::IPipelineModel> getSafePipelineModel()
      const {
    return getSafeAppContext().pPipelineModel;
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  [Template method] Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Operations.
  virtual MERROR onInitializeLocked();
  virtual MERROR onUninitializeLocked();
  virtual MERROR onConfigureStreamsLocked(
      camera3_stream_configuration_t* stream_list);
  virtual MERROR onFlushLocked();
  virtual bool waitUntilOpenDoneLocked();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Cam3Device Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Request creation and submission
  virtual camera_metadata const* i_construct_default_request_settings(int type);

 public:  ////                    Request creation and submission
  virtual MERROR i_process_capture_request(camera3_capture_request_t* request);

  /* IErrorCallback interface */
 public:
  virtual status_t deviceError(void);
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineModelMgr::IAppCallback Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  virtual auto onFrameUpdated(pipeline::model::UserOnFrameUpdated const& params)
      -> void;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSDefaultCam3Device
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_DEVICE3_DEPEND_DEFAULT_CAM3DEVICEIMP_H_
