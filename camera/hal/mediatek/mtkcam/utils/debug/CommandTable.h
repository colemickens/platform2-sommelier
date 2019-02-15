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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_DEBUG_COMMANDTABLE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_DEBUG_COMMANDTABLE_H_

#include <map>
#include <string>

static auto& getDebuggableMap() {
  static const std::map<std::string, std::string> sInst = {
      /**********************************************************************
       *  utils/
       **********************************************************************/
      {// debug
       "debug",
       "--module debug [--backtrace] \n"
       "       --backtrace: dump the currnet backtrace of this process.\n"},
      {// imgbuf: image buffer heap
       "NSCam::IImageBufferHeapManager",
       "--module NSCam::IImageBufferHeapManager [options] \n"
       "       Print debug information of image buffer heap.\n"},
      {// imgbuf: ion devices
       "NSCam::IIonDeviceProvider",
       "--module NSCam::IIonDeviceProvider [options] \n"
       "       Print debug information of ion devices.\n"},
      {// imgbuf: ion image buffer heap allocator
       "NSCam::IIonImageBufferHeapAllocator",
       "--module NSCam::IIonImageBufferHeapAllocator [options] \n"
       "       Print debug information of ion image buffer heap allocator.\n"},

      /**********************************************************************
       *  main/
       **********************************************************************/
      {// HAL3: camera device
       "android.hardware.camera.device@3.2::ICameraDevice",
       "--module android.hardware.camera.device@3.2::ICameraDevice [options] \n"
       "       Print debug information of camera device HAL3.\n"},
      {// HAL3: app stream manager
       "NSCam::v3::IAppStreamManager",
       "--module NSCam::v3::IAppStreamManager [options] \n"
       "       Print debug information of IAppStreamManager.\n"},

      /**********************************************************************
       *  middleware/
       **********************************************************************/

      /**********************************************************************
       *  pipeline
       **********************************************************************/
      {// IPipelineModelManager
       "NSCam::v3::pipeline::model::IPipelineModelManager",
       "--module NSCam::v3::pipeline::model::IPipelineModelManager [options] \n"
       "       Print debug information of IPipelineModelManager.\n"},

      /**********************************************************************
       *  aaa
       **********************************************************************/

      /**********************************************************************
       *  drv
       **********************************************************************/
  };
  //
  return sInst;
}
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_DEBUG_COMMANDTABLE_H_
