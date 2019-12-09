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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_TUNINGUTILS_TUNINGPLATFORMINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_TUNINGUTILS_TUNINGPLATFORMINFO_H_

#include <string>

namespace NSCam {
namespace TuningUtils {

struct CameraInfo {
  std::string sensor_id;
  std::string eeprom_path;
  int tuning_module_id;
  int mirror;
  int flip;
  int orientation;
  float minFocusDistance;
};

struct PlatformInfo {
  CameraInfo wf_sensor;  // world facing sensor
  CameraInfo uf_sensor;  // user facing sensor
};

class VISIBILITY_PUBLIC TuningPlatformInfo {
 public:
  TuningPlatformInfo();
  void getTuningInfo(PlatformInfo* info);
};

int VISIBILITY_PUBLIC GetTuningIndex(int sensor_id);

}  // namespace TuningUtils
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_TUNINGUTILS_TUNINGPLATFORMINFO_H_
