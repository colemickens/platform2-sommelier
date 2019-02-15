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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_COMMON_H_

#include "P2_Header.h"
#include "P2_Logger.h"

namespace P2 {

#define INVALID_P1_ISO_VAL (-99999)

template <typename T>
MBOOL tryGet(const IMetadata& meta, MUINT32 tag, T* val) {
  MBOOL ret = MFALSE;
  IMetadata::IEntry entry = meta.entryFor(tag);
  if (!entry.isEmpty()) {
    *val = entry.itemAt(0, Type2Type<T>());
    ret = MTRUE;
  }
  return ret;
}

template <typename T>
MBOOL tryGet(const IMetadata* meta, MUINT32 tag, T* val) {
  return (meta != nullptr) ? tryGet<T>(*meta, tag, val) : MFALSE;
}

template <typename T>
MBOOL trySet(IMetadata* meta, MUINT32 tag, const T& val) {
  if (meta != nullptr) {
    MBOOL ret = MFALSE;
    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    ret = (meta->update(tag, entry) == OK);
    return ret;
  } else {
    return MFALSE;
  }
}

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_COMMON_H_
