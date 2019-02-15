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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METASTORE_IMETADATAPROVIDER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METASTORE_IMETADATAPROVIDER_H_
//
#include <memory>
#include <mtkcam/utils/metadata/IMetadata.h>

struct camera_metadata;

namespace NSCam {
/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *  Metadata Provider Interface
 ******************************************************************************/
class VISIBILITY_PUBLIC IMetadataProvider {
 public:  ////                    Creation.
  static std::shared_ptr<IMetadataProvider> create(int32_t const i4OpenId);

  static std::shared_ptr<IMetadataProvider> create(int32_t const i4OpenId,
                                                   const IMetadata& aMeta_mtk,
                                                   camera_metadata* aMeta);

 public:  ////                    Instantiation.
  virtual ~IMetadataProvider() {}

 public:  ////                    Interface.
  virtual camera_metadata const* getStaticCharacteristics() const = 0;

  virtual IMetadata const& getMtkStaticCharacteristics() const = 0;

  virtual uint32_t getDeviceVersion() const = 0;

  virtual int getDeviceFacing() const = 0;

  virtual int getDeviceWantedOrientation() const = 0;

  virtual int getDeviceSetupOrientation() const = 0;

  virtual int getDeviceHasFlashLight() const = 0;

 public:  //// hal 1 support
  virtual int setStaticData(IMetadata* /*pMeta*/) { return -1; }

  virtual int restoreStaticData() { return -1; }
};

/******************************************************************************
 *  MetadataProvider Manager
 ******************************************************************************/
namespace NSMetadataProviderManager {
void VISIBILITY_PUBLIC clear();
ssize_t VISIBILITY_PUBLIC add(int32_t deviceId,
                              std::shared_ptr<IMetadataProvider> pProvider);
ssize_t VISIBILITY_PUBLIC remove(int32_t deviceId);

std::shared_ptr<IMetadataProvider> VISIBILITY_PUBLIC valueFor(int32_t deviceId);
std::shared_ptr<IMetadataProvider> VISIBILITY_PUBLIC valueAt(size_t index);
int32_t VISIBILITY_PUBLIC keyAt(size_t index);
ssize_t VISIBILITY_PUBLIC indexOfKey(int32_t deviceId);

};  // namespace NSMetadataProviderManager

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METASTORE_IMETADATAPROVIDER_H_
