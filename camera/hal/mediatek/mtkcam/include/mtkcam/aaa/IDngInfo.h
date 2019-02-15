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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IDNGINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IDNGINFO_H_

#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/module/module.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NS3Av3 {
class IDngInfo {
 protected:
  virtual ~IDngInfo() {}

 public:
  static IDngInfo* getInstance(char const* szCallerName,
                               MUINT const sensorIndex);

  virtual NSCam::IMetadata getStaticMetadata() const = 0;
  virtual NSCam::IMetadata getDynamicNoiseProfile(MUINT32 iso) const = 0;

  // utility namespace function
  virtual NSCam::IMetadata getShadingMapFromMem(
      MBOOL fgShadOn, const NSCam::IMetadata::Memory& lscData) const = 0;
  virtual NSCam::IMetadata getShadingMapFromHal(
      const NSCam::IMetadata& halMeta,
      const NSCam::IMetadata& appMeta) const = 0;
  virtual MUINT32 getRawBitDepth() const = 0;
};
};  // namespace NS3Av3

/**
 * @brief The definition of the maker of DngInfo instance.
 */
typedef NS3Av3::IDngInfo* (*DngInfo_FACTORY_T)(char const* szCallerName,
                                               MUINT const sensorIndex);
#define MAKE_DngInfo(...)                                              \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_DNG_INFO, DngInfo_FACTORY_T, \
                     __VA_ARGS__)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_IDNGINFO_H_
