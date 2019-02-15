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

#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#include "CommonRule.h"

namespace NSCam {
namespace TuningUtils {

void genFileName_LSC(char* pFilename,
                     int nFilename,
                     const FILE_DUMP_NAMING_HINT* pHint,
                     const char* pUserString) {
  if (pUserString == NULL) {
    pUserString = "";
  }
  int t;
  char* ptr = pFilename;
  const char* strIspProfileName = NULL;
  if (property_get_int32("vendor.debug.enable.normalAEB", 0)) {
    t = MakePrefix(ptr, nFilename, pHint->UniqueKey, pHint->FrameNo,
                   pHint->RequestNo, pHint->EvValue);
  } else {
    t = MakePrefix(ptr, nFilename, pHint->UniqueKey, pHint->FrameNo,
                   pHint->RequestNo);
  }
  ptr += t;
  nFilename -= t;

  t = snprintf(ptr, nFilename, "-%s", SENSOR_DEV_TO_STRING(pHint->SensorDev));
  ptr += t;
  nFilename -= t;

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

  strIspProfileName =
      getIspProfileName((NSIspTuning::EIspProfile_T)pHint->IspProfile);
  if (strIspProfileName) {
    t = snprintf(ptr, nFilename, "-%s", strIspProfileName);
  } else {
    t = snprintf(ptr, nFilename, "-profile");
  }
  ptr += t;
  nFilename -= t;

  t = snprintf(ptr, nFilename, ".lsc");
  ptr += t;
  nFilename -= t;
}

}  // namespace TuningUtils
}  // namespace NSCam
