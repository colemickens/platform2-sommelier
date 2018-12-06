/*
 * Copyright (C) 2018-2019 Intel Corporation
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

#define LOG_TAG "FaceEngine"

#include "PlatformData.h"
#include "LogHelper.h"
#include "Intel3aCoordinate.h"
#include "FaceEngine.h"

namespace cros {
namespace intel {

FaceEngine::FaceEngine(int cameraId, unsigned int maxFaceNum,
                           int maxWidth, int maxHeight, face_detection_mode fdMode) :
    mCameraId(cameraId),
    mInitialized(false),
    mMode(fdMode),
    mMaxWidth(maxWidth),
    mMaxHeight(maxHeight),
    mWidth(0),
    mHeight(0),
    mCameraThread("FaceEngine:" + std::to_string(cameraId))
{
    LOG1("@%s, maxFaceNum:%d, fdMode:%d", __FUNCTION__, maxFaceNum, fdMode);
    CLEAR(mResult);

    bool ret = mCameraThread.Start();
    CheckError(!ret, VOID_VALUE, "@%s, Camera thread failed to start", __FUNCTION__);

    ret = mFace.init(maxFaceNum, mMaxWidth, mMaxHeight, mMode);
    CheckError(!ret, VOID_VALUE, "@%s, mFace.init fails", __FUNCTION__);

    mInitialized = true;
}

FaceEngine::~FaceEngine()
{
    LOG1("@%s", __FUNCTION__);

    mFace.uninit();
    mCameraThread.Stop();
}

void FaceEngine::run(const pvl_image& frame)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    mWidth = frame.width;
    mHeight = frame.height;
    mFace.prepareRun(frame);

    base::Callback<bool()> closure = base::Bind(&FaceEngine::handleRun, base::Unretained(this));
    mCameraThread.PostTaskAsync<bool>(FROM_HERE, closure);
}

bool FaceEngine::handleRun()
{
    LOG1("@%s", __FUNCTION__);
    std::lock_guard<std::mutex> l(mLock);

    nsecs_t startTime = systemTime();
    bool ret = mFace.run(&mResult);
    LOG2("@%s: ret:%d, it takes %" PRId64 "ms", __FUNCTION__, ret, (systemTime() - startTime) / 1000000);

    return ret;
}

void FaceEngine::getMaxSupportedResolution(int* maxWidth, int* maxHeight) const
{
    LOG1("@%s, mMaxWidth:%d, mMaxHeight:%d", __FUNCTION__, mMaxWidth, mMaxHeight);
    *maxWidth = mMaxWidth;
    *maxHeight = mMaxHeight;
}

int FaceEngine::getFacesNum(void)
{
    std::lock_guard<std::mutex> l(mLock);
    CheckError(mInitialized == false, 0, "@%s, mInitialized is false", __FUNCTION__);

    LOG1("@%s, mResult.faceNum:%d", __FUNCTION__, mResult.faceNum);
    return mResult.faceNum;
}

int FaceEngine::getResult(FaceEngineResult* result)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(!result, UNKNOWN_ERROR, "@%s, result is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, UNKNOWN_ERROR, "@%s, mInitialized is false", __FUNCTION__);

    std::lock_guard<std::mutex> l(mLock);
    for (int i = 0; i < mResult.faceNum; i++) {
        result->faceResults[i] = mResult.faceResults[i];
    }

    result->faceNum = mResult.faceNum;

    return OK;
}

int FaceEngine::getResult(ia_face_state* faceState)
{
    CheckError(mInitialized == false, UNKNOWN_ERROR, "@%s, mInitialized is false", __FUNCTION__);

    std::lock_guard<std::mutex> l(mLock);
    LOG1("@%s, faceNum:%d", __FUNCTION__, mResult.faceNum);
    CheckError(!faceState, UNKNOWN_ERROR, "@%s, faceState is nullptr", __FUNCTION__);

    faceState->num_faces = mResult.faceNum;

    for (int i = 0; i < mResult.faceNum; i++) {
        faceState->faces[i].face_area.top = mResult.faceResults[i].rect.top;
        faceState->faces[i].face_area.bottom = mResult.faceResults[i].rect.bottom;
        faceState->faces[i].face_area.left = mResult.faceResults[i].rect.left;
        faceState->faces[i].face_area.right = mResult.faceResults[i].rect.right;
        faceState->faces[i].rip_angle = mResult.faceResults[i].rip_angle;
        faceState->faces[i].rop_angle = mResult.faceResults[i].rop_angle;
        faceState->faces[i].tracking_id = mResult.faceResults[i].tracking_id;
        faceState->faces[i].confidence = mResult.faceResults[i].confidence;
        faceState->faces[i].person_id = -1;
        faceState->faces[i].similarity = 0;
        faceState->faces[i].best_ratio = 0;
        faceState->faces[i].face_condition = 0;

        faceState->faces[i].smile_state = 0;
        faceState->faces[i].smile_score = 0;
        faceState->faces[i].mouth.x = mResult.mouthResults[i].mouth.x;
        faceState->faces[i].mouth.y = mResult.mouthResults[i].mouth.y;

        faceState->faces[i].eye_validity = 0;
    }

    return NO_ERROR;
}

int FaceEngine::getResult(CVFaceEngineAbstractResult* result)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(result == nullptr, UNKNOWN_ERROR, "@%s, result is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, UNKNOWN_ERROR, "@%s, mInitialized is false", __FUNCTION__);

    const ia_coordinate_system iaCoord = {IA_COORDINATE_TOP, IA_COORDINATE_LEFT,
                                          IA_COORDINATE_BOTTOM, IA_COORDINATE_RIGHT};

    //construct android coordinate based on active pixel array
    CameraWindow activePixelArray = PlatformData::getActivePixelArray(mCameraId);
    const ia_coordinate_system androidCoord = {0, 0, activePixelArray.height(), activePixelArray.width()};

    ia_coordinate destCoord = {0, 0};
    Intel3aCoordinate coordinate;

    std::lock_guard<std::mutex> l(mLock);
    for (int i = 0; i < mResult.faceNum; i++) {
        if (i == MAX_FACES_DETECTABLE)
            break;

        result->faceScores[i] = mResult.faceResults[i].confidence;
        result->faceIds[i] = mResult.faceResults[i].tracking_id;

        destCoord = coordinate.convert(iaCoord, androidCoord, {mResult.faceResults[i].rect.left, mResult.faceResults[i].rect.top});
        result->faceRect[i * 4] = destCoord.x; //rect.left
        result->faceRect[i * 4 + 1] = destCoord.y; //rect.top

        destCoord = coordinate.convert(iaCoord, androidCoord, {mResult.faceResults[i].rect.right, mResult.faceResults[i].rect.bottom});
        result->faceRect[i * 4 + 2] = destCoord.x; //rect.right
        result->faceRect[i * 4 + 3] = destCoord.y; //rect.bottom

        destCoord = coordinate.convert(iaCoord, androidCoord, {mResult.eyeResults[i].left_eye.x, mResult.eyeResults[i].left_eye.y});
        result->faceLandmarks[i * 6] = destCoord.x; //left_eye.x;
        result->faceLandmarks[i * 6 + 1] = destCoord.y; //left_eye.y;

        destCoord = coordinate.convert(iaCoord, androidCoord, {mResult.eyeResults[i].right_eye.x, mResult.eyeResults[i].right_eye.y});
        result->faceLandmarks[i * 6 + 2] = destCoord.x; //right_eye.x;
        result->faceLandmarks[i * 6 + 3] = destCoord.y; //right_eye.y;

        destCoord = coordinate.convert(iaCoord, androidCoord, {mResult.mouthResults[i].mouth.x, mResult.mouthResults[i].mouth.y});
        result->faceLandmarks[i * 6 + 4] = destCoord.x; //mouth.x;
        result->faceLandmarks[i * 6 + 5] = destCoord.y; //mouth.y;
    }
    result->faceNum = (mResult.faceNum < MAX_FACES_DETECTABLE ? mResult.faceNum : MAX_FACES_DETECTABLE);

    return OK;
}

}  // namespace intel
}  // namespace cros