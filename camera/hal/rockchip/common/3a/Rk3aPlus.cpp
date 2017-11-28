/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "Rk3aPlus"

#include <utils/Errors.h>
#include <math.h>
#include <limits.h> // SCHAR_MAX, SCHAR_MIN, INT_MAX
#include <sys/stat.h>

#include "PlatformData.h"
#include "CameraMetadataHelper.h"
#include "LogHelper.h"
#include "Utils.h"
#include "Rk3aPlus.h"

NAMESPACE_DECLARATION {
#define RK_3A_TUNING_FILE_PATH  "/etc/camera/rkisp1/"

Rk3aPlus::Rk3aPlus(int camId):
        Rk3aCore(camId),
        mCameraId(camId),
        mMinFocusDistance(0.0f),
        mMinAeCompensation(0),
        mMaxAeCompensation(0),
        mMinSensitivity(0),
        mMaxSensitivity(0),
        mMinExposureTime(0),
        mMaxExposureTime(0),
        mMaxFrameDuration(0)
{
    LOG1("@%s", __FUNCTION__);
}


status_t Rk3aPlus::initAIQ(const char* sensorName)
{
    status_t status = OK;
    /* get AIQ xml path */
    const CameraCapInfo* cap = PlatformData::getCameraCapInfo(mCameraId);
    std::string iq_file = cap->getIqTuningFile();
    std::string iq_file_path(RK_3A_TUNING_FILE_PATH);
    std::string iq_file_full_path = iq_file_path + iq_file;
    struct stat fileInfo;

    CLEAR(fileInfo);
    if (stat(iq_file_full_path .c_str(), &fileInfo) < 0) {
        if (errno == ENOENT) {
            LOGI("sensor tuning file missing: \"%s\"!", sensorName);
            return NAME_NOT_FOUND;
        } else {
            LOGE("ERROR querying sensor tuning filestat for \"%s\": %s!",
                 iq_file_full_path.c_str(), strerror(errno));
            return UNKNOWN_ERROR;
        }
    }

    status = init(iq_file_full_path.c_str());

    if (status == OK) {
        //TODO: do other init
    }

    return status;
}

rk_aiq_frame_use
Rk3aPlus::getFrameUseFromIntent(const CameraMetadata * settings)
{
    camera_metadata_ro_entry entry;
    rk_aiq_frame_use frameUse = rk_aiq_frame_use_preview;
    //# METADATA_Control control.captureIntent done
    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count == 1) {
        uint8_t captureIntent = entry.data.u8[0];
        switch (captureIntent) {
            case ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM:
                frameUse = rk_aiq_frame_use_preview;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW:
                frameUse = rk_aiq_frame_use_preview;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE:
                frameUse = rk_aiq_frame_use_still;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
                frameUse = rk_aiq_frame_use_video;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
                frameUse = rk_aiq_frame_use_video;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
                frameUse = rk_aiq_frame_use_continuous;
                break;
            case ANDROID_CONTROL_CAPTURE_INTENT_MANUAL:
                frameUse = rk_aiq_frame_use_still;
                break;
            default:
                LOGE("ERROR @%s: Unknow frame use %d", __FUNCTION__, captureIntent);
                break;
        }
    }
    return frameUse;
}

} NAMESPACE_DECLARATION_END

