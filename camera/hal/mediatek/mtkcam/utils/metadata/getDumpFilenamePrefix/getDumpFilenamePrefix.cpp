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

#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/getDumpFilenamePrefix.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <stdio.h>
#include <string.h>

namespace NSCam {

template <typename T>
inline MBOOL QUERY_ENTRY_SINGLE(const IMetadata& metadata,
                                MINT32 entry_tag,
                                T* value) {
  IMetadata::IEntry entry = metadata.entryFor(entry_tag);
  if (entry.tag() != IMetadata::IEntry::BAD_TAG) {
    *value = entry.itemAt(0, Type2Type<T>());
    return MTRUE;
  }
  return MFALSE;
}

const char* getDumpFilenamePrefix(char* pPrefix,
                                  int nPrefix,
                                  const IMetadata* /*appMeta*/,
                                  const IMetadata* halMeta) {
  MINT32 t32;
  MUINT32 UniqueKey = 0, FrameNo = 0, RequestNo = 0;
  char* ptr = pPrefix;
  int n;

  if (halMeta == NULL) {
    return "";
  }
  if (pPrefix == NULL) {
    return "";
  }

  n = snprintf(ptr, nPrefix, DUMP_PATH);
  ptr += n;
  nPrefix -= n;

  if (QUERY_ENTRY_SINGLE(*halMeta, MTK_PIPELINE_UNIQUE_KEY, &t32)) {
    UniqueKey = (MUINT32)t32;
    if (UniqueKey > 999999999) {
      UniqueKey = 999999999;
    }
    n = snprintf(ptr, nPrefix, "%09d", UniqueKey);
  } else {
    n = snprintf(ptr, nPrefix, "uniquekey");
  }
  ptr += n;
  nPrefix -= n;

  if (QUERY_ENTRY_SINGLE(*halMeta, MTK_PIPELINE_FRAME_NUMBER, &t32)) {
    FrameNo = (MUINT32)t32;
    if (FrameNo > 99999999) {
      FrameNo = 99999999;
    }
    n = snprintf(ptr, nPrefix, "-%04d", FrameNo);
  } else {
    n = snprintf(ptr, nPrefix, "-frme");
  }
  ptr += n;
  nPrefix -= n;

  if (QUERY_ENTRY_SINGLE(*halMeta, MTK_PIPELINE_REQUEST_NUMBER, &t32)) {
    RequestNo = (MUINT32)t32;
    if (RequestNo > 9999) {
      RequestNo = 9999;
    }
    n = snprintf(ptr, nPrefix, "-%04d", RequestNo);
  } else {
    n = snprintf(ptr, nPrefix, "-rqst");
  }
  ptr += n;
  nPrefix -= n;

  return pPrefix;
}

};  // namespace NSCam
