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

/**
 * @file Defs.h
 *
 * HDR Detection Defs
 *
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_HDRDETECTION_DEFS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_HDRDETECTION_DEFS_H_

#include <mtkcam/def/BuiltinTypes.h>

// ---------------------------------------------------------------------------

namespace NSCam {

// HDR mode
enum class HDRMode : uint8_t {
  // disable high dynamic range imaging techniques
  //
  // logically equivalent to
  // scene-mode ??SCENE_MODE_HDR
  OFF = 0,
  // capture a scene using high dynamic range imaging techniques
  //
  // logically equivalent to
  // scene-mode = SCENE_MODE_HDR
  ON,
  // capture a scene using high dynamic range imaging techniques
  // supports HDR scene detection
  //
  // logically equivalent to
  // scene-mode = SCENE_MODE_HDR
  // hdr-auto-mode = on
  AUTO,
  // capture/preview/record a scene using high dynamic range imaging techniques
  //
  // logically equivalent to
  // scene-mode = SCENE_MODE_HDR
  // hdr-auto-mode = off
  // video-hdr = on
  VIDEO_ON,
  // capture/preview/record a scene using high dynamic range imaging techniques
  // supports HDR scene detection
  //
  // logically equivalent to
  // scene-mode = SCENE_MODE_HDR
  // hdr-auto-mode = on
  // video-hdr = on
  VIDEO_AUTO,
  // Placeholder for range check; please extend results before this one
  NUM
};

// HDR detection result
enum class HDRDetectionResult {
  // HDR detection is turned off
  NONE = -1,
  // HDR should be turned off or the strength should be moderate
  NORMAL,
  // HDR should be turned on or the strength should be aggresive
  HDR,
  // Placeholder for range check; please extend results before this one
  NUM
};

}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_HDRDETECTION_DEFS_H_
