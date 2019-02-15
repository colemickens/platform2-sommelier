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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_EXIF_EXIFWRITER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_EXIF_EXIFWRITER_H_

#include "../CameraFeature_Common.h"

#include <custom/feature/mfnr/camera_custom_mfll.h>
#include <custom/debug_exif/dbg_exif_param.h>
#include <map>
#include <set>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class ExifWriter {
 public:
  explicit ExifWriter(const char* name);
  ~ExifWriter();

  MBOOL sendData(MINT32 reqId, MINT32 tag, MINT32 value);

  MBOOL makeExifFromCollectedData(RequestPtr request);

  MBOOL addReqMapping(MINT32 mainReqId, const set<MINT32>& vSubReqId);

  MBOOL makeExifFromCollectedData_multiframe_mapping(RequestPtr request);

 private:
  const char* tagToName(int tag);

  uint32_t doExifUpdate(RequestPtr request, const map<MUINT32, MUINT32>& data);

 private:
  const char* mName;

  MINT32 mEnable = -1;

  static std::mutex gLock;
  static std::map<MINT32, map<MUINT32, MUINT32> > gvCollectedData;
  static std::map<MINT32, set<MINT32> > gvReqMapping;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_EXIF_EXIFWRITER_H_
