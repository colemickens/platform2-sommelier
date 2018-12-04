/*
 * Copyright (C) 2016-2018 Intel Corporation
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

#define LOG_TAG "MediaCtlHelper"

#include <algorithm>
#include <linux/intel-ipu3.h>
#include "LogHelper.h"
#include "Camera3GFXFormat.h"
#include "MediaCtlHelper.h"
#include "MediaEntity.h"

namespace android {
namespace camera2 {

MediaCtlHelper::MediaCtlHelper(std::shared_ptr<MediaController> mediaCtl,
        IOpenCallBack *openCallBack) :
        mOpenVideoNodeCallBack(openCallBack),
        mMediaCtl(mediaCtl),
        mMediaCtlConfig(nullptr),
        mPipeType(IStreamConfigProvider::MEDIA_TYPE_MAX_COUNT)
{
}

MediaCtlHelper::~MediaCtlHelper()
{
    closeVideoNodes();
    resetLinks();
}

status_t MediaCtlHelper::configure(IStreamConfigProvider &graphConfigMgr,
                                   IStreamConfigProvider::MediaType type)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    LOG1("@%s, media type: %d", __FUNCTION__, type);

    mPipeType = type;

    media_device_info deviceInfo;
    CLEAR(deviceInfo);

    // Handle new common config
    mMediaCtlConfig = graphConfigMgr.getMediaCtlConfig(type);
    CheckError(mMediaCtlConfig == nullptr, UNKNOWN_ERROR, "mMediaCtlConfig is nullptr");

    status_t status = mMediaCtl->getMediaDevInfo(deviceInfo);
    if (status != NO_ERROR) {
        LOGE("Error getting device info");
        return status;
    }

    status = openVideoNodesPerPipe();
    if (status != NO_ERROR) {
        LOGE("Failed to open video nodes (ret = %d)", status);
        return status;
    }

    // setting all the Link necessary for the media controller.
    for (unsigned int i = 0; i < mMediaCtlConfig->mLinkParams.size(); i++) {
        MediaCtlLinkParams pipeLink = mMediaCtlConfig->mLinkParams[i];
        status = mMediaCtl->configureLink(pipeLink);
        if (status != NO_ERROR) {
            LOGE("Cannot set MediaCtl links (ret = %d)", status);
            return status;
        }
        mPrevMediaCtlLinks.push_back(pipeLink);
    }

    // PIPE_MODE must be set before setting formats. Other controls need to be set after formats.
    for (unsigned int i = 0; i < mMediaCtlConfig->mControlParams.size(); i++) {
        MediaCtlControlParams pipeControl = mMediaCtlConfig->mControlParams[i];
        if (pipeControl.controlId == V4L2_CID_INTEL_IPU3_MODE) {
            status = mMediaCtl->setControl(pipeControl.entityName.c_str(),
                    pipeControl.controlId, pipeControl.value,
                    pipeControl.controlName.c_str());
            if (status != NO_ERROR) {
                LOGE("Cannot set PIPE_MODE control (ret = %d)", status);
                return status;
            }
            break;
        }
    }

    // setting all the format necessary for the media controller entities
    for (unsigned int i = 0; i < mMediaCtlConfig->mFormatParams.size(); i++) {
        MediaCtlFormatParams pipeFormat = mMediaCtlConfig->mFormatParams[i];
        pipeFormat.field = 0;
        pipeFormat.stride = widthToStride(pipeFormat.formatCode, pipeFormat.width);

        status = mMediaCtl->setFormat(pipeFormat);
        if (status != NO_ERROR) {
            LOGE("Cannot set MediaCtl format (ret = %d)", status);
            return status;
        }

        // get the capture pipe output format
        std::shared_ptr<MediaEntity> entity = nullptr;
        status = mMediaCtl->getMediaEntity(entity, pipeFormat.entityName.c_str());
        if (status != NO_ERROR) {
            LOGE("Getting MediaEntity \"%s\" failed", pipeFormat.entityName.c_str());
            return status;
        }
        if (entity->getType() == DEVICE_VIDEO) {
            mConfigResults.pixelFormat = pipeFormat.formatCode;
            LOG1("Capture pipe output format: %s",
                    v4l2Fmt2Str(mConfigResults.pixelFormat));
        }
    }


    string imgu_name = "ipu3-imgu";
    if (type == IStreamConfigProvider::IMGU_VIDEO)
        imgu_name += " 0";
    else if (type == IStreamConfigProvider::IMGU_STILL)
        imgu_name += " 1";

    // setting all the selections for imgu
    for (auto& it :mMediaCtlConfig->mSelectionVideoParams) {
        std::shared_ptr<MediaEntity> entity;
        std::shared_ptr<cros::V4L2VideoNode> vNode;

        status = mMediaCtl->getMediaEntity(entity, it.entityName.c_str());
        if (status != NO_ERROR) {
            LOGE("Cannot get media entity (ret = %d)", status);
            return status;
        }
        status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&)vNode);
        if (status != NO_ERROR) {
            LOGE("Cannot get media entity device (ret = %d)", status);
            return status;
        }

        // set selection control to imgu node
        status = mMediaCtl->setSelection(imgu_name.c_str(),
                                       it.select.pad,
                                       it.select.target,
                                       it.select.r.top,
                                       it.select.r.left,
                                       it.select.r.width,
                                       it.select.r.height);
        if (status != NO_ERROR) {
            LOGE("Cannot set MediaCtl format selection %s (ret = %d)", imgu_name.c_str(), status);
            return status;
        }
     }

    // setting all the basic controls necessary for the media controller entities.
    // HFLIP already set earlier, so no need to set it again.
    for (unsigned int i = 0; i < mMediaCtlConfig->mControlParams.size(); i++) {
        MediaCtlControlParams pipeControl = mMediaCtlConfig->mControlParams[i];
        if (pipeControl.controlId != V4L2_CID_HFLIP &&
            pipeControl.controlId != V4L2_CID_INTEL_IPU3_MODE) {
            status = mMediaCtl->setControl(pipeControl.entityName.c_str(),
                    pipeControl.controlId, pipeControl.value,
                    pipeControl.controlName.c_str());
            if (status != NO_ERROR) {
                LOGE("Cannot set MediaCtl control (ret = %d)", status);
                return status;
            }
        }
    }

    return status;
}

std::map<IPU3NodeNames, std::shared_ptr<cros::V4L2VideoNode>> MediaCtlHelper::
        getConfiguredNodesPerName(IStreamConfigProvider::MediaType mediaType)
{
    std::map<IPU3NodeNames, std::shared_ptr<cros::V4L2VideoNode>> configuredNodes;
    CheckError((mediaType != IStreamConfigProvider::IMGU_VIDEO) &&
               (mediaType != IStreamConfigProvider::IMGU_STILL),
               configuredNodes, "Invalid media types: %d", mediaType);

    return mConfiguredNodesPerName;
}

status_t MediaCtlHelper::openVideoNodesPerPipe()
{
    LOG1("@%s, media type: %d", __FUNCTION__, mPipeType);
    status_t status = NO_ERROR;

    // Open video nodes that are listed in the current config
    for (unsigned int i = 0; i < mMediaCtlConfig->mVideoNodes.size(); i++) {
        MediaCtlElement element = mMediaCtlConfig->mVideoNodes[i];
        IPU3NodeNames isysNodeName = (IPU3NodeNames) element.isysNodeName;
        status = openVideoNode(element.name.c_str(), isysNodeName);
        if (status != NO_ERROR) {
            LOGE("Cannot open video node (status = 0x%X)", status);
            return status;
        }
    }

    return status;
}

status_t MediaCtlHelper::openVideoNode(const char *entityName, IPU3NodeNames isysNodeName)
{
    LOG1("@%s: %s, node: %d", __FUNCTION__, entityName, isysNodeName);
    status_t status = NO_ERROR;
    std::shared_ptr<MediaEntity> entity = nullptr;
    std::shared_ptr<cros::V4L2VideoNode> videoNode = nullptr;

    if (entityName != nullptr) {
        status = mMediaCtl->getMediaEntity(entity, entityName);
        if (status != NO_ERROR) {
            LOGE("Getting MediaEntity \"%s\" failed", entityName);
            return status;
        }

        status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&) videoNode);
        if (status != NO_ERROR) {
            LOGE("Error opening device \"%s\"", entityName);
            return status;
        }

        // mConfiguredNodesPerName is sorted from lowest to highest IPU3NodeNames value
        mConfiguredNodes.push_back(videoNode);
        if (mPipeType != IStreamConfigProvider::CIO2)
            mConfiguredNodesPerName.insert(std::make_pair(isysNodeName, videoNode));

        if (mOpenVideoNodeCallBack) {
            status = mOpenVideoNodeCallBack->opened(isysNodeName, videoNode);
        }
    }

    return status;
}

status_t MediaCtlHelper::closeVideoNodes()
{
    LOG1("@%s, media type: %d", __FUNCTION__, mPipeType);
    status_t status = NO_ERROR;

    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        status = mConfiguredNodes[i]->Close();
        if (status != NO_ERROR)
            LOGW("Error in closing video node for video pipe(%zu)", i);
    }

    mConfiguredNodes.clear();
    mConfiguredNodesPerName.clear();

    return NO_ERROR;
}

status_t MediaCtlHelper::resetLinks()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mPrevMediaCtlLinks.empty()) {
        LOG2("%s no links to reset", __FUNCTION__);
        return status;
    }

    for (size_t i = 0; i < mPrevMediaCtlLinks.size(); i++) {
        MediaCtlLinkParams pipeLink = mPrevMediaCtlLinks[i];
        pipeLink.enable = false;
        status = mMediaCtl->configureLink(pipeLink);
        if (status != NO_ERROR) {
            LOGE("Cannot reset MediaCtl link (ret = %d)", status);
            return status;
        }
    }
    mPrevMediaCtlLinks.clear();

    return status;
}

} /* namespace camera2 */
} /* namespace android */
