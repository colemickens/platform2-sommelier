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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_DISPATCHPROCESSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_DISPATCHPROCESSOR_H_

#include "P2_Processor.h"
#include "P2_BasicProcessor.h"
#include "P2_StreamingProcessor.h"

#include "P2_DumpPlugin.h"
#include "P2_ScanlinePlugin.h"
#include "P2_DrawIDPlugin.h"

#include <memory>

namespace P2 {

class DispatchProcessor
    : virtual public Processor<P2InitParam,
                               P2ConfigParam,
                               std::shared_ptr<P2FrameRequest>> {
 public:
  DispatchProcessor();
  virtual ~DispatchProcessor();

 protected:
  virtual MBOOL onInit(const P2InitParam& param);
  virtual MVOID onUninit();
  virtual MVOID onThreadStart();
  virtual MVOID onThreadStop();
  virtual MBOOL onConfig(const P2ConfigParam& param);
  virtual MBOOL onEnque(const std::shared_ptr<P2FrameRequest>& request);
  virtual MVOID onNotifyFlush();
  virtual MVOID onWaitFlush();

 private:
  MBOOL needBasicProcess(const std::shared_ptr<P2Request>& request);

 private:
  ILog mLog;
  P2Info mP2Info;
  std::shared_ptr<P2DumpPlugin> mDumpPlugin;
  std::shared_ptr<P2ScanlinePlugin> mScanlinePlugin;
  std::shared_ptr<P2DrawIDPlugin> mDrawIDPlugin;

  std::shared_ptr<BasicProcessor> mBasicProcessor;
  std::shared_ptr<StreamingProcessor> mStreamingProcessor;

  MUINT32 mForceProcessor = VAL_P2_PROC_DFT_POLICY;
  MBOOL mEnableBasic = MFALSE;
  MBOOL mEnableStreaming = MFALSE;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_DISPATCHPROCESSOR_H_
