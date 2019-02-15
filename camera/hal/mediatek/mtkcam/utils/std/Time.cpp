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

#define LOG_TAG "MtkCam/Utils"
//
#include <chrono>
#include "MyUtils.h"
//
/******************************************************************************
 *
 *****************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
uint32_t TimeTool::getReadableTime() {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  auto hh = std::chrono::duration_cast<std::chrono::hours>(
      now % std::chrono::hours(24));
  auto mm = std::chrono::duration_cast<std::chrono::minutes>(
      now % std::chrono::hours(1));
  auto ss = std::chrono::duration_cast<std::chrono::seconds>(
      now % std::chrono::minutes(1));
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now % std::chrono::seconds(1));
  return hh.count() * 10000000 + mm.count() * 100000 + ss.count() * 1000 +
         ms.count();
}

};  // namespace Utils
};  // namespace NSCam
