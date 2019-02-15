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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_DIP_NOTIFY_DATATYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_DIP_NOTIFY_DATATYPE_H_

/******************************************************************************
 *
 * @struct EDipModule
 * @brief module enum for Dip
 * @details
 *
 ******************************************************************************/
enum EDipModule {
  EDipModule_NONE = 0x0000,
  EDipModule_SRZ1 = 0x0001,
  EDipModule_SRZ2 = 0x0002,
  EDipModule_SRZ3 = 0x0004,
  EDipModule_SRZ4 = 0x0008,
  EDipModule_SRZ5 = 0x0010,
};

/******************************************************************************
 * Pipe ModuleParam Info (Descriptor).
 ******************************************************************************/
struct DipModuleCfg {
  EDipModule eDipModule;
  MVOID* moduleStruct;

 public:  //// constructors.
  DipModuleCfg() : eDipModule(EDipModule_NONE), moduleStruct(NULL) {}
  //
};

/******************************************************************************
 *
 * @struct SrzSize
 * @brief srz in/out size setting
 * @details
 *
 ******************************************************************************/
struct _SRZ_SIZE_INFO_ {
 public:
  unsigned int in_w;
  unsigned int in_h;
  unsigned int out_w;
  unsigned int out_h;
  unsigned int crop_x;
  unsigned int crop_y;
  uint64_t crop_floatX; /* x float precise */
  uint64_t crop_floatY; /* y float precise */
  uint32_t crop_w;
  uint32_t crop_h;

  _SRZ_SIZE_INFO_()
      : in_w(0x0),
        in_h(0x0),
        out_w(0x0),
        out_h(0x0),
        crop_x(0x0),
        crop_y(0x0),
        crop_floatX(0x0),
        crop_floatY(0x0),
        crop_w(0x0),
        crop_h(0x0) {}
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_DIP_NOTIFY_DATATYPE_H_
