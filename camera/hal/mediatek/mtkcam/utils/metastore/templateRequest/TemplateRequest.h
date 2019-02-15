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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_TEMPLATEREQUEST_TEMPLATEREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_TEMPLATEREQUEST_TEMPLATEREQUEST_H_

#include "custom/Info.h"

#include <map>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/metastore/ITemplateRequest.h>
#include <mtkcam/utils/metadata/IMetadataConverter.h>
#include <string>
#include <system/camera_metadata.h>

/******************************************************************************
 *
 ******************************************************************************/
using NSCam::IMetadata;
using NSCam::ITemplateRequest;
using NSCam::status_t;

namespace NSTemplateRequest {

/******************************************************************************
 *
 ******************************************************************************/
class TemplateRequest : public ITemplateRequest {
 public:
  TemplateRequest() {}

  virtual ~TemplateRequest() {}

  virtual camera_metadata const* getData(int requestType);
  virtual IMetadata const& getMtkData(int requestType);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Definitions.
  typedef std::map<int, camera_metadata*> RequestTemplateMap_t;
  typedef std::map<int, IMetadata> RequestTemplateMetadataMap_t;

 protected:  ////                    Data Members.
  Info mInfo;
  RequestTemplateMap_t mMapRequestTemplate;
  RequestTemplateMetadataMap_t mMapRequestTemplateMetadata;

 protected:  ////                    Operations (Request Metadata).
  virtual status_t constructRequestMetadata(int const requestType,
                                            camera_metadata** rpMetadata,
                                            IMetadata* rMtkMetadata);
  virtual status_t impConstructRequestMetadata(IMetadata* metadata,
                                               int const requestType);
  virtual status_t impConstructRequestMetadata_by_SymbolName(
      std::string const& s8Symbol, IMetadata* metadata, int const requestType);

  virtual void updateData(IMetadata* rMetadata);

 public:
  virtual status_t onCreate(int iOpenId);
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSTemplateRequest
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_TEMPLATEREQUEST_TEMPLATEREQUEST_H_
