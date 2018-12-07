/*
 * Copyright (C) 2018 Intel Corporation
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

#define LOG_TAG "CameraOrientationDetector"
#include <dirent.h>
#include <fcntl.h>
#include "CameraOrientationDetector.h"
#include "LogHelper.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <system/camera.h>

namespace android {
namespace camera2 {
CameraOrientationDetector::CameraOrientationDetector(int facing) :
    mFacing(facing),
    mPrepared(false),
    mScaleVal(0.0)
{
}

CameraOrientationDetector::~CameraOrientationDetector()
{
    for (auto& it : mFds) {
        close(it.second);
    }
}

/*
    In my device the cros-ec-accel is in /sys/bus/iio/devices/iio:device2.
    The device mapping may change on every boot.
    Under the above folder, there are in_accel_{x|y|z}_raw which reports
    the raw acceleration readings from the sensor on the three axes.
*/
void CameraOrientationDetector::prepare()
{
    LOG2("@%s", __FUNCTION__);
    const std::string dirPath = "/sys/bus/iio/devices/";
    const std::string filePrefix = "iio:device";
    const std::string gSensorName = "cros-ec-accel";
    const std::string xRaw = "/in_accel_x_raw";
    const std::string yRaw = "/in_accel_y_raw";
    const std::string zRaw = "/in_accel_z_raw";
    std::string scaleName = "/scale";
    std::string gSensorDevPath;

    DIR *dp = opendir(dirPath.c_str());
    CheckError((dp == nullptr), VOID_VALUE, "@%s, open %s failed. err:%s", __FUNCTION__, dirPath.c_str(), strerror(errno));

    struct dirent *dirp = nullptr;
    while ((dirp = readdir(dp)) != nullptr) {
        if ((dirp->d_type == DT_LNK) && (strncmp(dirp->d_name, filePrefix.c_str(), filePrefix.length()) == 0)) {
            std::string name = dirPath + dirp->d_name + "/name";
            int fd = open(name.c_str(), O_RDONLY);
            CheckError((fd < 0), VOID_VALUE, "@%s, open %s failed. err:%s", __FUNCTION__, name.c_str(), strerror(errno));

            char buf[128] = {};
            int len = read(fd, buf, sizeof(buf));
            close(fd);
            if (len == -1) {
                closedir(dp);
                LOGE("@%s, read %s failed. err:%s", __FUNCTION__, name.c_str(), strerror(errno));
                return;
            }
            if (strncmp(buf, gSensorName.c_str(), gSensorName.length()) == 0) {
                gSensorDevPath = dirPath + dirp->d_name;
                LOG2("@%s, gSensorDevPath:%s", __FUNCTION__, gSensorDevPath.c_str());
                break;
            }
        }
    }
    closedir(dp);

    // read out the scale value
    scaleName = gSensorDevPath + scaleName;
    int fd = open(scaleName.c_str(), O_RDONLY);
    CheckError((fd < 0), VOID_VALUE, "@%s, open %s failed. err:%s",
               __FUNCTION__, scaleName.c_str(), strerror(errno));
    char buf[32] = {};
    int len = read(fd, buf, sizeof(buf));
    close(fd);
    CheckError((len == -1), VOID_VALUE, "@%s, read %s failed. err:%s",
               __FUNCTION__, scaleName.c_str(), strerror(errno));
    mScaleVal = atof(buf);
    LOG2("@%s, mScaleVal:%f", __FUNCTION__, mScaleVal);

    // update the files name
    mXFileName = gSensorDevPath + xRaw;
    mYFileName = gSensorDevPath + yRaw;
    mZFileName = gSensorDevPath + zRaw;
    LOG2("@%s, file name, x:%s, y:%s, z:%s", __FUNCTION__, mXFileName.c_str(), mYFileName.c_str(), mZFileName.c_str());

    mVals = {{mXFileName, 0}, {mYFileName, 0}, {mZFileName, 0}};
    for (auto& it : mVals) {
        const std::string& fileName = it.first;
        int fd = open(fileName.c_str(), O_RDONLY);
        CheckError((fd < 0), VOID_VALUE, "@%s, open %s failed. err:%s",
                   __FUNCTION__, fileName.c_str(), strerror(errno));
        mFds[fileName] = fd;
    }

    mPrepared = true;
}

CameraOrientationDetectorAngle CameraOrientationDetector::getOrientation()
{
    LOG2("@%s, mPrepared:%d, x:%s, y:%s, z:%s", __FUNCTION__,
          mPrepared, mXFileName.c_str(), mYFileName.c_str(), mZFileName.c_str());
    if(!mPrepared) {
        return CAM_ORI_DETECTOR_ANGLE_0;
    }

    for (auto& it : mVals) {
        const std::string& fileName = it.first;
        int fd = mFds[fileName];

        off_t pos = lseek(fd, 0, SEEK_SET);
        CheckError((pos == -1), CAM_ORI_DETECTOR_ANGLE_0, "@%s, seek %s to beginning failed. err:%s",
                   __FUNCTION__, fileName.c_str(), strerror(errno));

        char buf[32] = {};
        int len = read(fd, buf, sizeof(buf));
        CheckError((len == -1), CAM_ORI_DETECTOR_ANGLE_0, "@%s, read %s failed. err:%s",
                   __FUNCTION__, fileName.c_str(), strerror(errno));

        it.second = atoi(buf);
    }

/*
    The g-sensor chip (such as, BMI160) on Nocturne reports the readings as 16-bit integer (+-32768).
    By checking the accel. reading on the x and y axes we can tell the device orientation:
    natual orientation: Y =  1G
    clockwise  90     : X = -1G
    clockwise 180     : Y = -1G
    clockwise 270     : X =  1G
    If Z is +-1G then the device is laying flat facing upward or downward.
    In this case we can't tell the correct device orientation
    and we usually assume the natural orientation in this case.
*/
    const int scalingMultiplier = 1000;
    const int GVal = 9.8 * scalingMultiplier; // g is 9.8 m/s^2, 9.8 * scalingMultiplier to avoid float caculation.
    int zVal = mScaleVal * mVals[mZFileName] * scalingMultiplier;
    if (((zVal > 0) && abs(zVal - GVal) <= GVal / 4)
        || ((zVal < 0) && abs(zVal + GVal) <= GVal / 4)) {
        return CAM_ORI_DETECTOR_ANGLE_0;
    }

    int xVal = mScaleVal * mVals[mXFileName] * scalingMultiplier;
    int yVal = mScaleVal * mVals[mYFileName] * scalingMultiplier;
    if (yVal > 0 && abs(yVal - GVal) <= GVal / 2) {
        return CAM_ORI_DETECTOR_ANGLE_0;
    } else if (xVal < 0 && abs(xVal + GVal) <= GVal / 2) {
        return mFacing == CAMERA_FACING_FRONT ? CAM_ORI_DETECTOR_ANGLE_90 : CAM_ORI_DETECTOR_ANGLE_270;
    }  else if (yVal < 0 && abs(yVal + GVal) <= GVal / 2) {
        return CAM_ORI_DETECTOR_ANGLE_180;
    } else if (xVal > 0 && abs(xVal - GVal) <= GVal / 2) {
        return mFacing == CAMERA_FACING_FRONT ? CAM_ORI_DETECTOR_ANGLE_270 : CAM_ORI_DETECTOR_ANGLE_90;
    }

    return CAM_ORI_DETECTOR_ANGLE_0;
}

}
}
