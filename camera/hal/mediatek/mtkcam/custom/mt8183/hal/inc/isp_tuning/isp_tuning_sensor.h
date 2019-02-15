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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_ISP_TUNING_ISP_TUNING_SENSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_ISP_TUNING_ISP_TUNING_SENSOR_H_

namespace NSIspTuning {

/*******************************************************************************
 *
 ******************************************************************************/
typedef enum {
  ESensorDev_None = 0x00,
  ESensorDev_Main = 0x01,
  ESensorDev_Sub = 0x02,
  ESensorDev_MainSecond = 0x04,
  ESensorDev_Main3D = 0x05,
  ESensorDev_SubSecond = 0x08
} ESensorDev_T;

};  // namespace NSIspTuning

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_ISP_TUNING_ISP_TUNING_SENSOR_H_
