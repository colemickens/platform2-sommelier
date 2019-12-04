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

#define LOG_TAG "AccelerationDetector"
#include "AccelerationDetector.h"
#include <CommonUtilMacros.h>
#include <dirent.h>
#include <fcntl.h>
#include <mtkcam/utils/std/Log.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <system/camera.h>

namespace cros {
namespace NSCam {
AccelerationDetector::AccelerationDetector()
    : mPrepared(false), mScaleVal(0.0) {}

AccelerationDetector::~AccelerationDetector() {
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
void AccelerationDetector::prepare() {
  CAM_LOGI("@%s", __FUNCTION__);
  const std::string dirPath = "/sys/bus/iio/devices/";
  const std::string filePrefix = "iio:device";
  const std::string gSensorName = "cros-ec-accel";
  const std::string xRaw = "/in_accel_x_raw";
  const std::string yRaw = "/in_accel_y_raw";
  const std::string zRaw = "/in_accel_z_raw";
  std::string scaleName = "/scale";
  std::string gSensorDevPath;
  bool findValidSensor = false;

  DIR* dp = opendir(dirPath.c_str());
  CheckWarning((dp == nullptr), VOID_VALUE, "@%s, open %s failed. err:%s",
               __FUNCTION__, dirPath.c_str(), strerror(errno));

  struct dirent* dirp = nullptr;
  while ((dirp = readdir(dp)) != nullptr) {
    if ((dirp->d_type == DT_LNK) &&
        (strncmp(dirp->d_name, filePrefix.c_str(), filePrefix.length()) == 0)) {
      std::string name = dirPath + dirp->d_name + "/name";
      int fd = open(name.c_str(), O_RDONLY);
      CheckError((fd < 0), VOID_VALUE, "@%s, open %s failed. err:%s",
                 __FUNCTION__, name.c_str(), strerror(errno));

      char buf[128] = {};
      int len = read(fd, buf, sizeof(buf));
      close(fd);
      if (len == -1) {
        closedir(dp);
        CAM_LOGE("@%s, read %s failed. err:%s", __FUNCTION__, name.c_str(),
                 strerror(errno));
        return;
      }
      if (strncmp(buf, gSensorName.c_str(), gSensorName.length()) == 0) {
        gSensorDevPath = dirPath + dirp->d_name;
        findValidSensor = true;
        CAM_LOGI("@%s, gSensorDevPath:%s", __FUNCTION__,
                 gSensorDevPath.c_str());
        break;
      }
    }
  }
  closedir(dp);

  if (!findValidSensor) {
    CAM_LOGE("%s: can't locate valid sensor pat.", __FUNCTION__);
    return;
  }

  // read out the scale value
  scaleName = gSensorDevPath + scaleName;
  int fd = open(scaleName.c_str(), O_RDONLY);
  CheckError((fd < 0), VOID_VALUE, "@%s, open %s failed. err:%s", __FUNCTION__,
             scaleName.c_str(), strerror(errno));
  char buf[32] = {};
  int len = read(fd, buf, sizeof(buf));
  close(fd);
  CheckError((len == -1), VOID_VALUE, "@%s, read %s failed. err:%s",
             __FUNCTION__, scaleName.c_str(), strerror(errno));
  mScaleVal = atof(buf);
  CAM_LOGI("@%s, mScaleVal:%f", __FUNCTION__, mScaleVal);

  // update the files name
  mXFileName = gSensorDevPath + xRaw;
  mYFileName = gSensorDevPath + yRaw;
  mZFileName = gSensorDevPath + zRaw;
  CAM_LOGI("@%s, file name, x:%s, y:%s, z:%s", __FUNCTION__, mXFileName.c_str(),
           mYFileName.c_str(), mZFileName.c_str());

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

bool AccelerationDetector::getAcceleration(float data[3]) {
  CAM_LOGI("@%s, mPrepared:%d, x:%s, y:%s, z:%s", __FUNCTION__, mPrepared,
           mXFileName.c_str(), mYFileName.c_str(), mZFileName.c_str());
  if (!mPrepared) {
    return false;
  }

  for (auto& it : mVals) {
    const std::string& fileName = it.first;
    int fd = mFds[fileName];

    off_t pos = lseek(fd, 0, SEEK_SET);
    CheckError((pos == -1), false, "@%s, seek %s to beginning failed. err:%s",
               __FUNCTION__, fileName.c_str(), strerror(errno));

    char buf[32] = {};
    int len = read(fd, buf, sizeof(buf));
    CheckError((len == -1), false, "@%s, read %s failed. err:%s", __FUNCTION__,
               fileName.c_str(), strerror(errno));

    it.second = atoi(buf);
  }

  data[0] = mScaleVal * mVals[mXFileName];
  data[1] = mScaleVal * mVals[mYFileName];
  data[2] = mScaleVal * mVals[mZFileName];

  return true;
}

}  // namespace NSCam
}  // namespace cros
