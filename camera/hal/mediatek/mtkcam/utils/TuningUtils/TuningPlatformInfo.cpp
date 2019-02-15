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

#define LOG_TAG "mtkcam-tuning-util"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include "cros-camera/utils/camera_config.h"

#include <memory>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/TuningUtils/TuningPlatformInfo.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>

namespace NSCam {
namespace TuningUtils {

TuningPlatformInfo::TuningPlatformInfo() {}

int GetTuningIndex(int sensor_id) {
  TuningPlatformInfo tuning_info;
  PlatformInfo sensor_info;
  tuning_info.getTuningInfo(&sensor_info);
  if (sensor_id == 0) {
    return sensor_info.main_sensor.tuning_module_id;
  } else if (sensor_id == 1) {
    return sensor_info.sub_sensor.tuning_module_id;
  } else {
    CAM_LOGE("%s: sensor id out of range", __func__);
    return 0;
  }
}

void TuningPlatformInfo::getTuningInfo(PlatformInfo* info) {
  std::unique_ptr<cros::CameraConfig> camera_config;

  base::FilePath cros_config_path("/run/camera/camera_config_path");
  if (base::PathExists(cros_config_path))
    camera_config =
        cros::CameraConfig::Create("/run/camera/camera_config_path");
  else
    camera_config = cros::CameraConfig::Create("/etc/camera/camera_info.json");

  info->main_sensor.tuning_module_id =
      camera_config->GetInteger("main_sensor_tuning_module_id", 0);
  info->main_sensor.mirror = camera_config->GetInteger("main_sensor_mirror", 0);
  info->main_sensor.flip = camera_config->GetInteger("main_sensor_flip", 0);
  info->main_sensor.eeprom_path = camera_config->GetString(
      "main_sensor_eeprom", "/sys/bus/nvmem/devices/2-00500/nvmem");
  info->main_sensor.orientation =
      camera_config->GetInteger("main_sensor_orientation", 0);
  info->main_sensor.minFocusDistance =
      camera_config->GetInteger("main_sensor_minFocusDistance", 0);

  info->sub_sensor.tuning_module_id =
      camera_config->GetInteger("sub_sensor_tuning_module_id", 0);
  info->sub_sensor.mirror = camera_config->GetInteger("sub_sensor_mirror", 0);
  info->sub_sensor.flip = camera_config->GetInteger("sub_sensor_flip", 0);
  info->sub_sensor.eeprom_path = camera_config->GetString(
      "sub_sensor_eeprom", "/sys/bus/nvmem/devices/4-00500/nvmem");
  info->sub_sensor.orientation =
      camera_config->GetInteger("sub_sensor_orientation", 0);
  info->sub_sensor.minFocusDistance =
      camera_config->GetInteger("sub_sensor_minFocusDistance", 0);
}

}  // namespace TuningUtils
}  // namespace NSCam
