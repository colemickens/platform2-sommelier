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

#define LOG_TAG "MtkCam/MetadataConverter"
//
#include "MyUtils.h"
#include "MetadataConverter.h"
#include <inttypes.h>
#include <memory>
#include <mtkcam/utils/metadata/conversion/MetadataConverter.h>
#include <mtkcam/utils/metadata/mtk_metadata_types.h>
#include <string>

using NSCam::IMetadata;
using NSCam::MPoint;
using NSCam::MRational;
using NSCam::MRect;
using NSCam::MSize;
using NSCam::status_t;
using NSCam::Type2Type;
using NSCam::TYPE_MDOUBLE;
using NSCam::TYPE_MFLOAT;
using NSCam::TYPE_MINT32;
using NSCam::TYPE_MINT64;
using NSCam::TYPE_MPoint;
using NSCam::TYPE_MRational;
using NSCam::TYPE_MRect;
using NSCam::TYPE_MSize;
using NSCam::TYPE_MUINT8;
//
/******************************************************************************
 *
 ******************************************************************************/

std::shared_ptr<IMetadataConverter> IMetadataConverter::createInstance(
    IMetadataTagSet const& pTagInfo) {
#if (PLATFORM_SDK_VERSION >= 21)
  return std::make_shared<MetadataConverter>(pTagInfo);
#else
  return NULL;
#endif
}

MetadataConverter::MetadataConverter(IMetadataTagSet const& pTagInfo)
    : mpTagInfo(pTagInfo) {}

size_t MetadataConverter::getCameraMetadataSize(
    const camera_metadata* metadata) const {
  return ::get_camera_metadata_size(metadata);
}

void MetadataConverter::freeCameraMetadata(camera_metadata* metadata) const {
  if (CC_LIKELY(metadata != nullptr)) {
    ::free_camera_metadata(metadata);
  }
}

MVOID
MetadataConverter::dumpAll(const IMetadata& rMetadata, int frameNo = -1) const {
#if (PLATFORM_SDK_VERSION >= 21)
  MY_LOGD("dump all metadata for frameNo %d count: %d", frameNo,
          rMetadata.count());
  for (size_t i = 0; i < rMetadata.count(); i++) {
    MUINT32 mtk_tag = rMetadata.entryAt(i).tag();
    MUINT32 mtk_type = getTagInfo()->getType(mtk_tag);
    MUINT32 android_tag = getTagInfo()->getAndroidTag((MUINT32)mtk_tag);
    MUINT32 android_type = get_camera_metadata_tag_type(android_tag);
    const char *tag_name, *tag_section;
    tag_section = get_camera_metadata_section_name(android_tag);
    if (tag_section == NULL) {
      tag_section = "unknownSection";
    }
    tag_name = get_camera_metadata_tag_name(android_tag);
    if (tag_name == NULL) {
      tag_name = "unknownTag";
    }
    const char* type_name;
    if (android_type >= NUM_TYPES) {
      type_name = "unknown";
    } else {
      type_name = camera_metadata_type_names[android_type];
    }
    std::string str = base::StringPrintf(
        "%s.%s (%05x): %s[%" PRIu32 "]", tag_section, tag_name, mtk_tag,
        type_name, rMetadata.entryAt(i).count());
    print(rMetadata, mtk_tag, mtk_type, str.c_str());
  }
#endif
}

MVOID
MetadataConverter::dump(const IMetadata& rMetadata, int frameNo = -1) const {
#if (PLATFORM_SDK_VERSION >= 21)
  MY_LOGD("dump partial metadata for frameNo %d", frameNo);
  for (size_t i = 0; i < rMetadata.count(); i++) {
    MUINT32 mtk_tag = rMetadata.entryAt(i).tag();
    MUINT32 mtk_type = getTagInfo()->getType(mtk_tag);
    MUINT32 android_tag = getTagInfo()->getAndroidTag((MUINT32)mtk_tag);
    MUINT32 android_type = get_camera_metadata_tag_type(android_tag);
    switch (android_tag) {
      case ANDROID_CONTROL_AE_TARGET_FPS_RANGE:
      case ANDROID_SENSOR_FRAME_DURATION:
      case ANDROID_CONTROL_AE_COMPENSATION_STEP:
      case ANDROID_CONTROL_AF_REGIONS:
      case ANDROID_CONTROL_AE_REGIONS:
      case ANDROID_SCALER_CROP_REGION:
      case ANDROID_SENSOR_EXPOSURE_TIME:
      case ANDROID_SENSOR_SENSITIVITY:
      case ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST: {
        const char *tag_name, *tag_section;
        tag_section = get_camera_metadata_section_name(android_tag);
        if (tag_section == NULL) {
          tag_section = "unknownSection";
        }
        tag_name = get_camera_metadata_tag_name(android_tag);
        if (tag_name == NULL) {
          tag_name = "unknownTag";
        }
        const char* type_name;
        if (android_type >= NUM_TYPES) {
          type_name = "unknown";
        } else {
          type_name = camera_metadata_type_names[android_type];
        }
        std::string str = base::StringPrintf(
            "%s.%s (%05x): %s[%" PRIu32 "]", tag_section, tag_name, mtk_tag,
            type_name, rMetadata.entryAt(i).count());
        print(rMetadata, mtk_tag, mtk_type, str);
      } break;
      default:
        break;
    }
  }
#endif
}

MVOID
MetadataConverter::print(const IMetadata& rMetadata,
                         const MUINT32 tag,
                         const MUINT32 type,
                         const std::string& str) const {
#if (PLATFORM_SDK_VERSION >= 21)
  std::string str_val;
  switch (type) {
    case TYPE_MUINT8: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        str_val +=
            base::StringPrintf("%d ", entry.itemAt(i, Type2Type<MUINT8>()));
      }
    } break;

    case TYPE_MINT32: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        str_val +=
            base::StringPrintf("%d ", entry.itemAt(i, Type2Type<MINT32>()));
      }
    } break;

    case TYPE_MFLOAT: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        str_val +=
            base::StringPrintf("%f ", entry.itemAt(i, Type2Type<MFLOAT>()));
      }
    } break;

    case TYPE_MINT64: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        MINT64 val = entry.itemAt(i, Type2Type<MINT64>());
        str_val += base::StringPrintf("%" PRId64 " ", val);
      }
    } break;
    case TYPE_MDOUBLE: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        str_val +=
            base::StringPrintf("%f ", entry.itemAt(i, Type2Type<MDOUBLE>()));
      }
    } break;
    case TYPE_MRational: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        MRational item = entry.itemAt(i, Type2Type<MRational>());
        str_val +=
            base::StringPrintf("[%d / %d] ", item.numerator, item.numerator);
      }
    } break;
    case TYPE_MPoint: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        MPoint item = entry.itemAt(i, Type2Type<MPoint>());
        str_val += base::StringPrintf("(%d, %d) ", item.x, item.y);
      }
    } break;
    case TYPE_MSize: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        MSize item = entry.itemAt(i, Type2Type<MSize>());
        str_val += base::StringPrintf("(%d,%d) ", item.w, item.h);
      }
    } break;
    case TYPE_MRect: {
      IMetadata::IEntry entry = rMetadata.entryFor(tag);
      for (size_t i = 0; i < entry.count(); i++) {
        MRect item = entry.itemAt(i, Type2Type<MRect>());
        str_val += base::StringPrintf("(%d, %d, %d, %d) ", item.p.x, item.p.y,
                                      item.s.w, item.s.h);
      }
    } break;
    default:
      break;
  }

  MY_LOGD("%s => %s", str.c_str(), str_val.c_str());
#endif
}
