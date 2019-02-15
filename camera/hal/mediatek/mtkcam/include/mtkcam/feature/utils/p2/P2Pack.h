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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2PACK_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2PACK_H_

#include <map>
#include <memory>

#include <mtkcam/feature/utils/p2/P2PlatInfo.h>
#include <mtkcam/feature/utils/p2/P2Data.h>
#include <mtkcam/def/common.h>

namespace NSCam {
namespace Feature {
namespace P2Util {

class VISIBILITY_PUBLIC P2InfoObj {
 public:
  explicit P2InfoObj(const ILog& log);
  virtual ~P2InfoObj();
  void addSensorInfo(const ILog& log, unsigned sensorID);

  std::shared_ptr<P2InfoObj> clone() const;
  const P2ConfigInfo& getConfigInfo() const;
  const P2SensorInfo& getSensorInfo(unsigned sensorID) const;

 public:
  ILog mLog;
  P2ConfigInfo mConfigInfo;
  std::map<unsigned, P2SensorInfo> mSensorInfoMap;
};

class VISIBILITY_PUBLIC P2Info {
 public:
  P2Info();
  P2Info(const P2Info& info, const ILog& log, unsigned sensorID);
  P2Info(const std::shared_ptr<P2InfoObj>& infoObj,
         const ILog& log,
         unsigned sensorID);
  const P2ConfigInfo& getConfigInfo() const;
  const P2SensorInfo& getSensorInfo() const;
  const P2SensorInfo& getSensorInfo(unsigned sensorID) const;
  const P2PlatInfo* getPlatInfo() const;

 public:
  ILog mLog;

 private:
  std::shared_ptr<P2InfoObj> mInfoObj;
  const P2ConfigInfo* mConfigInfoPtr = &P2ConfigInfo::Dummy;
  const P2SensorInfo* mSensorInfoPtr = &P2SensorInfo::Dummy;
  const P2PlatInfo* mPlatInfoPtr = NULL;
};

class VISIBILITY_PUBLIC P2DataObj {
 public:
  explicit P2DataObj(const ILog& log);
  virtual ~P2DataObj();
  const P2FrameData& getFrameData() const;
  const P2SensorData& getSensorData(unsigned sensorID) const;

 public:
  ILog mLog;
  P2FrameData mFrameData;
  std::map<unsigned, P2SensorData> mSensorDataMap;
};

class P2Data {
 public:
  P2Data();
  P2Data(const P2Data& data, const ILog& log, unsigned sensorID);
  P2Data(const std::shared_ptr<P2DataObj>& dataObj,
         const ILog& log,
         unsigned sensorID);
  const P2FrameData& getFrameData() const;
  const P2SensorData& getSensorData() const;
  const P2SensorData& getSensorData(unsigned sensorID) const;

 public:
  ILog mLog;

 private:
  std::shared_ptr<const P2DataObj> mDataObj;
  const P2FrameData* mFrameDataPtr = &P2FrameData::Dummy;
  const P2SensorData* mSensorDataPtr = &P2SensorData::Dummy;
};

class VISIBILITY_PUBLIC P2Pack {
 public:
  P2Pack();
  P2Pack(const ILog& log,
         const std::shared_ptr<P2InfoObj>& info,
         const std::shared_ptr<P2DataObj>& data);
  P2Pack(const P2Pack& src, const ILog& log, unsigned sensorID);

  P2Pack getP2Pack(const ILog& log, unsigned sensorID) const;

  const P2PlatInfo* getPlatInfo() const;
  const P2ConfigInfo& getConfigInfo() const;
  const P2SensorInfo& getSensorInfo() const;
  const P2SensorInfo& getSensorInfo(unsigned sensorID) const;
  const P2FrameData& getFrameData() const;
  const P2SensorData& getSensorData() const;
  const P2SensorData& getSensorData(unsigned sensorID) const;
  MBOOL isValid() const { return mIsValid; }

 public:
  ILog mLog;

 private:
  bool mIsValid = false;
  P2Info mInfo;
  P2Data mData;
};

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2PACK_H_
