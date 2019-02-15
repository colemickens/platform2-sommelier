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

#define LOG_TAG "MtkCam/TemplateRequest"
//
#include "MyUtils.h"
#include <dlfcn.h>
#include <hardware/camera3.h>
// converter
#include <memory>
#include <mtkcam/def/common.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/metadata/IMetadataTagSet.h>
#include <mtkcam/utils/metadata/IMetadataConverter.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/metadata/client/TagMap.h>
#include <mtkcam/utils/metadata/mtk_metadata_types.h>
#include <mtkcam/utils/metastore/templateRequest/TemplateRequest.h>
#include <mtkcam/utils/std/Log.h>
#include <string>
#include <system/camera_metadata.h>
//
using NSCam::IHalSensorList;
using NSCam::IMetadata;
using NSCam::IMetadataConverter;
using NSCam::IMetadataProvider;
using NSCam::IMetadataTagSet;
using NSCam::status_t;
using NSCam::Type2Type;
using NSCam::Type2TypeEnum;
using NSTemplateRequest::TemplateRequest;

/******************************************************************************
 *
 ******************************************************************************/
status_t TemplateRequest::impConstructRequestMetadata_by_SymbolName(
    std::string const& s8Symbol, IMetadata* metadata, int const requestType) {
  typedef status_t (*PFN_T)(IMetadata * metadata, int const requestType);
  //
  PFN_T pfn = (PFN_T)::dlsym(RTLD_DEFAULT, s8Symbol.c_str());
  if (!pfn) {
    MY_LOGD("%s not found", s8Symbol.c_str());
    return NAME_NOT_FOUND;
  }
  //
  status_t const status = pfn(metadata, requestType);
  MY_LOGD_IF(OK != status, "%s returns status[%s(%d)]", s8Symbol.c_str(),
             ::strerror(-status), -status);
  //
  return status;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t TemplateRequest::impConstructRequestMetadata(IMetadata* metadata,
                                                      int const requestType) {
  status_t status = OK;
  //
  {
#if 1
    std::string const s8Symbol_Sensor =
        base::StringPrintf("%s_DEVICE_%s", PREFIX_FUNCTION_REQUEST_METADATA,
                           mInfo.getSensorDrvName());
    status = impConstructRequestMetadata_by_SymbolName(s8Symbol_Sensor,
                                                       metadata, requestType);
    if (OK == status) {
      goto lbDevice;
    }
#endif
    //
    std::string const s8Symbol_Common = base::StringPrintf(
        "%s_DEVICE_%s", PREFIX_FUNCTION_REQUEST_METADATA, "COMMON");
    status = impConstructRequestMetadata_by_SymbolName(s8Symbol_Common,
                                                       metadata, requestType);
    if (OK == status) {
      goto lbDevice;
    }
  }
  //
lbDevice : {
#if 1
  std::string const s8Symbol_Sensor =
      base::StringPrintf("%s_PROJECT_%s", PREFIX_FUNCTION_REQUEST_METADATA,
                         mInfo.getSensorDrvName());
  status = impConstructRequestMetadata_by_SymbolName(s8Symbol_Sensor, metadata,
                                                     requestType);
  if (OK == status) {
    return OK;
  }
#endif
  //
  std::string const s8Symbol_Common = base::StringPrintf(
      "%s_PROJECT_%s", PREFIX_FUNCTION_REQUEST_METADATA, "COMMON");
  status = impConstructRequestMetadata_by_SymbolName(s8Symbol_Common, metadata,
                                                     requestType);
  if (OK == status) {
    return OK;
  }
}
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
static bool setTagInfo(IMetadataTagSet* rtagInfo) {
#define _IMP_SECTION_INFO_(...)
#undef _IMP_TAG_INFO_
#define _IMP_TAG_INFO_(_tag_, _type_, _name_) \
  rtagInfo->addTag(_tag_, _name_, Type2TypeEnum<_type_>::typeEnum);

#include <custom_metadata/custom_metadata_tag_info.inl>

#undef _IMP_TAG_INFO_

#undef _IMP_TAGCONVERT_
#define _IMP_TAGCONVERT_(_android_tag_, _mtk_tag_) \
  rtagInfo->addTagMap(_android_tag_, _mtk_tag_);
#if (PLATFORM_SDK_VERSION >= 21)
  ADD_ALL_MEMBERS;
#else
#warning "no tag info"
#endif

#undef _IMP_TAGCONVERT_

  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t TemplateRequest::constructRequestMetadata(int const requestType,
                                                   camera_metadata** rpMetadata,
                                                   IMetadata* rMtkMetadata) {
  MY_LOGD("constructRequestMetadata");

  status_t status = OK;

  // -----(1)-----//
  // get static informtation from customization (with camera_metadata format)
  // calculate its entry count and data count
  if (OK != (status = impConstructRequestMetadata(rMtkMetadata, requestType))) {
    MY_LOGE(
        "Unable evaluate the size for camera static info - status[%s(%d)]\n",
        ::strerror(-status), -status);
    return status;
  }
  MY_LOGD("Allocating %d entries from customization", rMtkMetadata->count());

  // calculate its entry count and data count
  // init converter
  IMetadataTagSet tagInfo;
  setTagInfo(&tagInfo);
  std::shared_ptr<IMetadataConverter> pConverter =
      IMetadataConverter::createInstance(tagInfo);
  size_t entryCount = 0;
  size_t dataCount = 0;
  MBOOL ret =
      pConverter->get_data_count(*rMtkMetadata, &entryCount, &dataCount);
  if (ret != OK) {
    MY_LOGE("get Imetadata count error\n");
    return UNKNOWN_ERROR;
  }
  MY_LOGD("Allocating %zu entries, %zu extra bytes from HAL modules",
          entryCount, dataCount);

  // -----(2)-----//
  // overwrite
  updateData(rMtkMetadata);

  // -----(3)-----//
  // convert to android metadata
  pConverter->convert(*rMtkMetadata, rpMetadata);
  ::sort_camera_metadata(*rpMetadata);

  return status;
}

/******************************************************************************
 *
 ******************************************************************************/
void TemplateRequest::updateData(IMetadata* rMetadata) {
  std::shared_ptr<IMetadataProvider> obj =
      NSCam::NSMetadataProviderManager::valueFor(mInfo.getDeviceId());
  if (obj == nullptr) {
    obj = IMetadataProvider::create(mInfo.getDeviceId());
    NSCam::NSMetadataProviderManager::add(mInfo.getDeviceId(), obj);
  }

  IMetadata static_meta = obj->getMtkStaticCharacteristics();

  {
    MRect cropRegion;
    IMetadata::IEntry active_array_entry =
        static_meta.entryFor(MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION);
    if (!active_array_entry.isEmpty()) {
      cropRegion = active_array_entry.itemAt(0, Type2Type<MRect>());
      cropRegion.p.x = 0;
      cropRegion.p.y = 0;
      IMetadata::IEntry entry(MTK_SCALER_CROP_REGION);
      entry.push_back(cropRegion, Type2Type<MRect>());
      rMetadata->update(MTK_SCALER_CROP_REGION, entry);
    }
  }

  {
    // === 3DNR ===
    IMetadata::IEntry nr3d_avail_entry =
        static_meta.entryFor(MTK_NR_FEATURE_AVAILABLE_3DNR_MODES);
    IMetadata::IEntry nr3d_entry =
        rMetadata->entryFor(MTK_NR_FEATURE_3DNR_MODE);
    MBOOL support3DNR =
        (IMetadata::IEntry::indexOf(nr3d_avail_entry,
                                    (MINT32)MTK_NR_FEATURE_3DNR_MODE_ON) != -1);

    if (nr3d_entry.isEmpty()) {
      // tag not set in custom, use default on
      if (support3DNR) {
        IMetadata::IEntry nr3d_entry_new(MTK_NR_FEATURE_3DNR_MODE);
        nr3d_entry_new.push_back(MTK_NR_FEATURE_3DNR_MODE_ON,
                                 Type2Type<MINT32>());
        rMetadata->update(MTK_NR_FEATURE_3DNR_MODE, nr3d_entry_new);
      }
    } else {
      // tag set in custom, confirm 3DNR support or not (just protect mechanism)
      if (!support3DNR) {
        nr3d_entry.clear();
        nr3d_entry.push_back(MTK_NR_FEATURE_3DNR_MODE_OFF, Type2Type<MINT32>());
        rMetadata->update(MTK_NR_FEATURE_3DNR_MODE, nr3d_entry);
      }
    }
  }

#ifndef EIS_SUPPORTED
  {
    IMetadata::IEntry eis_entry =
        rMetadata->entryFor(MTK_CONTROL_VIDEO_STABILIZATION_MODE);
    if (!eis_entry.isEmpty()) {
      eis_entry.clear();
      eis_entry.push_back(MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF,
                          Type2Type<MUINT8>());
      rMetadata->update(MTK_CONTROL_VIDEO_STABILIZATION_MODE, eis_entry);
    }
    IMetadata::IEntry adveis_entry =
        rMetadata->entryFor(MTK_EIS_FEATURE_EIS_MODE);
    if (!adveis_entry.isEmpty()) {
      adveis_entry.clear();
      adveis_entry.push_back(MTK_CONTROL_VIDEO_STABILIZATION_MODE_OFF,
                             Type2Type<MINT32>());
      rMetadata->update(MTK_EIS_FEATURE_EIS_MODE, adveis_entry);
    }
  }
#endif
}

/******************************************************************************
 *
 ******************************************************************************/
status_t TemplateRequest::onCreate(int iOpenId) {
  MY_LOGD("+");
  //
  IHalSensorList* pHalSensorList = GET_HalSensorList();
  int32_t sensorType = pHalSensorList->queryType(iOpenId);
  const char* sensorDrvName = pHalSensorList->queryDriverName(iOpenId);
  mInfo = Info(iOpenId, sensorType, sensorDrvName);

  //  Standard template types
  for (int type = CAMERA3_TEMPLATE_PREVIEW; type < CAMERA3_TEMPLATE_COUNT;
       type++) {
    camera_metadata* metadata = NULL;
    IMetadata mtkMetadata;
    status_t status = constructRequestMetadata(type, &metadata, &mtkMetadata);
    if (OK != status || NULL == metadata || mtkMetadata.isEmpty()) {
      MY_LOGE("constructRequestMetadata - type:%#x metadata:%p status[%s(%d)]",
              type, metadata, ::strerror(-status), -status);
      return status;
    }
    //
    mMapRequestTemplate.emplace(type, metadata);

    mMapRequestTemplateMetadata.emplace(type, mtkMetadata);
  }

#if 0
    //  vendor-defined request templates
    for (int type = CAMERA3_VENDOR_TEMPLATE_START;
         type < CAMERA3_VENDOR_TEMPLATE_COUNT; type++) {
        camera_metadata* metadata = NULL;
        status = constructRequestMetadata(type, metadata);
        if (OK != status || NULL == metadata) {
            MY_LOGE("CRM - type:%#x metadata:%p status[%s(%d)]",
                    type, metadata, ::strerror(-status), -status);
            return  status;
        }
        //
        MapRequestTemplate.add(type, metadata);
    }
#endif
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
camera_metadata const* TemplateRequest::getData(int requestType) {
  if (mMapRequestTemplate.find(requestType) == mMapRequestTemplate.end()) {
    return NULL;
  }
  return mMapRequestTemplate.at(requestType);
}

/******************************************************************************
 *
 ******************************************************************************/
IMetadata const& TemplateRequest::getMtkData(int requestType) {
  return mMapRequestTemplateMetadata.at(requestType);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<ITemplateRequest> ITemplateRequest::getInstance(int iOpenId) {
  std::shared_ptr<TemplateRequest> p = std::make_shared<TemplateRequest>();
  if (p != nullptr) {
    p->onCreate(iOpenId);
  }
  return p;
}
