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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_INFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_INFO_H_

#include "P2_Header.h"
#include "P2_Common.h"

namespace P2 {

class P2ConfigParam {
 public:
  P2ConfigParam();
  explicit P2ConfigParam(const P2Info& p2Info);

 public:
  P2Info mP2Info;
};

class P2InitParam {
 public:
  P2InitParam();
  explicit P2InitParam(const P2Info& p2Info);

 public:
  P2Info mP2Info;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_INFO_H_
