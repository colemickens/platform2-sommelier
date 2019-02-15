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

#include <camera_custom_3dnr.h>
#include "isp_tuning/isp_tuning.h"
#include <property_lib.h>  // For property_get().
#include <stdlib.h>        // For atio().

// E.g. 600 means that THRESHOLD_LOW is ISO600.
#define ISO_ENABLE_THRESHOLD_LOW 600

// E.g. 800 means that THRESHOLD_HIGH is ISO600.
#define ISO_ENABLE_THRESHOLD_HIGH 800

// E.g. 130 means thatrRaise max ISO limitation to 130% when 3DNR on.
// When set to 100, 3DNR is noise improvement priority.
// When set to higher than 100, 3DNR is frame rate improvement priority.
#define MAX_ISO_INCREASE_PERCENTAGE 100

// How many frames should 3DNR HW be turned off (for power saving) if it
// stays at inactive state. (Note: inactive state means ISO is lower than
// ISO_ENABLE_THRESHOLD_LOW).
#define HW_POWER_OFF_THRESHOLD 60

// How many frames should 3DNR HW be turned on again if it returns from
// inactive state and stays at active state. (Note: active state means
// ISO is higher than ISO_ENABLE_THRESHOLD_LOW).
#define HW_POWER_REOPEN_DELAY 4

// ISO value must higher then threshold to turn on 3DNR
#define NR3D_OFF_ISO_THRESHOLD 400
#define VHDR_NR3D_OFF_ISO_THRESHOLD 400

// GMV value must lower then threshold to turn on 3DNR
#define NR3D_GMV_THRESHOLD 28

int get_3dnr_iso_enable_threshold_low(void) {
  // Force change ISO Limit.
  unsigned int IsoEnableThresholdLowTemp = 0;
  unsigned int i4TempInputValue =
      ::property_get_int32("vendor.camera.3dnr.lowiso", 0);

  if (i4TempInputValue !=
      0) {  // Raise AE ISO limit to 130%. Parameter meaning: MTRUE: Enable the
            // function. MTRUE: Need to equivalent for orginal BV range. 130:
            // Raise Increase ISO Limit to 130% (increase 30%). 100: it means
            // don't need to increase.
    IsoEnableThresholdLowTemp = i4TempInputValue;
  } else {
    IsoEnableThresholdLowTemp = ISO_ENABLE_THRESHOLD_LOW;
  }

  return IsoEnableThresholdLowTemp;
}

int get_3dnr_iso_enable_threshold_high(void) {
  // Force change ISO Limit.
  unsigned int IsoEnableThresholdHighTemp = 0;
  unsigned int i4TempInputValue =
      ::property_get_int32("vendor.camera.3dnr.highiso", 0);

  if (i4TempInputValue !=
      0) {  // Raise AE ISO limit to 130%. Parameter meaning: MTRUE: Enable the
            // function. MTRUE: Need to equivalent for orginal BV range. 130:
            // Raise Increase ISO Limit to 130% (increase 30%). 100: it means
            // don't need to increase.
    IsoEnableThresholdHighTemp = i4TempInputValue;
  } else {
    IsoEnableThresholdHighTemp = ISO_ENABLE_THRESHOLD_HIGH;
  }

  return IsoEnableThresholdHighTemp;
}

int get_3dnr_max_iso_increase_percentage(void) {
  // Force change ISO Limit.
  unsigned int MaxIsoIncreasePercentageTemp = 0;
  unsigned int i4TempInputValue =
      ::property_get_int32("vendor.camera.3dnr.forceisolimit", 0);

  if (i4TempInputValue !=
      0) {  // Raise AE ISO limit to 130%. Parameter meaning: MTRUE: Enable the
            // function. MTRUE: Need to equivalent for orginal BV range. 130:
            // Raise Increase ISO Limit to 130% (increase 30%). 100: it means
            // don't need to increase.
    MaxIsoIncreasePercentageTemp = i4TempInputValue;
  } else {
    MaxIsoIncreasePercentageTemp = MAX_ISO_INCREASE_PERCENTAGE;
  }

  return MaxIsoIncreasePercentageTemp;
}

int get_3dnr_hw_power_off_threshold(void) {
  return HW_POWER_OFF_THRESHOLD;
}

int get_3dnr_hw_power_reopen_delay(void) {
  return HW_POWER_REOPEN_DELAY;
}

int get_3dnr_gmv_threshold(int force3DNR) {
  int i4GmvThreshold = NR3D_GMV_THRESHOLD;

  if (force3DNR) {
    i4GmvThreshold = ::property_get_int32("vendor.debug.3dnr.gmv.threshold",
                                          NR3D_GMV_THRESHOLD);
  }
  return i4GmvThreshold;
}

MBOOL NR3DCustom::isSupportRSC() {
  MBOOL ret = false;

  if (::property_get_int32("vendor.debug.3dnr.rsc.enable", 0)) {
    ret = true;
  }

  return ret;
}

MBOOL NR3DCustom::isEnabledRSC(MUINT32 mask) {
  MBOOL ret = false;
  return ret;
}

/*******************************************************************************
 * is VHDR case
 ******************************************************************************/
MBOOL is_vhdr_profile(MUINT8 ispProfile) {
  switch (ispProfile) {
    case NSIspTuning::EIspProfile_iHDR_Preview:
    case NSIspTuning::EIspProfile_zHDR_Preview:
    case NSIspTuning::EIspProfile_mHDR_Preview:
    case NSIspTuning::EIspProfile_iHDR_Video:
    case NSIspTuning::EIspProfile_zHDR_Video:
    case NSIspTuning::EIspProfile_mHDR_Video:
    case NSIspTuning::EIspProfile_iHDR_Preview_VSS:
    case NSIspTuning::EIspProfile_zHDR_Preview_VSS:
    case NSIspTuning::EIspProfile_mHDR_Preview_VSS:
    case NSIspTuning::EIspProfile_iHDR_Video_VSS:
    case NSIspTuning::EIspProfile_zHDR_Video_VSS:
    case NSIspTuning::EIspProfile_mHDR_Video_VSS:
    case NSIspTuning::EIspProfile_zHDR_Capture:
    case NSIspTuning::EIspProfile_mHDR_Capture:
    case NSIspTuning::EIspProfile_Auto_iHDR_Preview:
    case NSIspTuning::EIspProfile_Auto_zHDR_Preview:
    case NSIspTuning::EIspProfile_Auto_mHDR_Preview:
    case NSIspTuning::EIspProfile_Auto_iHDR_Video:
    case NSIspTuning::EIspProfile_Auto_zHDR_Video:
    case NSIspTuning::EIspProfile_Auto_mHDR_Video:
    case NSIspTuning::EIspProfile_Auto_iHDR_Preview_VSS:
    case NSIspTuning::EIspProfile_Auto_zHDR_Preview_VSS:
    case NSIspTuning::EIspProfile_Auto_mHDR_Preview_VSS:
    case NSIspTuning::EIspProfile_Auto_iHDR_Video_VSS:
    case NSIspTuning::EIspProfile_Auto_zHDR_Video_VSS:
    case NSIspTuning::EIspProfile_Auto_mHDR_Video_VSS:
    case NSIspTuning::EIspProfile_Auto_zHDR_Capture:
    case NSIspTuning::EIspProfile_Auto_mHDR_Capture:
    case NSIspTuning::EIspProfile_EIS_iHDR_Preview:
    case NSIspTuning::EIspProfile_EIS_zHDR_Preview:
    case NSIspTuning::EIspProfile_EIS_mHDR_Preview:
    case NSIspTuning::EIspProfile_EIS_iHDR_Video:
    case NSIspTuning::EIspProfile_EIS_zHDR_Video:
    case NSIspTuning::EIspProfile_EIS_mHDR_Video:
    case NSIspTuning::EIspProfile_EIS_Auto_iHDR_Preview:
    case NSIspTuning::EIspProfile_EIS_Auto_zHDR_Preview:
    case NSIspTuning::EIspProfile_EIS_Auto_mHDR_Preview:
    case NSIspTuning::EIspProfile_EIS_Auto_iHDR_Video:
    case NSIspTuning::EIspProfile_EIS_Auto_zHDR_Video:
    case NSIspTuning::EIspProfile_EIS_Auto_mHDR_Video:
      // VHDR case
      return true;
    default:
      // not VHDR case
      return false;
  }
}

/*******************************************************************************
 * get 3dnr iso threshold ( default / property force case / feature case)
 ******************************************************************************/
MINT32 NR3DCustom::get_3dnr_off_iso_threshold(MUINT8 ispProfile,
                                              MBOOL useAdbValue) {
  int i4IsoThreshold = 0;

  if (is_vhdr_profile(ispProfile)) {
    // set custom threshold by vhdr scenario
    i4IsoThreshold = VHDR_NR3D_OFF_ISO_THRESHOLD;
  } else {
    // set default 3dnr iso threshold
    i4IsoThreshold = NR3D_OFF_ISO_THRESHOLD;
  }

  if (useAdbValue) {
    // property force set case
    i4IsoThreshold =
        ::property_get_int32("vendor.debug.3dnr.iso.threshold", i4IsoThreshold);
  }

  return i4IsoThreshold;
}
