/*
 * Copyright (C) 2014-2017 Intel Corporation
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

 #define LOG_TAG "IPU3CameraCapInfo"

 #include "IPU3CameraCapInfo.h"

namespace android {
namespace camera2 {

IPU3CameraCapInfo::IPU3CameraCapInfo(SensorType type):
    mSensorType(type),
    mSensorFlipping(SENSOR_FLIP_OFF),
    mExposureSync(false),
    mDigiGainOnSensor(false),
    mGainExposureComp(false),
    mGainLag(0),
    mExposureLag(0),
    mFrameInitialSkip(0),
    mStatisticsInitialSkip(0),
    mCITMaxMargin(0),
    mSupportIsoMap(false),
    mNvmDirectory(""),
    mSensorName(""),
    mNvmData({nullptr,0})
{
    CLEAR(mFov);
}

IPU3CameraCapInfo::~IPU3CameraCapInfo()
{
    CLEAR(mFov);
    delete mGCMNodes;
}

const IPU3CameraCapInfo *getIPU3CameraCapInfo(int cameraId)
{
    if (cameraId > MAX_CAMERAS) {
        LOGE("ERROR: Invalid camera: %d", cameraId);
        cameraId = 0;
    }
    return (IPU3CameraCapInfo *)(PlatformData::getCameraCapInfo(cameraId));
}

const string IPU3CameraCapInfo::getMediaCtlEntityName(string type) const
{
    LOG1("@%s", __FUNCTION__);
    string name = getMediaCtlEntityNames(type)[0];

    return name;
}

const std::vector<string> IPU3CameraCapInfo::getMediaCtlEntityNames(string type) const
{
    LOG1("@%s", __FUNCTION__);
    string name = string("none");
    std::vector<string> names;

    for (unsigned int numidx = 0; numidx < mMediaCtlElements.size(); numidx++) {
        if (type == mMediaCtlElements[numidx].type) {
            name = mMediaCtlElements[numidx].name;
            LOG2("@%s: found type %s, with name %s", __FUNCTION__, type.c_str(), name.c_str());
            names.push_back(name);
        }
    }

    if (names.size() == 0)
        names.push_back(name);

    return names;
}

const std::string IPU3CameraCapInfo::getMediaCtlEntityType(std::string name) const
{
    LOG1("@%s", __FUNCTION__);
    std::string type("none");

    uint32_t idx;
    for (idx = 0; idx < mMediaCtlElements.size(); idx++) {
        if (name == mMediaCtlElements[idx].name) {
            type = mMediaCtlElements[idx].type;
            LOG2("@%s: found name %s, with type %s", __FUNCTION__,
                                                     name.c_str(),
                                                     type.c_str());
            break;
        }
    }
    return type;
}

} // namespace camera2
} // namespace android
