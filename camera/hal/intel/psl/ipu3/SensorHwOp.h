/*
 * Copyright (C) 2017 Intel Corporation
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

#ifndef CAMERA3_HAL_SENSORHWOP_H_
#define CAMERA3_HAL_SENSORHWOP_H_

#include "v4l2device.h"
#include "UtilityMacros.h"

namespace android {
namespace camera2 {

// Base class for operation which might be different based on kernel driver
class SensorHwOp {

public:
    SensorHwOp(std::shared_ptr<V4L2Subdevice> pixelArraySubdev);

    virtual ~SensorHwOp();

    virtual int updateMembers();
    virtual int getSensorOutputSize(int &width, int &height, int &code);
    virtual int getPixelRate(int &pixel_rate);
    virtual int getLinkFreq(int &bus_freq);
    virtual int getPixelClock(int64_t &pixel_clock);
    virtual int setExposure(int coarse_exposure, int fine_exposure);
    virtual int getExposure(int &coarse_exposure, int &fine_exposure);
    virtual int getExposureRange(int &exposure_min, int &exposure_max,
                                 int &exposure_step);
    virtual int setGains(int analog_gain, int digital_gain);
    virtual int getGains(int &analog_gain, int &digital_gain);

    virtual status_t setFrameDuration(unsigned int Xsetting,
                                      unsigned int Ysetting);

    virtual status_t getMinimumFrameDuration(unsigned int &Xsetting,
                                      unsigned int &Ysetting);
    virtual int getVBlank(unsigned int &vblank);
    virtual int getHBlank(unsigned int &vhlank);
    virtual int getAperture(int &aperture);
    virtual int updateFrameTimings();
    virtual int setSensorFT(int width, int height);
protected:
    virtual status_t getActivePixelArraySize(int &width, int &height,
                                             int &code);
protected:
    std::shared_ptr<V4L2Subdevice> pPixelArraySubdev;

    int pPixelRate;
    int pHorzBlank;
    int pVertBlank;
    int pCropWidth;
    int pCropHeight;
    int pSensorFTWidth;
    int pSensorFTHeight;

}; //class SensorHwOpBase

} // namespace camera2
} // namespace android
#endif
