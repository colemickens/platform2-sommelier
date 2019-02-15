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

#define LOG_TAG "mtkcam-FDIntentPolicy"

#include <compiler.h>
#include <mtkcam/pipeline/policy/IFaceDetectionIntentPolicy.h>
//
#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/**
 * Make a function target as a policy - default version
 */
FunctionType_FaceDetectionIntentPolicy makePolicy_FDIntent_Default() {
  return [](fdintent::RequestOutputParams& out,
            fdintent::RequestInputParams const& in) -> int {
    if (!in.hasFDNodeConfigured) {
      out.isFdEnabled = false;
      return OK;
    }

    auto const pMetadata = in.pRequest_AppControl;
    if (CC_UNLIKELY(!pMetadata)) {
      MY_LOGE("null app control input params");
      return -EINVAL;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("vendor.debug.camera.fd.enable", value, "1");
    int FDMetaEn = atoi(value);
    out.isFdEnabled = in.isFdEnabled_LastFrame || FDMetaEn;
    out.isFDMetaEn = FDMetaEn;
    return OK;
  };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
