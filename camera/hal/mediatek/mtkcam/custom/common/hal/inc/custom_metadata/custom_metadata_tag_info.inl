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
#include <mtkcam/utils/metadata/client/mtk_metadata_tag_info.inl>
#include "custom_metadata_tag.h"

/******************************************************************************
 *
 ******************************************************************************/
#ifndef _IMP_SECTION_INFO_

    template <mtk_camera_metadata_section section> struct MetadataSectionInfo
    {};

    #define _IMP_SECTION_INFO_(_section_, _name_) \
        template <> struct MetadataSectionInfo<_section_> { \
            enum \
            { \
                SECTION_START = _section_##_START, \
                SECTION_END   = _section_##_END, \
            }; \
        };

#endif


/******************************************************************************
 * SECTION INFO
 ******************************************************************************/
_IMP_SECTION_INFO_(CUSTOM_DEVICE_CAPABILITIES, "custom.device.capabilities")
_IMP_SECTION_INFO_(CUSTOM_CAPTURE_METADATA, "custom.capture.metadata")

#undef  _IMP_SECTION_INFO_
/******************************************************************************
 * TAG INFO
 ******************************************************************************/
_IMP_TAG_INFO_(ANDROID_HW_SENSOR_WB_RANGE, MINT32,     "sensorWbRange")
_IMP_TAG_INFO_(ANDROID_HW_SENSOR_WB_VALUE, MINT32,     "sensorWbValue")

#undef  _IMP_TAG_INFO_
