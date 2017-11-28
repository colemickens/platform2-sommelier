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

#ifndef RKISP1_LENSHW_H_
#define RKISP1_LENSHW_H_

#include "ICameraRKISP1HwControls.h"
#include "PlatformData.h"
#include "v4l2device.h"

namespace android {
namespace camera2 {

class MediaController;
class MediaEntity;

/**
 * \class LensHw
 * This class adds the methods that are needed
 * to drive the camera lens using v4l2 commands and custom ioctl.
 *
 */
class LensHw : public IRKISP1HWLensControl {
public:
    LensHw(int cameraId, std::shared_ptr<MediaController> mediaCtl);
    ~LensHw();

    status_t init();
    status_t setLens(std::shared_ptr<MediaEntity> entity);

    virtual const char* getLensName(void);
    virtual int getCurrentCameraId(void);

    // FOCUS
    virtual int moveFocusToPosition(int position);
    virtual int moveFocusToBySteps(int steps);
    virtual int getFocusPosition(int &position);
    virtual int getFocusStatus(int &status);
    virtual int startAutoFocus(void);
    virtual int stopAutoFocus(void);
    virtual int getAutoFocusStatus(int &status);
    virtual int setAutoFocusRange(int value);
    virtual int getAutoFocusRange(int &value);
    virtual int enableOis(bool enable);

    // ZOOM
    virtual int moveZoomToPosition(int position);
    virtual int moveZoomToBySteps(int steps);
    virtual int getZoomPosition(int &position);
    virtual int moveZoomContinuous(int position);
    /* [END] IHWLensControl overloads, */

    status_t getLatestPosition(int *position, long long unsigned int *time);

private:
    static const int MAX_LENS_NAME_LENGTH = 32;

    struct lensInfo {
        uint32_t index;      //!< V4L2 index
        char name[MAX_LENS_NAME_LENGTH];
    };


private:
    int mCameraId;
    std::shared_ptr<MediaController> mMediaCtl;
    std::shared_ptr<V4L2Subdevice> mLensSubdev;
    struct lensInfo mLensInput;
    int mLastLensPosition;
    bool mCurrentOisState;
    long long unsigned int mLensMovementStartTime; /*!< In useconds */
};  // class LensHW

} // namespace camera2
} // namespace android

#endif  // RKISP1_LENSHW_H__
