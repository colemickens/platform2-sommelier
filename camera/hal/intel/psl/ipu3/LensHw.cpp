/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#define LOG_TAG "LensHw"

#include <string>
#include "LensHw.h"
#include "cros-camera/v4l2_device.h"
#include "MediaController.h"
#include "MediaEntity.h"
#include "LogHelper.h"
#include "Camera3GFXFormat.h"
#include "IPU3CameraCapInfo.h"
#include "Utils.h"

namespace cros {
namespace intel {

LensHw::LensHw(int cameraId, std::shared_ptr<MediaController> mediaCtl):
    mCameraId(cameraId),
    mMediaCtl(mediaCtl),
    mLastLensPosition(-1),
    mCurrentOisState(false),
    mLensMovementStartTime(0)
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(mLensInput);
}

LensHw::~LensHw() {
    LOG1("@%s", __FUNCTION__);
}

status_t LensHw::init()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    std::string entityName;
    std::shared_ptr<MediaEntity> mediaEntity = nullptr;

    const IPU3CameraCapInfo * cap = getIPU3CameraCapInfo(mCameraId);
    entityName = cap->getMediaCtlEntityName("lens");
    if (entityName == "none") {
        LOGE("%s: No lens found", __FUNCTION__);
        return UNKNOWN_ERROR;
    } else {
        status = mMediaCtl->getMediaEntity(mediaEntity, entityName.c_str());
        if (mediaEntity == nullptr || status != NO_ERROR) {
            LOGE("%s, Could not retrieve Media Entity %s", __FUNCTION__, entityName.c_str());
            return UNKNOWN_ERROR;
        }
        status = setLens(mediaEntity);
        if (status != NO_ERROR) {
            LOGE("%s: Cannot set lens subDev", __FUNCTION__);
            return status;
        }
    }

    return NO_ERROR;
}

status_t LensHw::setLens(std::shared_ptr<MediaEntity> entity)
{
    LOG1("@%s", __FUNCTION__);

    if (entity->getType() != SUBDEV_LENS) {
        LOGE("%s is not sensor subdevice", entity->getName());
        return BAD_VALUE;
    }
    std::shared_ptr<cros::V4L2Subdevice> lens;
    status_t status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&) lens);
    CheckError(status != NO_ERROR, status, "entity->getDevice fails, status:%d", status);

    mLensSubdev = lens;

    return NO_ERROR;
}

int LensHw::moveFocusToPosition(int position)   // focus with absolute value
{
    LOG2("@%s: %d", __FUNCTION__, position);

    if (position == mLastLensPosition) return NO_ERROR;

    status_t status = mLensSubdev->SetControl(V4L2_CID_FOCUS_ABSOLUTE, position);
    CheckError(status != NO_ERROR, status, "failed to set focus position %d", position);

    mLastLensPosition = position;
    // we use same clock as the timestamps from the buffers where
    // the statistics are provided. This is a monotonic clock in microsenconds
    mLensMovementStartTime = systemTime()/1000;

    return NO_ERROR;
}

int LensHw::moveFocusToBySteps(int steps)      // focus with relative value
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_FOCUS_RELATIVE, steps);
}

int LensHw::getFocusPosition(int &position)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->GetControl(V4L2_CID_FOCUS_ABSOLUTE, &position);
}

int LensHw::getFocusStatus(int &status)
{
    LOG2("@%s", __FUNCTION__);
    // TODO make a v4l2 implementation
    UNUSED(status);
    return NO_ERROR;
}

int LensHw::startAutoFocus(void)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_AUTO_FOCUS_START, 1);
}

int LensHw::stopAutoFocus(void)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_AUTO_FOCUS_STOP, 0);
}

int LensHw::getAutoFocusStatus(int &status)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->GetControl(V4L2_CID_AUTO_FOCUS_STATUS,
                                    reinterpret_cast<int*>(&status));
}

int LensHw::setAutoFocusRange(int value)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_AUTO_FOCUS_RANGE, value);
}

int LensHw::getAutoFocusRange(int &value)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->GetControl(V4L2_CID_AUTO_FOCUS_RANGE, &value);
}

// ZOOM
int LensHw::moveZoomToPosition(int position)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_ZOOM_ABSOLUTE, position);
}

int LensHw::moveZoomToBySteps(int steps)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_ZOOM_RELATIVE, steps);
}

int LensHw::getZoomPosition(int &position)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->GetControl(V4L2_CID_ZOOM_ABSOLUTE, &position);
}

int LensHw::moveZoomContinuous(int position)
{
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->SetControl(V4L2_CID_ZOOM_CONTINUOUS, position);
}

int LensHw::getCurrentCameraId(void)
{
    LOG1("@%s, id: %d", __FUNCTION__, mCameraId);
    return mCameraId;
}

const char* LensHw::getLensName(void)
{
    return mLensInput.name;
}

int LensHw::enableOis(bool enable)
{
    // Avoid enabling OIS for every frame
    if (enable == mCurrentOisState)
        return OK;

    if (CC_UNLIKELY(mLensSubdev == nullptr)) {
        LOGE("No lens subdev attached");
        return UNKNOWN_ERROR;
    }

    LOG1("@%s  %s", __FUNCTION__, enable ? "ON" : "OFF");
    mLensSubdev->SetControl(V4L2_CID_IMAGE_STABILIZATION, enable ? 1 : 0);
    // ignore error to avoid constant update of the control.
    mCurrentOisState = enable;
    return OK;
}

/**
 * getLatestPosition
 *
 * returns the latest position commanded to the lens actuator and when this
 * was issued.
 * This method does not query the driver.
 *
 * \param: lensPosition[OUT]: lens position last applied
 * \param: time[OUT]: time in micro seconds when the lens move command was sent.
 */
status_t
LensHw::getLatestPosition(int *lensPosition, long long unsigned int *time)
{
    if (CC_LIKELY(lensPosition != nullptr)) {
        *lensPosition = mLastLensPosition;
    }

    if (CC_LIKELY(time != nullptr)) {
        *time = mLensMovementStartTime;
    }
    return OK;
}

}   // namespace intel
}   // namespace cros
