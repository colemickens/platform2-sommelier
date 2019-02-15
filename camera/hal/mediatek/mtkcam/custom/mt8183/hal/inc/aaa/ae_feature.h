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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_AAA_AE_FEATURE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_AAA_AE_FEATURE_H_

// AE mode definition
typedef enum {
  LIB3A_AE_MODE_UNSUPPORTED = -1,
  LIB3A_AE_MODE_OFF = 0,
  LIB3A_AE_MODE_ON = 1,
  LIB3A_AE_MODE_ON_AUTO_FLASH = 2,
  LIB3A_AE_MODE_ON_ALWAYS_FLASH = 3,
  LIB3A_AE_MODE_ON_AUTO_FLASH_REDEYE = 4,
  LIB3A_AE_MODE_MAX
} LIB3A_AE_MODE_T;

// Cam mode definition
typedef enum {
  LIB3A_AECAM_MODE_UNSUPPORTED = -1,
  LIB3A_AECAM_MODE_PHOTO = 0,
  LIB3A_AECAM_MODE_VIDEO = 1,
  LIB3A_AECAM_MODE_ZSD = 2,
  LIB3A_AECAM_MODE_S3D = 3,
  LIB3A_AECAM_MODE_MAX
} LIB3A_AECAM_MODE_T;

// Sensor index definition
typedef enum {
  LIB3A_SENSOR_MODE_UNSUPPORTED = -1,
  LIB3A_SENSOR_MODE_PRVIEW = 0,
  LIB3A_SENSOR_MODE_CAPTURE = 1,
  LIB3A_SENSOR_MODE_VIDEO = 2,
  LIB3A_SENSOR_MODE_VIDEO1 = 3,
  LIB3A_SENSOR_MODE_VIDEO2 = 4,
  LIB3A_SENSOR_MODE_CUSTOM1 = 5,
  LIB3A_SENSOR_MODE_CUSTOM2 = 6,
  LIB3A_SENSOR_MODE_CUSTOM3 = 7,
  LIB3A_SENSOR_MODE_CUSTOM4 = 8,
  LIB3A_SENSOR_MODE_CUSTOM5 = 9,
  LIB3A_SENSOR_MODE_CAPTURE_ZSD = 10,
  LIB3A_SENSOR_MODE_MAX
} LIB3A_SENSOR_MODE_T;

// AE EV compensation
typedef enum {  // enum  for evcompensate
  LIB3A_AE_EV_COMP_UNSUPPORTED = -1,
  LIB3A_AE_EV_COMP_00 = 0,    // Disable EV compenate
  LIB3A_AE_EV_COMP_01 = 1,    // EV compensate 0.1
  LIB3A_AE_EV_COMP_02 = 2,    // EV compensate 0.2
  LIB3A_AE_EV_COMP_03 = 3,    // EV compensate 0.3
  LIB3A_AE_EV_COMP_04 = 4,    // EV compensate 0.4
  LIB3A_AE_EV_COMP_05 = 5,    // EV compensate 0.5
  LIB3A_AE_EV_COMP_06 = 6,    // EV compensate 0.6
  LIB3A_AE_EV_COMP_07 = 7,    // EV compensate 0.7
  LIB3A_AE_EV_COMP_08 = 8,    // EV compensate 0.8
  LIB3A_AE_EV_COMP_09 = 9,    // EV compensate 0.9
  LIB3A_AE_EV_COMP_10 = 10,   // EV compensate 1.0
  LIB3A_AE_EV_COMP_11 = 11,   // EV compensate 1.1
  LIB3A_AE_EV_COMP_12 = 12,   // EV compensate 1.2
  LIB3A_AE_EV_COMP_13 = 13,   // EV compensate 1.3
  LIB3A_AE_EV_COMP_14 = 14,   // EV compensate 1.4
  LIB3A_AE_EV_COMP_15 = 15,   // EV compensate 1.5
  LIB3A_AE_EV_COMP_16 = 16,   // EV compensate 1.6
  LIB3A_AE_EV_COMP_17 = 17,   // EV compensate 1.7
  LIB3A_AE_EV_COMP_18 = 18,   // EV compensate 1.8
  LIB3A_AE_EV_COMP_19 = 19,   // EV compensate 1.9
  LIB3A_AE_EV_COMP_20 = 20,   // EV compensate 2.0
  LIB3A_AE_EV_COMP_21 = 21,   // EV compensate 2.1
  LIB3A_AE_EV_COMP_22 = 22,   // EV compensate 2.2
  LIB3A_AE_EV_COMP_23 = 23,   // EV compensate 2.3
  LIB3A_AE_EV_COMP_24 = 24,   // EV compensate 2.4
  LIB3A_AE_EV_COMP_25 = 25,   // EV compensate 2.5
  LIB3A_AE_EV_COMP_26 = 26,   // EV compensate 2.6
  LIB3A_AE_EV_COMP_27 = 27,   // EV compensate 2.7
  LIB3A_AE_EV_COMP_28 = 28,   // EV compensate 2.8
  LIB3A_AE_EV_COMP_29 = 29,   // EV compensate 2.9
  LIB3A_AE_EV_COMP_30 = 30,   // EV compensate 3.0
  LIB3A_AE_EV_COMP_31 = 31,   // EV compensate 3.1
  LIB3A_AE_EV_COMP_32 = 32,   // EV compensate 3.2
  LIB3A_AE_EV_COMP_33 = 33,   // EV compensate 3.3
  LIB3A_AE_EV_COMP_34 = 34,   // EV compensate 3.4
  LIB3A_AE_EV_COMP_35 = 35,   // EV compensate 3.5
  LIB3A_AE_EV_COMP_36 = 36,   // EV compensate 3.6
  LIB3A_AE_EV_COMP_37 = 37,   // EV compensate 3.7
  LIB3A_AE_EV_COMP_38 = 38,   // EV compensate 3.8
  LIB3A_AE_EV_COMP_39 = 39,   // EV compensate 3.9
  LIB3A_AE_EV_COMP_40 = 40,   // EV compensate 4.0
  LIB3A_AE_EV_COMP_n01 = 41,  // EV compensate -0.1
  LIB3A_AE_EV_COMP_n02 = 42,  // EV compensate -0.2
  LIB3A_AE_EV_COMP_n03 = 43,  // EV compensate -0.3
  LIB3A_AE_EV_COMP_n04 = 44,  // EV compensate -0.4
  LIB3A_AE_EV_COMP_n05 = 45,  // EV compensate -0.5
  LIB3A_AE_EV_COMP_n06 = 46,  // EV compensate -0.6
  LIB3A_AE_EV_COMP_n07 = 47,  // EV compensate -0.7
  LIB3A_AE_EV_COMP_n08 = 48,  // EV compensate -0.8
  LIB3A_AE_EV_COMP_n09 = 49,  // EV compensate -0.9
  LIB3A_AE_EV_COMP_n10 = 50,  // EV compensate -1.0
  LIB3A_AE_EV_COMP_n11 = 51,  // EV compensate -1.1
  LIB3A_AE_EV_COMP_n12 = 52,  // EV compensate -1.2
  LIB3A_AE_EV_COMP_n13 = 53,  // EV compensate -1.3
  LIB3A_AE_EV_COMP_n14 = 54,  // EV compensate -1.4
  LIB3A_AE_EV_COMP_n15 = 55,  // EV compensate -1.5
  LIB3A_AE_EV_COMP_n16 = 56,  // EV compensate -1.6
  LIB3A_AE_EV_COMP_n17 = 57,  // EV compensate -1.7
  LIB3A_AE_EV_COMP_n18 = 58,  // EV compensate -1.8
  LIB3A_AE_EV_COMP_n19 = 59,  // EV compensate -1.9
  LIB3A_AE_EV_COMP_n20 = 60,  // EV compensate -2.0
  LIB3A_AE_EV_COMP_n21 = 61,  // EV compensate -2.1
  LIB3A_AE_EV_COMP_n22 = 62,  // EV compensate -2.2
  LIB3A_AE_EV_COMP_n23 = 63,  // EV compensate -2.3
  LIB3A_AE_EV_COMP_n24 = 64,  // EV compensate -2.4
  LIB3A_AE_EV_COMP_n25 = 65,  // EV compensate -2.5
  LIB3A_AE_EV_COMP_n26 = 66,  // EV compensate -2.6
  LIB3A_AE_EV_COMP_n27 = 67,  // EV compensate -2.7
  LIB3A_AE_EV_COMP_n28 = 68,  // EV compensate -2.8
  LIB3A_AE_EV_COMP_n29 = 69,  // EV compensate -2.9
  LIB3A_AE_EV_COMP_n30 = 70,  // EV compensate -3.0
  LIB3A_AE_EV_COMP_n31 = 71,  // EV compensate -3.1
  LIB3A_AE_EV_COMP_n32 = 72,  // EV compensate -3.2
  LIB3A_AE_EV_COMP_n33 = 73,  // EV compensate -3.3
  LIB3A_AE_EV_COMP_n34 = 74,  // EV compensate -3.4
  LIB3A_AE_EV_COMP_n35 = 75,  // EV compensate -3.5
  LIB3A_AE_EV_COMP_n36 = 76,  // EV compensate -3.6
  LIB3A_AE_EV_COMP_n37 = 77,  // EV compensate -3.7
  LIB3A_AE_EV_COMP_n38 = 78,  // EV compensate -3.8
  LIB3A_AE_EV_COMP_n39 = 79,  // EV compensate -3.9
  LIB3A_AE_EV_COMP_n40 = 80,  // EV compensate -4.0
  LIB3A_AE_EV_COMP_MAX
} LIB3A_AE_EVCOMP_T;

// AE metering mode
typedef enum {  // enum for metering
  LIB3A_AE_METERING_MODE_UNSUPPORTED = -1,
  LIB3A_AE_METERING_MODE_CENTER_WEIGHT,  // CENTER WEIGHTED MODE
  LIB3A_AE_METERING_MODE_SOPT,           // SPOT MODE
  LIB3A_AE_METERING_MODE_AVERAGE,        // AVERAGE MODE
  LIB3A_AE_METERING_MODE_MULTI,          // MULTI MODE
  LIB3A_AE_METERING_MODE_MAX
} LIB3A_AE_METERING_MODE_T;

// AE set flicker mode
typedef enum {
  LIB3A_AE_FLICKER_MODE_UNSUPPORTED = -1,
  LIB3A_AE_FLICKER_MODE_60HZ,
  LIB3A_AE_FLICKER_MODE_50HZ,
  LIB3A_AE_FLICKER_MODE_AUTO,  // No support in MT6516
  LIB3A_AE_FLICKER_MODE_OFF,   // No support in MT6516
  LIB3A_AE_FLICKER_MODE_MAX
} LIB3A_AE_FLICKER_MODE_T;

// AE set frame rate mode   //10base
typedef enum {
  LIB3A_AE_FRAMERATE_MODE_UNSUPPORTED = -1,
  LIB3A_AE_FRAMERATE_MODE_DYNAMIC = 0,
  LIB3A_AE_FRAMERATE_MODE_05FPS = 50,
  LIB3A_AE_FRAMERATE_MODE_15FPS = 150,
  LIB3A_AE_FRAMERATE_MODE_30FPS = 300,
  LIB3A_AE_FRAMERATE_MODE_60FPS = 600,
  LIB3A_AE_FRAMERATE_MODE_90FPS = 900,
  LIB3A_AE_FRAMERATE_MODE_120FPS = 1200,
  LIB3A_AE_FRAMERATE_MODE_240FPS = 2400,
  LIB3A_AE_FRAMERATE_MODE_MAX = LIB3A_AE_FRAMERATE_MODE_240FPS
} LIB3A_AE_FRAMERATE_MODE_T;

// for flicker detection algorithm used only.
typedef enum {
  LIB3A_AE_FLICKER_AUTO_MODE_UNSUPPORTED = -1,
  LIB3A_AE_FLICKER_AUTO_MODE_50HZ,
  LIB3A_AE_FLICKER_AUTO_MODE_60HZ,
  LIB3A_AE_FLICKER_AUTO_MODE_MAX
} LIB3A_AE_FLICKER_AUTO_MODE_T;

// remove later
typedef enum {
  LIB3A_AE_STROBE_MODE_UNSUPPORTED = -1,
  LIB3A_AE_STROBE_MODE_AUTO = 0,
  LIB3A_AE_STROBE_MODE_SLOWSYNC =
      0,  // NOW DO NOT SUPPORT SLOW SYNC, TEMPERALLY THE SAME WITH AUTO
  LIB3A_AE_STROBE_MODE_FORCE_ON = 1,
  LIB3A_AE_STROBE_MODE_FORCE_OFF = 2,
  LIB3A_AE_STROBE_MODE_FORCE_TORCH = 3,
  LIB3A_AE_STROBE_MODE_REDEYE = 4,
  LIB3A_AE_STROBE_MODE_TOTAL_NUM,
  LIB3A_AE_STROBE_MODE_MIN = LIB3A_AE_STROBE_MODE_AUTO,
  LIB3A_AE_STROBE_MODE_MAX = LIB3A_AE_STROBE_MODE_FORCE_OFF
} LIB3A_AE_STROBE_MODE_T;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_AAA_AE_FEATURE_H_
