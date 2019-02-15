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

#include <mtkcam/feature/utils/p2/P2PlatInfo.h>

#if MTK_DP_ENABLE  // check later
#include <DpDataType.h>
#include <DpIspStream.h>
#endif
#include <INormalStream.h>
#include <isp_tuning/isp_tuning.h>
#include <map>
#include <memory>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/std/Log.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MtkCam/P2PlatInfo"

using NSCam::NSIoPipe::EDIPHWVersion_40;
using NSCam::NSIoPipe::EDIPHWVersion_50;
using NSCam::NSIoPipe::EDIPINFO_DIPVERSION;
using NSCam::v4l2::INormalStream;

namespace NSCam {
namespace Feature {
namespace P2Util {

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
  return (meta != NULL) ? tryGet<T>(*meta, tag, val) : MFALSE;
}

template <typename T>
MBOOL trySet(IMetadata* meta, MUINT32 tag, const T& val) {
  if (meta != NULL) {
    MBOOL ret = MFALSE;
    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    ret = (meta->update(tag, entry) == OK);
    return ret;
  } else {
    return MFALSE;
  }
}

class P2PlatInfoImp : public P2PlatInfo {
 public:
  explicit P2PlatInfoImp(MUINT32 sensorID);
  ~P2PlatInfoImp();
  virtual MBOOL isDip50() const;
  virtual MRect getActiveArrayRect() const;

 private:
  MVOID initSensorDev();
  MVOID initActiveArrayRect();

 private:
  MUINT32 mSensorID = -1;

  std::map<NSIoPipe::EDIPInfoEnum, MUINT32> mDipInfo;
  MUINT32 mDipVersion = NSIoPipe::EDIPHWVersion_40;
#if MTK_DP_ENABLE
  std::map<DP_ISP_FEATURE_ENUM, bool> mIspFeature;
#endif

  NSCam::IHalSensorList* mHalSensorList = NULL;
  NSIspTuning::ESensorDev_T mSensorDev;

  MRect mActiveArrayRect;
};

P2PlatInfoImp::P2PlatInfoImp(MUINT32 sensorID) {
  mSensorID = sensorID;
  mDipInfo[EDIPINFO_DIPVERSION] = EDIPHWVersion_40;
#if MTK_DP_ENABLE
  if (!INormalStream::queryDIPInfo(mDipInfo)) {
    MY_LOGE("queryDIPInfo fail!");
  }
#endif
  mDipVersion = mDipInfo[EDIPINFO_DIPVERSION];
#if MTK_DP_ENABLE
  DpIspStream::queryISPFeatureSupport(mIspFeature);
#endif
  initSensorDev();
  initActiveArrayRect();
}

P2PlatInfoImp::~P2PlatInfoImp() {}

MBOOL P2PlatInfoImp::isDip50() const {
  return MTRUE;
}

MRect P2PlatInfoImp::getActiveArrayRect() const {
  return mActiveArrayRect;
}

MVOID P2PlatInfoImp::initSensorDev() {
  mHalSensorList = GET_HalSensorList();
  mSensorDev =
      (NSIspTuning::ESensorDev_T)mHalSensorList->querySensorDevIdx(mSensorID);
}

MVOID P2PlatInfoImp::initActiveArrayRect() {
  mActiveArrayRect = MRect(1600, 1200);
  std::shared_ptr<IMetadataProvider> metaProvider =
      NSCam::NSMetadataProviderManager::valueFor(mSensorID);

  if (metaProvider == NULL) {
    MY_LOGE("get NSMetadataProvider failed, use (1600,1200)");
    return;
  }

  IMetadata meta = metaProvider->getMtkStaticCharacteristics();
  if (!tryGet<MRect>(meta, MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION,
                     &mActiveArrayRect)) {
    MY_LOGE("MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION failed, use (1600,1200)");
    return;
  }

  MY_LOGD("Sensor(%d) Active array(%d,%d)(%dx%d)", mSensorID,
          mActiveArrayRect.p.x, mActiveArrayRect.p.y, mActiveArrayRect.s.w,
          mActiveArrayRect.s.h);
}

template <unsigned ID>
const P2PlatInfo* getPlatInfo_() {
  static P2PlatInfoImp* sPlatInfo = new P2PlatInfoImp(ID);
  return sPlatInfo;
}

const P2PlatInfo* P2PlatInfo::getInstance(MUINT32 sensorID) {
  switch (sensorID) {
    case 0:
      return getPlatInfo_<0>();
    case 1:
      return getPlatInfo_<1>();
    case 2:
      return getPlatInfo_<2>();
    case 3:
      return getPlatInfo_<3>();
    case 4:
      return getPlatInfo_<4>();
    case 5:
      return getPlatInfo_<5>();
    case 6:
      return getPlatInfo_<6>();
    case 7:
      return getPlatInfo_<7>();
    case 8:
      return getPlatInfo_<8>();
    case 9:
      return getPlatInfo_<9>();
    default:
      MY_LOGE("invalid sensorID=%d", sensorID);
      return NULL;
  }
}

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam
