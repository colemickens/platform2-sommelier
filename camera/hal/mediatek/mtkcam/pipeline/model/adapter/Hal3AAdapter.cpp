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
#undef LOG_TAG
#define LOG_TAG "MtkCam/HwPipeline/Adapter"
//
#include "IHal3AAdapter.h"
#include "MyUtils.h"
//
#include <memory>
#include <mtkcam/aaa/IHal3A.h>
#include <string>
//
using NS3Av3::IHal3A;
using NSCam::v3::pipeline::model::IHal3AAdapter;
/******************************************************************************
 *
 ******************************************************************************/
class Hal3AAdapter : public IHal3AAdapter {
 protected:  ////            Data Members.
  int32_t mId;
  std::string const mName;

  std::shared_ptr<NS3Av3::IHal3A> mHal3a = nullptr;

 public:  ////    Operations.
  Hal3AAdapter(int32_t id, std::string const& name) : mId(id), mName(name) {
    MY_LOGD("%p", this);
  }

  virtual auto init() -> bool {
    CAM_TRACE_NAME("init(3A)");
    MAKE_Hal3A(
        mHal3a, [](IHal3A* p) { p->destroyInstance(LOG_TAG); }, mId, LOG_TAG);
    MY_LOGE_IF(!mHal3a, "Bad mHal3a");
    return (nullptr != mHal3a);
  }

  virtual ~Hal3AAdapter() { MY_LOGD("deconstruction"); }

  auto notifyPowerOn() -> bool override {
    if (mHal3a) {
      CAM_TRACE_NAME("3A notifyPowerOn");
      return mHal3a->notifyPwrOn();
    }
    return true;
  }

  auto notifyPowerOff() -> bool override {
    CAM_TRACE_NAME("3A notifyPowerOff");
    if (mHal3a) {
      bool ret = mHal3a->notifyPwrOff();
      if (!ret) {
        CAM_TRACE_NAME("3A notifyPowerOff fail");
      }
      return ret;
    }
    return true;
  }
};

/******************************************************************************
 *
 ******************************************************************************/
auto IHal3AAdapter::create(int32_t id, std::string const& name)
    -> std::shared_ptr<IHal3AAdapter> {
  auto p = std::make_shared<Hal3AAdapter>(id, name);

  if (!p->init()) {
    return nullptr;
  }
  return p;
}
