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

#define LOG_TAG "EEPROM_UPDATE"

#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Misc.h>
#include <mtkcam/utils/TuningUtils/TuningPlatformInfo.h>
#include <property_lib.h>

using NSCam::TuningUtils::PlatformInfo;
using NSCam::TuningUtils::TuningPlatformInfo;

int main(int argc, char* const argv[]) {
  PlatformInfo pInfo;
  TuningPlatformInfo pPlatformInfo;

  pPlatformInfo.getTuningInfo(&pInfo);

  if (!NSCam::Utils::makePath(CAL_DUMP_PATH, 0755))
    CAM_LOGE("makePath[%s] fails", CAL_DUMP_PATH);

  char eeprom_buffer[4096];
  unsigned int file_size;
  file_size =
      NSCam::Utils::loadFileToBuf(pInfo.main_sensor.eeprom_path.c_str(),
                                  (unsigned char* const)eeprom_buffer, 4096);
  NSCam::Utils::saveBufToFile(CAL_DUMP_PATH "/main_sensor_eeprom",
                              (unsigned char* const)eeprom_buffer, file_size);

  file_size =
      NSCam::Utils::loadFileToBuf(pInfo.sub_sensor.eeprom_path.c_str(),
                                  (unsigned char* const)eeprom_buffer, 4096);
  NSCam::Utils::saveBufToFile(CAL_DUMP_PATH "/sub_sensor_eeprom",
                              (unsigned char* const)eeprom_buffer, file_size);

  return 0;
}
