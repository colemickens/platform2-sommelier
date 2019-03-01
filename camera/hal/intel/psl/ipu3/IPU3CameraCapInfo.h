/*
 * Copyright (C) 2014-2019 Intel Corporation
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

#ifndef _CAMERA3_HAL_IPU3CAMERACAPINFO_H_
#define _CAMERA3_HAL_IPU3CAMERACAPINFO_H_

#include <unordered_map>
#include <string>
#include <vector>
#include "PlatformData.h"
#include "MediaCtlPipeConfig.h"

namespace cros {
namespace intel {

class GraphConfigNodes;

class IPU3CameraCapInfo : public CameraCapInfo {
public:
    IPU3CameraCapInfo(SensorType type);
    ~IPU3CameraCapInfo();
    virtual int sensorType(void) const { return mSensorType; }
    bool exposureSyncEnabled(void) const { return mExposureSync; }
    bool digiGainOnSensor(void) const { return mDigiGainOnSensor; }
    bool gainExposureCompEnabled(void) const { return mGainExposureComp; }
    int gainLag(void) const { return mGainLag; }
    int exposureLag(void) const { return mExposureLag; }
    const float* fov(void) const { return mFov; }
    int getCITMaxMargin(void) const { return mCITMaxMargin; }
    bool getSupportIsoMap(void) const { return mSupportIsoMap; }
    const int getMaxNvmDataSize(void) const { return mMaxNvmDataSize; }
    const string getNvmDirectory(void) const { return mNvmDirectory; }
    const char* getSensorName(void) const { return mSensorName.c_str(); }
    const ia_binary_data getNvmData(void) const { return mNvmData; }
    void setNvmData(std::unique_ptr<char[]>& data, unsigned int dataSize);
    bool isNvmDataValid() { return mNvmDataBuf.get() ? true : false; }
    const std::string& getGraphSettingsFile(void) const { return mGraphSettingsFile; }
    const std::string getTestPatternBayerFormat(void) const { return mTestPatternBayerFormat; }
    int getSensorTestPatternMode(int mode) const;

    const std::string getMediaCtlEntityName(std::string type) const;
    const std::vector<std::string> getMediaCtlEntityNames(std::string type) const;
    const std::string getMediaCtlEntityType(std::string name) const;

    int mSensorType;
    int mSensorFlipping;
    bool mExposureSync;
    bool mDigiGainOnSensor;
    bool mGainExposureComp;
    int mGainLag;
    int mExposureLag;
    float mFov[2]; /* mFov[0] is fov horizontal, mFov[1] is fov vertical */
    int mCITMaxMargin;
    bool mSupportIsoMap;
    int mMaxNvmDataSize;
    std::string mNvmDirectory;
    std::string mSensorName;

    std::string mGraphSettingsFile;
    std::string mTestPatternBayerFormat;
    std::unordered_map<int, int> mTestPatternMap; /* key: Android standard test pattern mode
                                                     value: sensor test pattern mode */
    int mAgMultiplier;
    int mAgMaxRatio;   /*
                        * total_gain = analog_gain * mAgMultiplier * digital_gain
                        * the max ratio of analog gain, the suitable discrete
                        * DG = total_gain / mAgMaxRatio / mAgMultiplier
                        */
    int mSMIAm0;
    int mSMIAm1;
    int mSMIAc0;
    int mSMIAc1; /*
                  * SMIA parameters. In the general case, 3A handles SMIA calculations,
                  * but for the case of using discrete DG, 3A only passes the total gain to HAL,
                  * HAL should know SMIA parameters to transfer AG to AG code.
                  */
private:
    friend class CameraProfiles;
    std::vector<MediaCtlElement> mMediaCtlElements;

    ia_binary_data mNvmData;
    std::unique_ptr<char[]> mNvmDataBuf;
};

const IPU3CameraCapInfo * getIPU3CameraCapInfo(int cameraId);

} // namespace intel
} // namespace cros
#endif // _CAMERA3_HAL_IPU3CAMERACAPINFO_H_
