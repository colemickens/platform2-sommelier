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

#define LOG_TAG "FaceEngineLibrary"

#include "FaceEngineLibrary.h"

#include <string.h>

#include "LogHelper.h"
#include "common/UtilityMacros.h"
#include <ia_coordinate.h>

namespace intel {
namespace camera {
using namespace android::camera2;
FaceEngineLibrary::FaceEngineLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

FaceEngineLibrary::~FaceEngineLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

status_t FaceEngineLibrary::init(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(face_engine_init_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    mFDHandle = nullptr;
    mEDHandle = nullptr;
    mMDHandle = nullptr;

    face_engine_init_params* params = static_cast<face_engine_init_params*>(pData);
    mMaxFacesNum = MIN(params->max_face_num, MAX_FACES_DETECTABLE);
    mMode = params->fd_mode;
    LOG2("@%s, mMaxFacesNum:%d, face mode:%d", __FUNCTION__, mMaxFacesNum, mMode);

    pvl_err faceRet = pvl_face_detection_create(nullptr, &mFDHandle);
    pvl_err eyeRet = pvl_eye_detection_create(nullptr, &mEDHandle);
    pvl_err mouthRet = pvl_mouth_detection_create(nullptr, &mMDHandle);
    if (faceRet == pvl_success &&
        eyeRet == pvl_success &&
        mouthRet == pvl_success) {
        return OK;
    }

    LOGE("@%s, faceRet:%d, eyeRet:%d, mouthRet:%d", __FUNCTION__, faceRet, eyeRet, mouthRet);
    return UNKNOWN_ERROR;
}

status_t FaceEngineLibrary::uninit()
{
    LOG1("@%s", __FUNCTION__);

    if (mFDHandle) {
        pvl_face_detection_destroy(mFDHandle);
        mFDHandle = nullptr;
    }

    if (mEDHandle) {
        pvl_eye_detection_destroy(mEDHandle);
        mEDHandle = nullptr;
    }

    if (mMDHandle) {
        pvl_mouth_detection_destroy(mMDHandle);
        mMDHandle = nullptr;
    }

    return OK;
}

void FaceEngineLibrary::convertCoordinate(int faceId, int width, int height, pvl_rect& src, pvl_rect* dst)
{
    LOG1("@%s, face:%d rect, src left:%d, top:%d, right:%d, bottom:%d", __FUNCTION__,
    faceId, src.left, src.top, src.right, src.bottom);
    CheckError((dst == nullptr), VOID_VALUE, "@%s, dst is nullptr", __FUNCTION__);

    const ia_coordinate_system iaCoordinate = {IA_COORDINATE_TOP, IA_COORDINATE_LEFT,
                                               IA_COORDINATE_BOTTOM, IA_COORDINATE_RIGHT};
    const ia_coordinate_system faceCoordinate = {0, 0, height, width};

    ia_coordinate topLeft = ia_coordinate_convert(&faceCoordinate, &iaCoordinate, {src.left, src.top});
    ia_coordinate bottomRight = ia_coordinate_convert(&faceCoordinate, &iaCoordinate, {src.right, src.bottom});

    *dst = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};

    LOG2("@%s, face:%d rect, dst left:%d, top:%d, right:%d, bottom:%d", __FUNCTION__,
    faceId, dst->left, dst->top, dst->right, dst->bottom);
}

status_t FaceEngineLibrary::run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(face_engine_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);
    CheckError(!mFDHandle, UNKNOWN_ERROR, "@%s, mFDHandle is nullptr", __FUNCTION__);

    if (mMode == FD_MODE_OFF) {
        LOG2("@%s, face_detect_mode is FD_MODE_OFF", __FUNCTION__);
        return OK;
    }

    face_engine_run_params* params = static_cast<face_engine_run_params*>(pData);
    pvl_image image;
    bool ret = mIpc.serverUnflattenRun(*params, &image);
    CheckError(!ret, UNKNOWN_ERROR, "@%s, serverUnflattenRun fails", __FUNCTION__);

    FaceEngineResult* results = &params->results;
    int32_t fdRet = pvl_face_detection_run_in_preview(mFDHandle, &image, results->faceResults, mMaxFacesNum);
    results->faceNum = (fdRet > 0) ? fdRet : 0;
    LOG2("@%s, fdRet:%d, detected face number:%d, w:%d, h:@%d",
         __FUNCTION__, fdRet, results->faceNum, params->width, params->height);

    for (int i = 0; i < results->faceNum; i++) {
        LOG2("@%s, face:%d rect, left:%d, top:%d, right:%d, bottom:%d", __FUNCTION__, i,
              results->faceResults[i].rect.left,
              results->faceResults[i].rect.top,
              results->faceResults[i].rect.right,
              results->faceResults[i].rect.bottom);
        LOG2("@%s, confidence:%d, rip_angle:%d, rop_angle:%d, tracking_id:%d", __FUNCTION__,
              results->faceResults[i].confidence,
              results->faceResults[i].rip_angle,
              results->faceResults[i].rop_angle,
              results->faceResults[i].tracking_id);

        if (mMode == FD_MODE_FULL) {
            if (mEDHandle) {
                pvl_err ret = pvl_eye_detection_run(mEDHandle, &image,
                                &results->faceResults[i].rect,
                                results->faceResults[i].rip_angle,
                                &results->eyeResults[i]);

                LOG2("@%s, ret:%d, eye:%d left_eye:(%d, %d) right_eye:(%d, %d) confidence:%d",
                      __FUNCTION__, ret, i,
                      results->eyeResults[i].left_eye.x,
                      results->eyeResults[i].left_eye.y,
                      results->eyeResults[i].right_eye.x,
                      results->eyeResults[i].right_eye.y,
                      results->eyeResults[i].confidence);
            }

            if (mMDHandle) {
                pvl_err ret = pvl_mouth_detection_run(mMDHandle, &image,
                                &results->faceResults[i].rect,
                                results->faceResults[i].rip_angle,
                                &results->mouthResults[i]);

                LOG2("@%s, ret:%d, (%d, %d) confidence %d", __FUNCTION__, ret,
                      results->mouthResults[i].mouth.x,
                      results->mouthResults[i].mouth.y,
                      results->mouthResults[i].confidence);
            }
        }
    }

    for (int i = 0; i < results->faceNum; i++) {
        convertCoordinate(i, params->width, params->height, results->faceResults[i].rect,
                          &results->faceResults[i].rect);
    }

    return OK;
}

} /* namespace camera */
} /* namespace intel */
