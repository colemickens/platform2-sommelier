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

#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#include <string.h>

namespace NSCam {
namespace TuningUtils {

FILE_DUMP_NAMING_HINT::FILE_DUMP_NAMING_HINT() {
  ImgWidth = 0;
  ImgHeight = 0;
  ImgFormat = -1;
  IspProfile = -1;
  SensorFormatOrder = -1;
  SensorType = -1;
  SensorOpenId = -1;
  SensorDev = -1;
  UniqueKey = -1;
  FrameNo = -1;
  RequestNo = -1;
  EvValue = -1;
  memset(additStr, '\0', 32);
}

bool extract(FILE_DUMP_NAMING_HINT* pHint, const IMetadata* pMetadata) {
  bool ret = true;

  if (pMetadata == NULL) {
    return false;
  }

  ret &= IMetadata::getEntry<int>(pMetadata, MTK_PIPELINE_UNIQUE_KEY,
                                  &pHint->UniqueKey);
  ret &= IMetadata::getEntry<int>(pMetadata, MTK_PIPELINE_FRAME_NUMBER,
                                  &pHint->FrameNo);
  ret &= IMetadata::getEntry<int>(pMetadata, MTK_PIPELINE_REQUEST_NUMBER,
                                  &pHint->RequestNo);
  ret &= IMetadata::getEntry<int>(pMetadata, MTK_PIPELINE_EV_VALUE,
                                  &pHint->EvValue);

  return true;
}

bool extract(FILE_DUMP_NAMING_HINT* pHint, const IImageBuffer* pImgBuf) {
  pHint->ImgWidth = pImgBuf->getImgSize().w;
  pHint->ImgHeight = pImgBuf->getImgSize().h;
  pHint->ImgFormat = pImgBuf->getImgFormat();
  return true;
}

bool extract_by_SensorDev(FILE_DUMP_NAMING_HINT* pHint, int sensorDev) {
  pHint->SensorDev = sensorDev;
  IHalSensorList* const pIHalSensorList = GET_HalSensorList();
  if (pIHalSensorList) {
    NSCam::SensorStaticInfo sensorStaticInfo;
    memset(&sensorStaticInfo, 0, sizeof(NSCam::SensorStaticInfo));
    pIHalSensorList->querySensorStaticInfo((MUINT)sensorDev, &sensorStaticInfo);
    pHint->SensorFormatOrder = sensorStaticInfo.sensorFormatOrder;
    return true;
  } else {
    pHint->SensorFormatOrder = -1;
  }
  return false;
}

bool extract_by_SensorOpenId(FILE_DUMP_NAMING_HINT* pHint, int openId) {
  pHint->SensorOpenId = openId;
  MUINT sensorDev = 0;
  IHalSensorList* const pIHalSensorList = GET_HalSensorList();
  if (pIHalSensorList) {
    sensorDev = (MUINT32)pIHalSensorList->querySensorDevIdx(openId);
    pHint->SensorDev = sensorDev;
    NSCam::SensorStaticInfo sensorStaticInfo;
    memset(&sensorStaticInfo, 0, sizeof(NSCam::SensorStaticInfo));
    pIHalSensorList->querySensorStaticInfo(sensorDev, &sensorStaticInfo);
    pHint->SensorFormatOrder = sensorStaticInfo.sensorFormatOrder;
    return true;
  } else {
    pHint->SensorFormatOrder = -1;
  }
  return false;
}

}  // namespace TuningUtils
}  // namespace NSCam
