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

#define LOG_TAG "MtkCam/pipeline"
//
#include "MyUtils.h"
#include <memory>
#include <mutex>
#include "IPipelineFrameNumberGenerator.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
/**
 * An implementation of pipeline frameNo generator.
 */
class PipelineFrameNumberGenerator : public IPipelineFrameNumberGenerator {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  //                       Instantiation
  ~PipelineFrameNumberGenerator();
  PipelineFrameNumberGenerator();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineFrameNumberGenerator Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  //                       Operations
  virtual uint32_t generateFrameNo();
  virtual uint32_t getFrameNo();
  virtual void resetFrameNo();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  //                       Data Members
  mutable std::mutex mFrameNoLock;
  int32_t mFrameNo;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam

using NSCam::v3::IPipelineFrameNumberGenerator;
/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineFrameNumberGenerator>
IPipelineFrameNumberGenerator::create() {
  auto ptr = std::make_shared<PipelineFrameNumberGenerator>();
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::PipelineFrameNumberGenerator::PipelineFrameNumberGenerator()
    : mFrameNo(0) {}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::PipelineFrameNumberGenerator::~PipelineFrameNumberGenerator() {}

/******************************************************************************
 *
 ******************************************************************************/
uint32_t NSCam::v3::PipelineFrameNumberGenerator::generateFrameNo() {
  std::lock_guard<std::mutex> _l(mFrameNoLock);
  return mFrameNo++;
}

/******************************************************************************
 *
 ******************************************************************************/
uint32_t NSCam::v3::PipelineFrameNumberGenerator::getFrameNo() {
  std::lock_guard<std::mutex> _l(mFrameNoLock);
  MY_LOGD("frameNo:%d", mFrameNo);
  return mFrameNo;
}

/******************************************************************************
 *
 ******************************************************************************/
void NSCam::v3::PipelineFrameNumberGenerator::resetFrameNo() {
  std::lock_guard<std::mutex> _l(mFrameNoLock);
  mFrameNo = 0;
}
