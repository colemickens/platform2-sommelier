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

#include <mtkcam/feature/utils/p2/P2Pack.h>

#include <memory>

namespace NSCam {
namespace Feature {
namespace P2Util {

P2InfoObj::P2InfoObj(const ILog& log) : mLog(log) {}

P2InfoObj::~P2InfoObj() {}

std::shared_ptr<P2InfoObj> P2InfoObj::clone() const {
  std::shared_ptr<P2InfoObj> child = std::make_shared<P2InfoObj>(this->mLog);
  child->mConfigInfo = this->mConfigInfo;
  child->mSensorInfoMap = this->mSensorInfoMap;
  return child;
}

const P2ConfigInfo& P2InfoObj::getConfigInfo() const {
  return mConfigInfo;
}

void P2InfoObj::addSensorInfo(const ILog& log, unsigned sensorID) {
  mConfigInfo.mAllSensorID.push_back(sensorID);
  mSensorInfoMap[sensorID] = P2SensorInfo(log, sensorID);
}

const P2SensorInfo& P2InfoObj::getSensorInfo(unsigned sensorID) const {
  auto it = mSensorInfoMap.find(sensorID);
  return (it != mSensorInfoMap.end()) ? it->second : P2SensorInfo::Dummy;
}

P2Info::P2Info() {}

P2Info::P2Info(const P2Info& info, const ILog& log, unsigned sensorID)
    : mLog(log),
      mInfoObj(info.mInfoObj),
      mConfigInfoPtr(info.mConfigInfoPtr),
      mSensorInfoPtr(&info.getSensorInfo(sensorID)),
      mPlatInfoPtr(P2PlatInfo::getInstance(sensorID)) {}

P2Info::P2Info(const std::shared_ptr<P2InfoObj>& infoObj,
               const ILog& log,
               unsigned sensorID)
    : mLog(log),
      mInfoObj(infoObj),
      mPlatInfoPtr(P2PlatInfo::getInstance(sensorID)) {
  if (mInfoObj != NULL) {
    mConfigInfoPtr = &mInfoObj->getConfigInfo();
    mSensorInfoPtr = &mInfoObj->getSensorInfo(sensorID);
  }
}

const P2ConfigInfo& P2Info::getConfigInfo() const {
  return *mConfigInfoPtr;
}

const P2SensorInfo& P2Info::getSensorInfo() const {
  return *mSensorInfoPtr;
}

const P2SensorInfo& P2Info::getSensorInfo(unsigned sensorID) const {
  return mInfoObj->getSensorInfo(sensorID);
}

const P2PlatInfo* P2Info::getPlatInfo() const {
  return mPlatInfoPtr;
}

P2DataObj::P2DataObj(const ILog& log) : mLog(log) {}

P2DataObj::~P2DataObj() {}

const P2FrameData& P2DataObj::getFrameData() const {
  return mFrameData;
}

const P2SensorData& P2DataObj::getSensorData(unsigned sensorID) const {
  auto it = mSensorDataMap.find(sensorID);
  return (it != mSensorDataMap.end()) ? it->second : P2SensorData::Dummy;
}

P2Data::P2Data() {}

P2Data::P2Data(const P2Data& data, const ILog& log, unsigned sensorID)
    : mLog(log),
      mDataObj(data.mDataObj),
      mFrameDataPtr(data.mFrameDataPtr),
      mSensorDataPtr(&data.getSensorData(sensorID)) {}

P2Data::P2Data(const std::shared_ptr<P2DataObj>& dataObj,
               const ILog& log,
               unsigned sensorID)
    : mLog(log), mDataObj(dataObj) {
  if (mDataObj != NULL) {
    mFrameDataPtr = &mDataObj->getFrameData();
    if (dataObj->mSensorDataMap.count(sensorID) == 0) {
      dataObj->mSensorDataMap[sensorID] = P2SensorData();
    }
    mSensorDataPtr = &mDataObj->getSensorData(sensorID);
  }
}

const P2FrameData& P2Data::getFrameData() const {
  return *mFrameDataPtr;
}

const P2SensorData& P2Data::getSensorData() const {
  return *mSensorDataPtr;
}

const P2SensorData& P2Data::getSensorData(unsigned sensorID) const {
  return mDataObj->getSensorData(sensorID);
}

P2Pack::P2Pack() {}

P2Pack::P2Pack(const ILog& log,
               const std::shared_ptr<P2InfoObj>& info,
               const std::shared_ptr<P2DataObj>& data)
    : mLog(log) {
  if (info != NULL) {
    mIsValid = true;
    unsigned mainID = info->getConfigInfo().mMainSensorID;
    mInfo = P2Info(info, log, mainID);
    mData = P2Data(data, log, mainID);
  }
}

P2Pack::P2Pack(const P2Pack& src, const ILog& log, unsigned sensorID)
    : mLog(log),
      mIsValid(src.mIsValid),
      mInfo(src.mInfo, log, sensorID),
      mData(src.mData, log, sensorID) {}

P2Pack P2Pack::getP2Pack(const ILog& log, unsigned sensorID) const {
  return P2Pack(*this, log, sensorID);
}

const P2PlatInfo* P2Pack::getPlatInfo() const {
  return mInfo.getPlatInfo();
}

const P2ConfigInfo& P2Pack::getConfigInfo() const {
  return mInfo.getConfigInfo();
}

const P2SensorInfo& P2Pack::getSensorInfo() const {
  return mInfo.getSensorInfo();
}

const P2SensorInfo& P2Pack::getSensorInfo(unsigned sensorID) const {
  return mInfo.getSensorInfo(sensorID);
}

const P2FrameData& P2Pack::getFrameData() const {
  return mData.getFrameData();
}

const P2SensorData& P2Pack::getSensorData() const {
  return mData.getSensorData();
}

const P2SensorData& P2Pack::getSensorData(unsigned sensorID) const {
  return mData.getSensorData(sensorID);
}

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam
