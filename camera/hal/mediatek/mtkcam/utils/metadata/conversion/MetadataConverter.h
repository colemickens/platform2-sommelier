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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METADATA_CONVERSION_METADATACONVERTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METADATA_CONVERSION_METADATACONVERTER_H_
//
#include <system/camera_metadata.h>

#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/metadata/IMetadataConverter.h>
#include <string>
#include <vector>

using NSCam::IMetadata;
using NSCam::IMetadataConverter;
using NSCam::IMetadataTagSet;
using NSCam::status_t;
/******************************************************************************
 *  Metadata Converter
 ******************************************************************************/
class MetadataConverter : public IMetadataConverter {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  IMetadataTagSet mpTagInfo;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  explicit MetadataConverter(IMetadataTagSet const& pTagInfo);
  ~MetadataConverter() {}

  IMetadataTagSet const* getTagInfo() const { return &mpTagInfo; }

  MBOOL convert(const camera_metadata*, IMetadata*) const;
  MBOOL convert(const IMetadata&, camera_metadata**, size_t*) const;
  MBOOL convertWithoutAllocate(const IMetadata&, camera_metadata**) const;
  size_t getCameraMetadataSize(const camera_metadata* metadata) const;
  void freeCameraMetadata(camera_metadata*) const;

  status_t get_data_count(IMetadata const&, size_t*, size_t*) const;

  MVOID dump(const IMetadata& rMetadata, int frameNo) const;
  MVOID dumpAll(const IMetadata& rMetadata, int frameNo) const;

 private:
  MBOOL tryToConvert(const IMetadata& rMetadata,
                     camera_metadata** pDstMetadata,
                     size_t* entryCount,
                     size_t* dataCount) const;

  static status_t update(camera_metadata** mBuffer,
                         MUINT32 tag,
                         const void* data,
                         MUINT32 data_count);

  static status_t resizeIfNeeded(camera_metadata** pBuffer,
                                 size_t extraEntries,
                                 size_t extraData);

  MVOID print(const IMetadata& rMetadata,
              const MUINT32 tag,
              const MUINT32 type,
              const std::string& str) const;
};

/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METADATA_CONVERSION_METADATACONVERTER_H_
