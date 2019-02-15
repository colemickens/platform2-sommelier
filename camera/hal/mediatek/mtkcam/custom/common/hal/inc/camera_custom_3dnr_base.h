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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_CUSTOM_3DNR_BASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_CUSTOM_3DNR_BASE_H_

#include <cstdint>
#include <math.h>
#include <mtkcam/def/BuiltinTypes.h>
#include <property_lib.h>

enum NR3D_MODE {
  NR3D_MODE_OFF = 0,
  NR3D_MODE_3DNR_10,
  NR3D_MODE_3DNR_20,
  NR3D_MODE_3DNR_30,
  NR3D_MODE_3DNR_40,
};

#define NR3D_MODE_ENABLE_3DNR_10(x) ((x) |= (1 << NR3D_MODE_3DNR_10))
#define NR3D_MODE_IS_3DNR_10_ENABLED(x) \
  ((x & (1 << NR3D_MODE_3DNR_10)) ? true : false)

#define NR3D_MODE_ENABLE_3DNR_20(x) ((x) |= (1 << NR3D_MODE_3DNR_20))
#define NR3D_MODE_IS_3DNR_20_ENABLED(x) \
  ((x & (1 << NR3D_MODE_3DNR_20)) ? true : false)

#define NR3D_MODE_ENABLE_3DNR_30(x) ((x) |= (1 << NR3D_MODE_3DNR_30))
#define NR3D_MODE_IS_3DNR_30_ENABLED(x) \
  ((x & (1 << NR3D_MODE_3DNR_30)) ? true : false)

#define NR3D_MODE_ENABLE_3DNR_40(x) ((x) |= (1 << NR3D_MODE_3DNR_40))
#define NR3D_MODE_IS_3DNR_40_ENABLED(x) \
  ((x & (1 << NR3D_MODE_3DNR_40)) ? true : false)
#define isNR3DUsageMaskEnable(x, mask) ((x) & (mask))

// ISO value must higher then threshold to turn on 3DNR
#define DEFAULT_NR3D_OFF_ISO_THRESHOLD 400

// gmv ConfThreshold
#define NR3D_GMVX_CONF_LOW_THRESHOLD 20
#define NR3D_GMVX_CONF_HIGH_THRESHOLD 30

#define NR3D_GMVY_CONF_LOW_THRESHOLD 20
#define NR3D_GMVY_CONF_HIGH_THRESHOLD 30

#define NR3D_GYRO_CONF_THRESHOLD 200

class NR3DCustomBase {
 public:
  enum USAGE_MASK {
    USAGE_MASK_NONE = 0,
    USAGE_MASK_DUAL_ZOOM = 1 << 0,
    USAGE_MASK_MULTIUSER = 1 << 1,
    USAGE_MASK_HIGHSPEED = 1 << 2,
  };
  struct AdjustmentInput {
    bool force3DNR;
    int confX, confY;
    int gmvX, gmvY;
    bool isGyroValid;
    float gyroX, gyroY, gyroZ;

    // RSC info
    intptr_t pMV;
    intptr_t pBV;  // size is rssoSize
    int rrzoW, rrzoH;
    int rssoW, rssoH;
    unsigned int staGMV;  // gmv value of RSC
    bool isRscValid;

    AdjustmentInput()
        : force3DNR(false),
          confX(0),
          confY(0),
          gmvX(0),
          gmvY(0),
          isGyroValid(false),
          gyroX(0),
          gyroY(0),
          gyroZ(0),
          pMV(NULL),
          pBV(NULL),
          rrzoW(0),
          rrzoH(0),
          rssoW(0),
          rssoH(0),
          staGMV(0),
          isRscValid(false) {}

    void setGmv(int confX, int confY, int gmvX, int gmvY) {
      this->confX = confX;
      this->confY = confY;
      this->gmvX = gmvX;
      this->gmvY = gmvY;
    }

    void setGyro(bool isGyroValid, float gyroX, float gyroY, float gyroZ) {
      this->isGyroValid = isGyroValid;
      this->gyroX = gyroX;
      this->gyroY = gyroY;
      this->gyroZ = gyroZ;
    }

    void setRsc(bool isRscValid,
                intptr_t pMV,
                intptr_t pBV,
                int rrzoW,
                int rrzoH,
                int rssoW,
                int rssoH,
                unsigned int staGMV) {
      this->isRscValid = isRscValid;
      this->pMV = pMV;
      this->pBV = pBV;
      this->rrzoW = rrzoW;
      this->rrzoH = rrzoH;
      this->rssoW = rssoW;
      this->rssoH = rssoH;
      this->staGMV = staGMV;
    }
  };

  struct AdjustmentOutput {
    bool isGmvOverwritten;
    int gmvX, gmvY;

    AdjustmentOutput() : isGmvOverwritten(0), gmvX(0), gmvY(0) {}

    void setGmv(bool isGmvOverwritten, int gmvX, int gmvY) {
      this->isGmvOverwritten = isGmvOverwritten;
      this->gmvX = gmvX;
      this->gmvY = gmvY;
    }
  };

 protected:
  // DO NOT create instance
  NR3DCustomBase() {}

  template <typename _T>
  static inline _T __MAX(const _T& a, const _T& b) {
    return (a < b ? b : a);
  }

 public:
  /* Get 3DNR Mode
   */
  static MUINT32 get3DNRMode(MUINT32 mask) {
    (void)mask;
    return NR3D_MODE_OFF;
  }

  static MINT32 get_3dnr_off_iso_threshold(MUINT8 ispProfile = 0,
                                           MBOOL useAdbValue = 0) {
    return DEFAULT_NR3D_OFF_ISO_THRESHOLD;
  }

  static void adjust_parameters(
      const AdjustmentInput& input,
      AdjustmentOutput* output) {  // state may be NULL
    output->setGmv(false, input.gmvX, input.gmvY);

    int confXThrL = NR3D_GMVX_CONF_LOW_THRESHOLD;
    int confXThrH = NR3D_GMVX_CONF_HIGH_THRESHOLD;
    int confYThrL = NR3D_GMVY_CONF_LOW_THRESHOLD;
    int confYThrH = NR3D_GMVY_CONF_HIGH_THRESHOLD;
    int confGyro = NR3D_GYRO_CONF_THRESHOLD;

    if (input.force3DNR) {
      confXThrL = ::property_get_int32("vendor.debug.nr3d.confXL",
                                       NR3D_GMVX_CONF_LOW_THRESHOLD);
      confXThrH = ::property_get_int32("vendor.debug.nr3d.confXH",
                                       NR3D_GMVX_CONF_HIGH_THRESHOLD);
      confYThrL = ::property_get_int32("vendor.debug.nr3d.confYL",
                                       NR3D_GMVY_CONF_LOW_THRESHOLD);
      confYThrH = ::property_get_int32("vendor.debug.nr3d.confYH",
                                       NR3D_GMVY_CONF_HIGH_THRESHOLD);
      confGyro = ::property_get_int32("vendor.debug.nr3d.confGyro",
                                      NR3D_GYRO_CONF_THRESHOLD);
    }

    if ((input.confX <= confXThrL && input.confY <= confYThrH) ||
        (input.confY <= confYThrL && input.confX <= confXThrH)) {
      output->setGmv(true, 0, 0);
    }
    if (input.isGyroValid) {
      float absX = fabs(input.gyroX);
      float absY = fabs(input.gyroY);
      float absZ = fabs(input.gyroZ);
      float gyromax = __MAX(absX, absY);
      gyromax = __MAX(gyromax, absZ);
      float confGyroF = static_cast<float>(confGyro / 1000.0f);  // 0.2
      if (gyromax < confGyroF) {
        output->setGmv(true, 0, 0);
      }
    }
    if (input.isRscValid) {
      // TODO(MTK) : add calculation
    }
  }
};
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_CUSTOM_3DNR_BASE_H_
