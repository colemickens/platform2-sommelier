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

#define LOG_TAG "MtkCam/MetadataConvert"
//
#include "MyUtils.h"
#include <MetadataConverter.h>
#include <mtkcam/utils/metadata/IMetadataTagSet.h>
#include <mtkcam/utils/metadata/mtk_metadata_types.h>
#include <system/camera_metadata.h>

using NSCam::BAD_VALUE;
using NSCam::IMetadata;
using NSCam::MPoint;
using NSCam::MPointF;
using NSCam::MRational;
using NSCam::MRect;
using NSCam::MRectF;
using NSCam::MSize;
using NSCam::MSizeF;
using NSCam::NAME_NOT_FOUND;
using NSCam::NO_ERROR;
using NSCam::NO_INIT;
using NSCam::OK;
using NSCam::PERMISSION_DENIED;
using NSCam::Type2Type;
using NSCam::TYPE_MRect;
using NSCam::TYPE_MSize;
using NSCam::UNKNOWN_ERROR;

/******************************************************************************
 *
 ******************************************************************************/

template <class T>
static void AddToEntry(IMetadata::IEntry* entry, T* data_start) {
  entry->push_back(*data_start, Type2Type<T>());
}

#define ARRAY_TO_ENTRY(_ENTRY_, _DATA_START_, _TYPE_)               \
  if (_TYPE_ == TYPE_BYTE)                                          \
    AddToEntry(&_ENTRY_, reinterpret_cast<MUINT8*>(_DATA_START_));  \
  if (_TYPE_ == TYPE_INT32)                                         \
    AddToEntry(&_ENTRY_, reinterpret_cast<MINT32*>(_DATA_START_));  \
  if (_TYPE_ == TYPE_FLOAT)                                         \
    AddToEntry(&_ENTRY_, reinterpret_cast<MFLOAT*>(_DATA_START_));  \
  if (_TYPE_ == TYPE_INT64)                                         \
    AddToEntry(&_ENTRY_, reinterpret_cast<MINT64*>(_DATA_START_));  \
  if (_TYPE_ == TYPE_DOUBLE)                                        \
    AddToEntry(&_ENTRY_, reinterpret_cast<MDOUBLE*>(_DATA_START_)); \
  if (_TYPE_ == TYPE_RATIONAL)                                      \
    AddToEntry(&_ENTRY_, reinterpret_cast<MRational*>(_DATA_START_));

// camera_metadata --> IMetadata
MBOOL
MetadataConverter::convert(const camera_metadata* pMetadata,
                           IMetadata* rDstBuffer) const {
  CAM_LOGD("Convert from camera_metadata to IMetadata");

  if (pMetadata == NULL) {
    MY_LOGE("camera_metadat has not allocated");
    return false;
  }

  for (unsigned int i = 0; i < get_camera_metadata_entry_count(pMetadata);
       i++) {
    // (1) get android entry
    camera_metadata_entry android_entry;
    int result;
    if (OK !=
        (result = get_camera_metadata_entry(
             const_cast<camera_metadata*>(pMetadata), i, &android_entry))) {
      MY_LOGE("cannot get metadata entry");
      continue;
    }

    // (2) get mtk tag
    if (getTagInfo() == NULL) {
      MY_LOGD("get TagConvert fail\n");
      return false;
    }
#if (PLATFORM_SDK_VERSION >= 21)
    MUINT32 mtk_tag = getTagInfo()->getMtkTag((MUINT32)android_entry.tag);
#else
    MUINT32 mtk_tag = 0;
#endif
    if (mtk_tag == IMetadata::IEntry::BAD_TAG) {
      CAM_LOGE("%s: Tag 0x%x not found in Mtk Metadata. Shouldn't happened",
               __FUNCTION__, android_entry.tag);
      continue;
    }
    IMetadata::IEntry MtkEntry(mtk_tag);

    // (3.1) get types
    int android_type = android_entry.type;
    int mtk_type = getTagInfo()->getType(mtk_tag);

    CAM_LOGD(
        " android (tag: 0x%x, type: %d), data count: %zu, mtk (tag: 0x%x, "
        "name: %s, type: %d)",
        android_entry.tag, android_type, android_entry.count, mtk_tag,
        getTagInfo()->getName(mtk_tag), mtk_type);

    // (3.2) types are equal ==> normal copy.
    if (android_type == mtk_type) {
      for (unsigned int j = 0;
           j <
           android_entry.count * camera_metadata_type_size[android_entry.type];
           j += camera_metadata_type_size[android_entry.type]) {
        ARRAY_TO_ENTRY(MtkEntry, &android_entry.data.u8[j], android_entry.type);
      }
    } else {  // (3.3) types are not equal
      // by case
      if (android_type == TYPE_INT32 && mtk_type == TYPE_MRect) {
        for (size_t j = 0; j < android_entry.count; j += 4) {
          MRect data(MPoint(android_entry.data.i32[j * 4],
                            android_entry.data.i32[j * 4 + 1]),
                     MSize(android_entry.data.i32[j * 4 + 2],
                           android_entry.data.i32[j * 4 + 3]));
          AddToEntry(&MtkEntry, &data);
        }
      } else if (android_type == TYPE_INT32 && mtk_type == TYPE_MSize) {
        for (size_t j = 0; j < android_entry.count; j += 2) {
          MSize data(android_entry.data.i32[j * 2],
                     android_entry.data.i32[j * 2 + 1]);
          AddToEntry(&MtkEntry, &data);
        }
      }
    }

    rDstBuffer->update(mtk_tag, MtkEntry);
  }

  return true;
}
