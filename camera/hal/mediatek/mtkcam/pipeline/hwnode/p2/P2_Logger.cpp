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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_Logger.h"
#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG P2_Logger
#define P2_TRACE TRACE_P2_LOGGER
#include "P2_LogHeader.h"
#include <property_lib.h>
namespace P2 {

bool isTrace1On() {
  static int spVal = property_get_int32("persist.vendor." KEY_TRACE_P2, 0);
  static int sdVal = property_get_int32("vendor.debug." KEY_TRACE_P2, 0);
  static int sVal = sdVal ? sdVal : spVal;
  static bool sTrace = (sVal == 1);
  return sTrace;
}

bool isTrace2On() {
  static int spVal = property_get_int32("persist.vendor." KEY_TRACE_P2, 0);
  static int sdVal = property_get_int32("vendor.debug." KEY_TRACE_P2, 0);
  static int sVal = sdVal ? sdVal : spVal;
  static bool sTrace = (sVal >= 1 && sVal <= 2);
  return sTrace;
}

}  // namespace P2
