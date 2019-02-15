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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_IMETADATACONVERTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_IMETADATACONVERTER_H_
//
#include "IMetadata.h"
#include "IMetadataTagSet.h"
#include <mtkcam/def/common.h>
#include <memory>

struct camera_metadata;

namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC IMetadataConverter {
 public:
  static std::shared_ptr<IMetadataConverter> createInstance(
      IMetadataTagSet const& pTagInfo);
  virtual MBOOL convert(const camera_metadata* src, IMetadata* dst) const = 0;
  virtual MBOOL convert(const IMetadata& src,
                        camera_metadata** dst,
                        size_t* pDstSize = nullptr) const = 0;
  virtual MBOOL convertWithoutAllocate(const IMetadata& src,
                                       camera_metadata** dst) const = 0;
  virtual size_t getCameraMetadataSize(
      const camera_metadata* metadata) const = 0;
  virtual void freeCameraMetadata(camera_metadata* metadata) const = 0;
  virtual status_t get_data_count(IMetadata const& src,
                                  size_t* entryCount,
                                  size_t* dataCount) const = 0;

  virtual MVOID dump(const IMetadata& rMetadata, int frameNo = -1) const = 0;
  virtual MVOID dumpAll(const IMetadata& rMetadata, int frameNo = -1) const = 0;
  virtual ~IMetadataConverter() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_IMETADATACONVERTER_H_
