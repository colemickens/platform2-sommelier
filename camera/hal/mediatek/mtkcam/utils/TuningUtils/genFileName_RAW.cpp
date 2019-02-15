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

#include "CommonRule.h"
#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>

namespace NSCam {
namespace TuningUtils {

// P2-like node dump
void genFileName_RAW(char* pFilename,
                     int nFilename,
                     const FILE_DUMP_NAMING_HINT* pHint,
                     RAW_PORT type,
                     const char* pUserString) {
  if (pUserString == NULL) {
    pUserString = "";
  }
  int t;
  char* ptr = pFilename;
  if (property_get_int32("vendor.debug.enable.normalAEB", 0)) {
    t = MakePrefix(ptr, nFilename, pHint->UniqueKey, pHint->FrameNo,
                   pHint->RequestNo, pHint->EvValue);
  } else {
    t = MakePrefix(ptr, nFilename, pHint->UniqueKey, pHint->FrameNo,
                   pHint->RequestNo);
  }
  ptr += t;
  nFilename -= t;

  if (pHint->SensorDev >= 0) {
    t = snprintf(ptr, nFilename, "-%s", SENSOR_DEV_TO_STRING(pHint->SensorDev));
    ptr += t;
    nFilename -= t;
  }

  if (type != RAW_PORT_NULL) {
    if (type == RAW_PORT_IMGO) {
      t = snprintf(ptr, nFilename, "-imgo");
    } else if (type == RAW_PORT_RRZO) {
      t = snprintf(ptr, nFilename, "-rrzo");
    } else {
      t = snprintf(ptr, nFilename, "-undef");
    }
    ptr += t;
    nFilename -= t;
  }

  if (*(pHint->additStr) != '\0') {
    t = snprintf(ptr, nFilename, "-%s", pHint->additStr);
    ptr += t;
    nFilename -= t;
  }

  if (*pUserString != '\0') {
    t = snprintf(ptr, nFilename, "-%s", pUserString);
    ptr += t;
    nFilename -= t;
  }

  t = snprintf(ptr, nFilename, "__%dx%d_%s_%s", pHint->ImgWidth,
               pHint->ImgHeight, IMAGE_FORMAT_TO_BITS(pHint->ImgFormat),
               SENSOR_FORMAT_TO_STRING(pHint->SensorFormatOrder));
  ptr += t;
  nFilename -= t;

  t = snprintf(ptr, nFilename, ".%s",
               IMAGE_FORMAT_TO_FILE_EXT(pHint->ImgFormat));
  ptr += t;
  nFilename -= t;
}

}  // namespace TuningUtils
}  // namespace NSCam
