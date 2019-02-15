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

#define LOG_TAG "mtkcam-pipelinemodel"
#include "MyUtils.h"

//
#include <hal/mediatek/mtkcam/pipeline/model/PipelineModelImpl.h>
#include <memory>
#include <string>
#include <vector>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::pipeline::model::PipelineModelImpl;
using NSCamHW::HwInfoHelper;
/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
PipelineModelImpl::PipelineModelImpl(CreationParams const& creationParams)
    : mPipelineStaticInfo(std::make_shared<PipelineStaticInfo>())
      //
      ,
      mOpenId(creationParams.openId),
      mLogLevel(0),
      mHalDeviceAdapter(IHalDeviceAdapter::create(mOpenId)) {
  mLogLevel = ::property_get_int32("vendor.debug.camera.log", 0);
  if (mLogLevel == 0) {
    mLogLevel =
        ::property_get_int32("vendor.debug.camera.log.pipelinemodel", 0);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::createInstance(CreationParams const& creationParams)
    -> std::shared_ptr<PipelineModelImpl> {
  auto pPipeline = std::make_shared<PipelineModelImpl>(creationParams);

  if (CC_UNLIKELY(!pPipeline->init())) {
    CAM_LOGE("pipelinemodel instance init fail");
    return nullptr;
  }

  return pPipeline;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::init() -> bool {
  MY_LOGD1("+");

  bool ret = initPipelineStaticInfo();
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("Fail on initPipelineStaticInfo");
    return false;
  }

  MY_LOGD1("-");
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::initPipelineStaticInfo() -> bool {
  bool ret = false;
  MY_LOGD1("+");

  if (CC_UNLIKELY(mHalDeviceAdapter == nullptr)) {
    MY_LOGE("Fail on IHalDeviceAdapter::create()");
    return false;
  }

  ret =
      mHalDeviceAdapter->getPhysicalSensorId(&(mPipelineStaticInfo->sensorIds));
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("Fail on getPhysicalSensorId");
    return false;
  }

  mPipelineStaticInfo->openId = mOpenId;

  mPipelineStaticInfo->sensorRawTypes.resize(
      mPipelineStaticInfo->sensorIds.size());
  for (size_t i = 0; i < mPipelineStaticInfo->sensorIds.size(); i++) {
    HwInfoHelper helper(mPipelineStaticInfo->sensorIds[i]);
    if (CC_UNLIKELY(!helper.updateInfos())) {
      MY_LOGE("cannot properly update infos");
      return false;
    }

    if (CC_UNLIKELY(!helper.getSensorRawFmtType(
            &mPipelineStaticInfo->sensorRawTypes[i]))) {
      MY_LOGW("sensorId[%zu]:%d fail on getSensorRawFmtType", i,
              mPipelineStaticInfo->sensorIds[i]);
    }
  }

  NSCam::IHalSensorList* pSensorHalList = nullptr;
  NSCam::SensorStaticInfo sensorStaticInfo;

  pSensorHalList = GET_HalSensorList();
  if (pSensorHalList == nullptr) {
    MY_LOGE("pSensorHalList::get fail");
    return false;
  }

  MUINT32 sensorDev =
      pSensorHalList->querySensorDevIdx(mPipelineStaticInfo->sensorIds[0]);
  pSensorHalList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);

  MY_LOGD1("-");
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::dumpState(const std::vector<std::string>& options)
    -> void {
  //  Instantiation data
  {
    auto const& o = *mPipelineStaticInfo;
    std::string os;
    os += "{";
    os += ".openId=";
    os += std::to_string(o.openId);

    os += ", .sensorId={";
    for (auto const id : o.sensorIds) {
      os += std::to_string(id);
      os += " ";
    }
    os += "}";

    os += ", .sensorRawType={";
    for (auto const v : o.sensorRawTypes) {
      os += std::to_string(v);
      os += " ";
    }
    os += "}";

    if (o.isType3PDSensorWithoutPDE) {
      os += ", .isType3PDSensorWithoutPDE=true";
    }

    if (o.isVhdrSensor) {
      os += ", .isVhdrSensor=true";
    }
    os += "}";
    MY_LOGI("%s", os.c_str());
  }

  std::lock_guard<std::mutex> l(mLock);
  if (mSession != nullptr) {
    mSession->dumpState(options);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::open(
    std::string const& userName,
    std::weak_ptr<IPipelineModelCallback> const& callback) -> int {
  MY_LOGD1("+");
  //
  {
    std::lock_guard<std::mutex> _l(mLock);

    mUserName = userName;
    mCallback = callback;

    mvOpenFutures.push_back(std::async(std::launch::async, [this]() {
      return CC_LIKELY(mHalDeviceAdapter != nullptr) &&
             CC_LIKELY(mHalDeviceAdapter->open()) &&
             CC_LIKELY(mHalDeviceAdapter->powerOn());
    }));
  }
  //
  MY_LOGD1("-");
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::waitUntilOpenDone() -> bool {
  MY_LOGD1("+");
  std::lock_guard<std::mutex> _l(mLock);
  bool ret = waitUntilOpenDoneLocked();
  MY_LOGD1("- ret:%d", ret);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::waitUntilOpenDoneLocked() -> bool {
  MY_LOGD1("+");
  //
  for (auto& fut : mvOpenFutures) {
    bool result = fut.get();
    if (CC_UNLIKELY(!result)) {
      MY_LOGE("Fail to init");
      return false;
    }
  }
  //
  mvOpenFutures.clear();
  //
  MY_LOGD1("-");
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::close() -> void {
  MY_LOGD1("+");
  {
    std::lock_guard<std::mutex> _l(mLock);

    NSCam::Utils::CamProfile profile(__FUNCTION__, mUserName.c_str());

    waitUntilOpenDoneLocked();
    profile.print("waitUntilInitDone -");

    MY_LOGD1("destroying mSession");
    mSession = nullptr;  // created at configure; destroyed at close.

    if (CC_LIKELY(mHalDeviceAdapter != nullptr)) {
      mHalDeviceAdapter->powerOff();
      profile.print("Device powerOff -");
      mHalDeviceAdapter->close();
      profile.print("Device close -");
    }
    mUserName.clear();
  }
  MY_LOGD1("-");
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::configure(
    std::shared_ptr<UserConfigurationParams> const& params) -> int {
  int err = OK;
  MY_LOGD1("+");

  if (!mCallback.expired()) {
    std::lock_guard<std::mutex> _l(mLock);

    IPipelineModelSessionFactory::CreationParams sessionCfgParams;
    sessionCfgParams.pPipelineStaticInfo = mPipelineStaticInfo;
    sessionCfgParams.pUserConfigurationParams = params;
    sessionCfgParams.pPipelineModelCallback = mCallback.lock();
    //
    mSession = nullptr;
    mSession = IPipelineModelSessionFactory::createPipelineModelSession(
        sessionCfgParams);
    if (CC_UNLIKELY(mSession == nullptr)) {
      MY_LOGE("null session");
      err = NO_INIT;
    }
  }
  MY_LOGD1("- err:%d", err);
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::submitRequest(
    std::vector<std::shared_ptr<UserRequestParams>> const& requests,
    uint32_t* numRequestProcessed) -> int {
  int err = OK;
  std::shared_ptr<IPipelineModelSession> session;

  MY_LOGD2("+");
  {
    std::lock_guard<std::mutex> _l(mLock);
    session = mSession;
  }
  {
    if (CC_UNLIKELY(session == nullptr)) {
      MY_LOGE("null session");
      err = DEAD_OBJECT;
    } else {
      err = session->submitRequest(requests, numRequestProcessed);
    }
  }
  MY_LOGD2("- err:%d", err);
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::beginFlush() -> int {
  int err = OK;
  MY_LOGD1("+");

  {
    std::lock_guard<std::mutex> _l(mLock);
    if (mSession != nullptr) {
      err = mSession->beginFlush();
    }
  }

  MY_LOGD1("-");
  return err;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelImpl::endFlush() -> void {
  MY_LOGD1("+");

  {
    std::lock_guard<std::mutex> _l(mLock);
    if (mSession != nullptr) {
      mSession->endFlush();
    }
  }

  MY_LOGD1("-");
}
