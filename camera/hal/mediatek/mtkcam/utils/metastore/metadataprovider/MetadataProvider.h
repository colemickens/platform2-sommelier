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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_METADATAPROVIDER_METADATAPROVIDER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_METADATAPROVIDER_METADATAPROVIDER_H_
//
#include "custom/Info.h"
#include <memory>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/metadata/IMetadataConverter.h>
#include <system/camera_metadata.h>
#include <string>

using NSCam::IMetadata;
using NSCam::IMetadataConverter;
using NSCam::IMetadataProvider;
using NSCam::status_t;

namespace NSMetadataProvider {

/******************************************************************************
 *  Metadata Provider
 ******************************************************************************/
class MetadataProvider : public IMetadataProvider {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Data Members.
  Info mInfo;
  camera_metadata* mpStaticCharacteristics;
  IMetadata mpHALMetadata;
  //
  // support hal 1
  IMetadata mpUpdatedHALMetadata;
  mutable pthread_rwlock_t mRWLock;

 public:  ////                    Instantiation.
  virtual ~MetadataProvider();
  explicit MetadataProvider(int32_t const i4OpenId);
  //
  MetadataProvider(int32_t const i4OpenId,
                   const IMetadata& aMeta_mtk,
                   camera_metadata* aMeta);

  virtual uint32_t getDeviceVersion() const;
  virtual int getDeviceFacing() const;
  virtual int getDeviceWantedOrientation() const;
  virtual int getDeviceSetupOrientation() const;
  virtual int getDeviceHasFlashLight() const;

 public:
  virtual int setStaticData(IMetadata* pMeta);
  virtual int restoreStaticData();

 protected:  ////                    Operations (Static Metadata).
  virtual status_t constructStaticMetadata(
      std::shared_ptr<IMetadataConverter> pConverter,
      camera_metadata** rpMetadata,
      IMetadata* mtkMetadata);
  virtual status_t impConstructStaticMetadata(IMetadata* metadata);
  virtual status_t impConstructStaticMetadata_by_SymbolName(
      std::string const& s8Symbol, IMetadata* metadata);

  void updateData(IMetadata* rMetadata);

  MBOOL isHdrSensor(MUINT const availVhdrEntryCount);

  MVOID updateHdrData(MBOOL const isHDRSensor,
                      MBOOL const isSingleFrameSupport,
                      MINT32 const hdrDetectionMode,
                      IMetadata* rMetadata);

 public:  ////                    Operations.
  virtual status_t onCreate();

 public:  ////                    Static Info Interface.
  virtual camera_metadata const* getStaticCharacteristics() const {
    return mpStaticCharacteristics;
  }
  virtual IMetadata const& getMtkStaticCharacteristics() const {
    return mpHALMetadata;
  }
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSMetadataProvider
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_METADATAPROVIDER_METADATAPROVIDER_H_
