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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IRESOURCECONCURRENCY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IRESOURCECONCURRENCY_H_
//
#include <memory>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interface of Resource Concurrency
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class IResourceConcurrency {
 public:
  enum CLIENT_HANDLER {
    CLIENT_HANDLER_0 = 0,
    CLIENT_HANDLER_1,
    CLIENT_HANDLER_MAX,                       // the amount of CLIENT_HANDLER
    CLIENT_HANDLER_NULL = CLIENT_HANDLER_MAX  // undefined CLIENT_HANDLER
  };

  /**
   * This utility is for the resource to control the concurrency.
   */

 public:
  /**
   * The static function to create the instance of ResourceConcurrency.
   * And this function will return a SP of IResourceConcurrency.
   */
  static std::shared_ptr<IResourceConcurrency> createInstance(
      const char* name, MINT64 timeout_ms);

 public:
  /**
   * Request the Client-Handler of this resource.
   * If all Client-Handler are requested, no more available Client-Handler,
   * it will return CLIENT_HANDLER_NULL.
   */
  virtual CLIENT_HANDLER requestClient() = 0;

  /**
   * Return the Client-Handler of this resource.
   * After this Client-Handler returned, this ID cannot be used anymore.
   */
  virtual MERROR returnClient(CLIENT_HANDLER id) = 0;

  /**
   * Acquire the resource by Client-Handler.
   */
  virtual MERROR acquireResource(CLIENT_HANDLER id) = 0;

  /**
   * Release the resource by Client-Handler.
   */
  virtual MERROR releaseResource(CLIENT_HANDLER id) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_IRESOURCECONCURRENCY_H_
