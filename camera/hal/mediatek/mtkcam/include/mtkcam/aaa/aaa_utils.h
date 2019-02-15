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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_AAA_UTILS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_AAA_UTILS_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/IMetadata.h>

namespace NS3Av3 {
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
template <typename T>
inline MINT32 UPDATE_ENTRY_SINGLE(NSCam::IMetadata* metadata,
                                  MINT32 entry_tag,
                                  T value) {
  NSCam::IMetadata::IEntry entry(entry_tag);
  entry.push_back(value, NSCam::Type2Type<T>());
  return metadata->update(entry_tag, entry);
}

template <typename T>
inline MINT32 UPDATE_ENTRY_ARRAY(NSCam::IMetadata* metadata,
                                 MINT32 entry_tag,
                                 const T* array,
                                 MUINT32 size) {
  NSCam::IMetadata::IEntry entry(entry_tag);
  for (MUINT32 i = size; i != 0; i--) {
    entry.push_back(*array++, NSCam::Type2Type<T>());
  }
  return metadata->update(entry_tag, entry);
}

template <typename T>
inline MINT32 UPDATE_MEMORY(NSCam::IMetadata* metadata,
                            MINT32 entry_tag,
                            const T& data) {
  NSCam::IMetadata::Memory rTmp;
  rTmp.resize(sizeof(T));
  ::memcpy(rTmp.editArray(), &data, sizeof(T));
  return UPDATE_ENTRY_SINGLE(metadata, entry_tag, rTmp);
}

template <typename T>
inline MBOOL QUERY_ENTRY_SINGLE(const NSCam::IMetadata& metadata,
                                MINT32 entry_tag,
                                T* value) {
  NSCam::IMetadata::IEntry entry = metadata.entryFor(entry_tag);
  if ((entry.tag() != NSCam::IMetadata::IEntry::BAD_TAG) &&
      (!entry.isEmpty())) {
    *value = entry.itemAt(0, NSCam::Type2Type<T>());
    return MTRUE;
  }
  return MFALSE;
}

template <typename T>
inline MBOOL GET_ENTRY_SINGLE_IN_ARRAY(const NSCam::IMetadata& metadata,
                                       MINT32 entry_tag,
                                       T tag) {
  MUINT32 cnt = 0;

  NSCam::IMetadata::IEntry entry = metadata.entryFor(entry_tag);
  if ((entry.tag() != NSCam::IMetadata::IEntry::BAD_TAG) &&
      (!entry.isEmpty())) {
    cnt = entry.count();
    for (MUINT32 i = 0; i < cnt; i++) {
      if (tag == entry.itemAt(i, NSCam::Type2Type<T>())) {
        return MTRUE;
      }
    }
  }
  return MFALSE;
}

template <typename T>
inline MBOOL GET_ENTRY_ARRAY(const NSCam::IMetadata& metadata,
                             MINT32 entry_tag,
                             T* array,
                             MUINT32 size) {
  NSCam::IMetadata::IEntry entry = metadata.entryFor(entry_tag);
  if ((entry.tag() != NSCam::IMetadata::IEntry::BAD_TAG) &&
      (entry.count() == size)) {
    for (MUINT32 i = 0; i < size; i++) {
      *array++ = entry.itemAt(i, NSCam::Type2Type<T>());
    }
    return MTRUE;
  }
  return MFALSE;
}
};      // namespace NS3Av3
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_AAA_AAA_UTILS_H_
