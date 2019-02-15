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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_CAMMANAGER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_CAMMANAGER_H_
//
#include <condition_variable>
#include <mutex>
#include <vector>
//
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC CamManager {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Operations.
  CamManager()
      : mbRecord(false),
        mbAvailable(true),
        mDeviceInConfig(-1),
        mFrameRate0(0) {
    mvOpenId.clear();
    mvUsingId.clear();
  }

  static CamManager* getInstance();

  void incDevice(int32_t openId);

  void decDevice(int32_t openId);

  int32_t getFirstOpenId() const;

  // should be called before config sensor, must call configUsingDeviceDone()
  // and decUsingDevice() after calling incUsingDevice()
  void incUsingDevice(int32_t openId);

  // should be called after config sensor, must call configUsingDeviceDone()
  // after calling incUsingDevice()
  void configUsingDeviceDone(int32_t openId);

  // should be called after release sensor, must call decUsingDevice() after
  // calling incUsingDevice()
  void decUsingDevice(int32_t openId);

  int32_t getFirstUsingId() const;

  uint32_t getDeviceCount() const;

  void setFrameRate(uint32_t const id, uint32_t const frameRate);

  uint32_t getFrameRate(uint32_t const id) const;

  bool isMultiDevice() const;

  bool setThermalPolicy(const char* policy, bool usage);
  void getStartPreviewLock();
  void releaseStartPreviewLock();

  /**
   *  Set recording mode hint.
   *  The default value is FALSE. This should be called after starting/stopping
   * recod.
   *
   *  param
   *      TRUE if camera start record.
   *      FALSE if camera stop record.
   *
   *  return
   *      N/A
   *
   */
  void setRecordingHint(bool const isRecord);

  /**
   *  Set HW available hint.
   *  The default value is TRUE. This should be called after starting/stopping
   * preview.
   *
   *  param
   *      TRUE if driver support app to open another camera.
   *      FALSE if driver does NOT support app to open another camera.
   *
   *  return
   *      N/A
   *
   */
  void setAvailableHint(bool const isAvailable);

  /**
   *  increase sensor power on count
   *
   *  param
   *      caller: caller name.
   *
   *  return
   *      N/A
   *
   */
  void incSensorCount(const char* caller);

  /**
   *  decrease sensor power on count
   *
   *  param
   *      caller: caller name.
   *
   *  return
   *      N/A
   *
   */
  void decSensorCount(const char* caller);

  /**
   *  get current sensor count
   *
   *  param
   *      N/A
   *
   *  return
   *      N/A
   *
   */
  int32_t getSensorCount() const;

  /**
   *  Get permission to open camera or to start preview.
   *
   *  param
   *      N/A
   *
   *  return
   *      TRUE: app can operate on this camera.
   *      FALSE: app can NOT operate on this camera.
   *
   */
  bool getPermission() const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  mutable std::mutex mLockMtx;
  mutable std::mutex mLockFps;
  mutable std::mutex mLockConfig;
  mutable std::mutex mLockStartPreview;
  std::condition_variable mConfigCond;
  bool mbRecord;
  bool mbAvailable;
  int32_t mDeviceInConfig;
  std::vector<int32_t> mvOpenId;
  std::vector<int32_t> mvUsingId;
  uint32_t mFrameRate0;
  int32_t mSensorPowerCount = 0;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  class UsingDeviceHelper {
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Interfaces.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   public:
    explicit UsingDeviceHelper(int32_t openId)
        : mOpenId(openId), mbConfigDone(false) {
      CamManager::getInstance()->incUsingDevice(mOpenId);
    }

    void onLastStrongRef(const void* /*id*/) {
      if (!mbConfigDone) {
        CamManager::getInstance()->configUsingDeviceDone(mOpenId);
      }
      CamManager::getInstance()->decUsingDevice(mOpenId);
    }

    void configDone() {
      CamManager::getInstance()->configUsingDeviceDone(mOpenId);
      mbConfigDone = true;
    }

    bool isFirstUsingDevice() {
      if (CamManager::getInstance()->getFirstUsingId() == mOpenId) {
        return true;
      }
      return false;
    }

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //  Data Members.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   private:
    int32_t mOpenId;
    bool mbConfigDone;
  };
};
/****************************************************************************
 *
 ****************************************************************************/

};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_CAMMANAGER_H_
