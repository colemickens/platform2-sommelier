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

#define LOG_TAG "MtkCam/MetadataProvider.Custom"
//
#include "Info.h"
#include <custom_metadata/custom_metadata_tag.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/Log.h>

using NSCam::IMetadata;
using NSCam::Type2Type;
using NSMetadataProvider::Info;
/******************************************************************************
 *
 ******************************************************************************/

#define CONFIG_METADATA_BEGIN(_tag_)  \
  {                                   \
    IMetadata::IEntry myENTRY(_tag_); \
    IMetadata*& myCAPABILITY = rMetadata;

#define CONFIG_METADATA_END()                                            \
  {                                                                      \
    MERROR err = myCAPABILITY->update(myENTRY.tag(), myENTRY);           \
    if (OK != err) {                                                     \
      MY_LOGE("IMetadata::update(), tag:%d err:%d", myENTRY.tag(), err); \
      return MFALSE;                                                     \
    }                                                                    \
  }                                                                      \
  }

#define CONFIG_METADATA2_BEGIN(_tag_) \
  {                                   \
    IMetadata::IEntry myENTRY(_tag_);

#define CONFIG_METADATA2_END()                  \
  myCAPABILITY2.update(myENTRY.tag(), myENTRY); \
  }

#define CONFIG_ENTRY_METADATA(_macros_...)                             \
  {                                                                    \
    IMetadata myCAPABILITY2;                                           \
    _macros_ myENTRY.push_back(myCAPABILITY2, Type2Type<IMetadata>()); \
  }

#define CONFIG_ENTRY_VALUE(_value_, _type_) \
  myENTRY.push_back(_value_, Type2Type<_type_>());

/******************************************************************************
 *
 ******************************************************************************/
#include <custgen.config_static.h>
#include <mtkcam/utils/metadata/mtk_metadata_types.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSMetadataProvider {
void showCustInfo() {
#if defined(MY_CUST_VERSION)
  MY_LOGD("MY_CUST_VERSION=\"%s\"", MY_CUST_VERSION);
#endif
#if defined(MY_CUST_FTABLE_FILE_LIST)
  MY_LOGD("MY_CUST_FTABLE_FILE_LIST=\"%s\"", MY_CUST_FTABLE_FILE_LIST);
#endif
#if defined(MY_CUST_FTABLE_FINAL_FILE)
  MY_LOGD("MY_CUST_FTABLE_FINAL_FILE=\"%s\"", MY_CUST_FTABLE_FINAL_FILE);
#endif
}
};  //  namespace NSMetadataProvider
