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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_IPIPELINEFRAMENUMBERGENERATOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_IPIPELINEFRAMENUMBERGENERATOR_H_
//
#include <memory>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * An interface of pipeline frameNo generator.
 */
class IPipelineFrameNumberGenerator {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  //                       Operations
  static std::shared_ptr<IPipelineFrameNumberGenerator> create();
  /**
   * generate pipeline frameNo.
   *
   * @return
   *      return pipeline frame number.
   */
  virtual uint32_t generateFrameNo() = 0;

  /**
   * get current pipeline frameNo.
   *
   * @return
   *      return current pipeline frame number.
   */
  virtual uint32_t getFrameNo() = 0;

  /**
   * reset pipeline frameNo.
   *
   */
  virtual void resetFrameNo() = 0;
  virtual ~IPipelineFrameNumberGenerator() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_IPIPELINEFRAMENUMBERGENERATOR_H_
