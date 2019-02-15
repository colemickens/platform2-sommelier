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

#define LOG_TAG "camera_custom_exif"
//
#include <custom/debug_exif/IDebugExif.h>
#include <dbg_cam_param.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Log.h>
//
/******************************************************************************
 *
 ******************************************************************************/
namespace {
class DebugExifImpl : public NSCam::Custom::IDebugExif {
 public:
  virtual debug_exif_buffer_info const* getBufInfo(uint32_t keyID) const {
    switch (keyID) {
      case DEBUG_EXIF_KEYID_CAM:
        return &NSCam::Custom::sDbgExifBufInfo_cam;

      default:
        ALOGE("Not supported keyID:%x", keyID);
        break;
    }
    return NULL;
  }

 public:  ////    MF
  virtual uint32_t getTagId_MF_TAG_VERSION() const {
    return dbg_cam_mf_param_9::MF_TAG_VERSION;
  }
  virtual uint32_t getTagId_MF_TAG_IMAGE_HDR() const {
    return dbg_cam_mf_param_9::MF_TAG_IMAGE_HDR;
  }
};  // class
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
VISIBILITY_PUBLIC extern "C" NSCam::Custom::IDebugExif*
getInstance_DebugExif() {
  static DebugExifImpl* inst = new DebugExifImpl();
  return inst;
}
