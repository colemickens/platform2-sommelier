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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METASTORE_ITEMPLATEREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METASTORE_ITEMPLATEREQUEST_H_
//
#include <memory>
#include <mtkcam/utils/metadata/IMetadata.h>

struct camera_metadata;
//
namespace NSCam {
/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC ITemplateRequest {
 public:  ////                    Interface.
  static std::shared_ptr<ITemplateRequest> getInstance(int iOpenId);

  virtual ~ITemplateRequest() {}

  virtual camera_metadata const* getData(int requestType) = 0;

  virtual IMetadata const& getMtkData(int requestType) = 0;
};

/******************************************************************************
 *  MetadataProvider Manager
 ******************************************************************************/
namespace NSTemplateRequestManager {
void clear();
ssize_t VISIBILITY_PUBLIC add(int32_t deviceId,
                              std::shared_ptr<ITemplateRequest> pRequest);

std::shared_ptr<ITemplateRequest> VISIBILITY_PUBLIC valueFor(int32_t deviceId);
std::shared_ptr<ITemplateRequest> valueAt(size_t index);
int32_t keyAt(size_t index);
ssize_t indexOfKey(int32_t deviceId);

};  // namespace NSTemplateRequestManager

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METASTORE_ITEMPLATEREQUEST_H_
