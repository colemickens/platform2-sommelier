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

#include <mtkcam/feature/utils/p2/P2Data.h>

namespace NSCam {
namespace Feature {
namespace P2Util {

const P2ConfigInfo P2ConfigInfo::Dummy;
const P2SensorInfo P2SensorInfo::Dummy;
const P2FrameData P2FrameData::Dummy;
const P2SensorData P2SensorData::Dummy;

P2ConfigInfo::P2ConfigInfo() {}

P2ConfigInfo::P2ConfigInfo(const ILog& log) : mLog(log) {}

P2SensorInfo::P2SensorInfo() {}

P2SensorInfo::P2SensorInfo(const ILog& log, const MUINT32& id)
    : mLog(log), mSensorID(id), mPlatInfo(P2PlatInfo::getInstance(id)) {
  if (mPlatInfo) {
    mActiveArray = mPlatInfo->getActiveArrayRect();
  }
}

P2FrameData::P2FrameData() {}

P2FrameData::P2FrameData(const ILog& log) : mLog(log) {}

P2SensorData::P2SensorData() {}

P2SensorData::P2SensorData(const ILog& log) : mLog(log) {}

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam
