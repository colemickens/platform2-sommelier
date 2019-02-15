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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_3RDPARTY_MTK_SWNR_SWNRIMPL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_3RDPARTY_MTK_SWNR_SWNRIMPL_H_

#include <future>
#include <list>
#include <map>
#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
#include <mtkcam/3rdparty/plugin/PipelinePluginType.h>
#include <mtkcam/aaa/ICaptureNR.h>
#include <mtkcam/def/common.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <thread>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

/******************************************************************************
 *
 ******************************************************************************/
class SwnrPluginProviderImp : public YuvPlugin::IProvider {
  typedef YuvPlugin::Property Property;
  typedef YuvPlugin::Selection Selection;
  typedef YuvPlugin::Request::Ptr RequestPtr;
  typedef YuvPlugin::RequestCallback::Ptr RequestCallbackPtr;

 public:
  SwnrPluginProviderImp();

  virtual ~SwnrPluginProviderImp();

  const Property& property() override;

  MERROR negotiate(Selection* sel) override;

  void init() override;

  MERROR process(RequestPtr pRequest,
                 RequestCallbackPtr pCallback = nullptr) override;

  void abort(const std::vector<RequestPtr>& pRequests) override;

  void uninit() override;

  void set(MINT32 iOpenId) override;

 protected:
  bool onDequeRequest();

  bool onProcessFuture();

  int32_t doSwnr(RequestPtr const req);

  bool queryNrThreshold(int const scenario,
                        int* pHw_threshold,
                        int* pSwnr_threshold);

  void waitForIdle();
  void threadLoop();

 private:
  MINT32 mOpenId = 0;
  MINT32 mEnableLog = 1;
  MINT32 muDumpBuffer = 0;
  MINT32 mEnable = -1;
  //
  ISwNR* mpSwnr = nullptr;
  //
  mutable std::mutex mFutureLock;
  mutable std::condition_variable mFutureCond;
  std::map<RequestPtr, std::future<int32_t> > mvFutures;
  std::thread mThread;
  volatile MBOOL mbRequestExit = MFALSE;
};
/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSPipelinePlugin

};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_3RDPARTY_MTK_SWNR_SWNRIMPL_H_
