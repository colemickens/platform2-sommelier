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
#define LOG_TAG "MediaController"

#include "MediaController.h"
#include "MediaEntity.h"
#include "cros-camera/v4l2_device.h"
#include "LogHelper.h"
#include "SysCall.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "Camera3V4l2Format.h"

namespace android {
namespace camera2 {

MediaController::MediaController(const char *path) :
    mPath(path),
    mFd(-1)
{
    LOG1("@%s", __FUNCTION__);
    mDeviceInfo = {};
    mEntities.clear();
    mEntityDesciptors.clear();
}

MediaController::~MediaController()
{
    LOG1("@%s", __FUNCTION__);
    mEntityDesciptors.clear();
    mEntities.clear();
    close();
}

status_t MediaController::init()
{
    LOG1("@%s %s", __FUNCTION__, mPath.c_str());
    status_t status = NO_ERROR;

    status = open();
    if (status != NO_ERROR) {
        LOGE("Error opening media device");
        return status;
    }

    status = getDeviceInfo();
    if (status != NO_ERROR) {
        LOGE("Error getting media info");
        return status;
    }

    status = findEntities();
    if (status != NO_ERROR) {
        LOGE("Error finding media entities");
        return status;
    }

    return status;
}

status_t MediaController::open()
{
    LOG1("@%s %s", __FUNCTION__, mPath.c_str());
    status_t ret = NO_ERROR;
    struct stat st;

    if (mFd != -1) {
        LOGW("Trying to open a device already open");
        return NO_ERROR;
    }

    if (stat (mPath.c_str(), &st) == -1) {
        LOGE("Error stat media device %s: %s",
             mPath.c_str(), strerror(errno));
        return UNKNOWN_ERROR;
    }

    if (!S_ISCHR (st.st_mode)) {
        LOGE("%s is not a device", mPath.c_str());
        return UNKNOWN_ERROR;
    }

    mFd = SysCall::open(mPath.c_str(), O_RDWR);
    if (mFd < 0) {
        if (mFd == -EPERM) {
            // Return permission denied, to allow skipping this device.
            // Our HAL may not want to really use this device at all.
            ret = PERMISSION_DENIED;
        }
    }

    return ret;
}

status_t MediaController::close()
{
    LOG1("@%s device : %s", __FUNCTION__, mPath.c_str());

    if (mFd == -1) {
        LOGW("Device not opened!");
        return INVALID_OPERATION;
    }

    if (SysCall::close(mFd) < 0) {
        LOGE("Close media device failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    mFd = -1;
    return NO_ERROR;
}

int MediaController::xioctl(int request, void *arg) const
{
    int ret(0);
    if (mFd == -1) {
        LOGE("%s invalid device closed!",__FUNCTION__);
        return INVALID_OPERATION;
    }

    do {
        ret = SysCall::ioctl (mFd, request, arg);
    } while (-1 == ret && EINTR == errno);

    if (ret < 0)
        LOGI ("%s: Request 0x%x failed: %s", __FUNCTION__, request, strerror(errno));

    return ret;
}

status_t MediaController::getDeviceInfo()
{
    LOG1("@%s", __FUNCTION__);
    mDeviceInfo = {};
    int ret = xioctl(MEDIA_IOC_DEVICE_INFO, &mDeviceInfo);
    if (ret < 0) {
        LOGE("Failed to get media device information");
        return UNKNOWN_ERROR;
    }

    LOG1("Media device: %s", mDeviceInfo.driver);
    return NO_ERROR;
}

status_t MediaController::enqueueMediaRequest(uint32_t mediaRequestId) {
    UNUSED(mediaRequestId);
    LOGE("Function not implemented in Kernel");
    return BAD_VALUE;
}

status_t MediaController::findEntities()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    struct media_entity_desc entity;

    // Loop until all media entities are found
    for (int i = 0; ; i++) {
        entity = {};
        status = findMediaEntityById(i | MEDIA_ENT_ID_FLAG_NEXT, entity);
        if (status != NO_ERROR) {
            LOGD("@%s: %d media entities found", __FUNCTION__, i);
            break;
        }
        mEntityDesciptors.insert(std::make_pair(entity.name, entity));
        LOG1("entity name: %s, id: %d, pads: %d, links: %d",
              entity.name, entity.id, entity.pads, entity.links);
    }

    if (mEntityDesciptors.size() > 0)
        status = NO_ERROR;

    return status;
}

status_t MediaController::getEntityNameForId(unsigned int entityId, std::string &entityName) const
{
    LOG1("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;

    if (mEntityDesciptors.empty()) {
        LOGE("No media descriptors");
        return UNKNOWN_ERROR;
    }

    for (const auto &entityDesciptors : mEntityDesciptors) {
        if (entityId == entityDesciptors.second.id) {
            entityName = entityDesciptors.second.name;
            return NO_ERROR;
        }
    }

    return status;
}

/**
 * Function to get the sink names for a given media entity.
 *
 * \param[IN] media entity instance
 * \param[OUT] sink names for the media entity
 */
status_t MediaController::getSinkNamesForEntity(std::shared_ptr<MediaEntity> mediaEntity, std::vector<std::string> &names)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;
    std::vector<media_link_desc> links;

    if (mediaEntity == nullptr) {
        LOGE("mediaEntity instance is null");
        return UNKNOWN_ERROR;
    }

    mediaEntity->getLinkDesc(links);
    if (links.empty()) {
        return NO_ERROR;
    } else {
        for (unsigned int i = 0; i < links.size(); i++) {
            std::string name;
            status = getEntityNameForId(links[0].sink.entity, name);
            if (status != NO_ERROR) {
                LOGE("Error getting name for Id");
                return status;
            }
            names.push_back(name);
        }
    }

    return NO_ERROR;
}

status_t MediaController::getMediaDevInfo(media_device_info &info)
{
    LOG1("@%s", __FUNCTION__);

    if (mFd < 0) {
        LOGE("Media controller isn't initialized");
        return UNKNOWN_ERROR;
    }

    info = mDeviceInfo;

    return NO_ERROR;
}

status_t MediaController::enumLinks(struct media_links_enum &linkInfo)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    ret = xioctl(MEDIA_IOC_ENUM_LINKS, &linkInfo);
    if (ret < 0) {
        LOGE("Enumerating entity links failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

/**
 * Find description for given entity ID
 *
 * Using media controller here to query entity with given index.
 */
status_t MediaController::findMediaEntityById(int index, struct media_entity_desc &mediaEntityDesc)
{
    LOG1("@%s", __FUNCTION__);
    int ret = 0;
    mediaEntityDesc = {};
    mediaEntityDesc.id = index;
    ret = xioctl(MEDIA_IOC_ENUM_ENTITIES, &mediaEntityDesc);
    if (ret < 0) {
        LOG1("Enumerating entities failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t MediaController::setFormat(const MediaCtlFormatParams &formatParams)
{
    LOG1("@%s entity %s pad %d (%dx%d) format(%d)", __FUNCTION__,
                               formatParams.entityName.c_str(), formatParams.pad,
                               formatParams.width, formatParams.height,
                               formatParams.formatCode);
    std::shared_ptr<MediaEntity> entity;
    status_t status = NO_ERROR;
    const char* entityName = formatParams.entityName.c_str();

    status = getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__, entityName);
        return status;
    }
    if (entity->getType() == DEVICE_VIDEO) {
        std::shared_ptr<cros::V4L2VideoNode> node;
        cros::V4L2Format v4l2_fmt;
        v4l2_fmt.SetPixelFormat(formatParams.formatCode);
        v4l2_fmt.SetWidth(formatParams.width);
        v4l2_fmt.SetHeight(formatParams.height);
        v4l2_fmt.SetBytesPerLine(pixelsToBytes(formatParams.formatCode, formatParams.stride), 0);
        v4l2_fmt.SetField(formatParams.field);

        status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&) node);
        if (!node || status != NO_ERROR) {
            LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
            return status;
        }
        status = node->SetFormat(v4l2_fmt);
    } else {
        std::shared_ptr<cros::V4L2Subdevice> subdev;
        status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&) subdev);
        if (!subdev || status != NO_ERROR) {
            LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
            return status;
        }
        struct v4l2_subdev_format format = {};
        format.pad = formatParams.pad;
        format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        format.format.code = formatParams.formatCode;
        format.format.width = formatParams.width;
        format.format.height = formatParams.height;
        format.format.field = formatParams.field;
        status = subdev->SetFormat(format);
    }
    return status;
}

status_t MediaController::setSelection(const char* entityName, int pad, int target, int top, int left, int width, int height)
{
    LOG1("@%s, entity %s, pad:%d, top:%d, left:%d, width:%d, height:%d", __FUNCTION__,
         entityName, pad, top, left, width, height);
    std::shared_ptr<MediaEntity> entity;
    std::shared_ptr<cros::V4L2Subdevice> subdev;

    status_t status = getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__, entityName);
        return status;
    }
    status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&) subdev);
    if (!subdev || status != NO_ERROR) {
        LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
        return status;
    }
    struct v4l2_subdev_selection selection = {};
    selection.pad = pad;
    selection.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    selection.target = target;
    selection.flags = 0;
    selection.r.top = top;
    selection.r.left = left;
    selection.r.width = width;
    selection.r.height = height;
    return subdev->SetSelection(selection);
}

status_t MediaController::setControl(const char* entityName, int controlId, int value, const char *controlName)
{
    LOG1("@%s entity %s ctrl ID %d value %d name %s", __FUNCTION__,
                                                         entityName, controlId,
                                                         value,controlName);
    std::shared_ptr<MediaEntity> entity = nullptr;
    std::shared_ptr<cros::V4L2Subdevice> subdev = nullptr;

    status_t status = getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__, entityName);
        return status;
    }
    status = entity->getDevice((std::shared_ptr<cros::V4L2Device>&) subdev);
    if (!subdev || status != NO_ERROR) {
        LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
        return status;
    }
    return subdev->SetControl(controlId, value);
}

/**
 * Enable or disable link between two given media entities
 */
status_t MediaController::configureLink(const MediaCtlLinkParams &linkParams)
{
    LOG1(" @%s: %s \"%s\" [%d] --> \"%s\" [%d]", __FUNCTION__,
         linkParams.enable?"enable":"disable",
         linkParams.srcName.c_str(), linkParams.srcPad,
         linkParams.sinkName.c_str(), linkParams.sinkPad);
    status_t status = NO_ERROR;

    std::shared_ptr<MediaEntity> srcEntity, sinkEntity;
    struct media_link_desc linkDesc;
    struct media_pad_desc srcPadDesc, sinkPadDesc;

    status = getMediaEntity(srcEntity, linkParams.srcName.c_str());
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed",
             __FUNCTION__, linkParams.srcName.c_str());
        return status;
    }

    status = getMediaEntity(sinkEntity, linkParams.sinkName.c_str());
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed",
             __FUNCTION__, linkParams.sinkName.c_str());
        return status;
    }
    linkDesc = {};
    srcPadDesc = {};
    sinkPadDesc = {};
    srcEntity->getPadDesc(srcPadDesc, linkParams.srcPad);
    sinkEntity->getPadDesc(sinkPadDesc, linkParams.sinkPad);

    linkDesc.source = srcPadDesc;
    linkDesc.sink = sinkPadDesc;

    if (linkParams.enable) {
        linkDesc.flags |= linkParams.flags;
    } else if (linkParams.flags & MEDIA_LNK_FL_DYNAMIC) {
        linkDesc.flags |= MEDIA_LNK_FL_DYNAMIC;
        linkDesc.flags &= ~MEDIA_LNK_FL_ENABLED;
    } else {
        linkDesc.flags &= ~MEDIA_LNK_FL_ENABLED;
    }

    status = setupLink(linkDesc);

    // refresh source entity links
    if (status == NO_ERROR) {
        struct media_entity_desc entityDesc;
        struct media_links_enum linksEnum;
        entityDesc = {};
        linksEnum = {};
        struct media_link_desc *links = nullptr;

        srcEntity->getEntityDesc(entityDesc);
        links = new struct media_link_desc[entityDesc.links];
        memset(links, 0, sizeof(struct media_link_desc) * entityDesc.links);

        linksEnum.entity = entityDesc.id;
        linksEnum.pads = nullptr;
        linksEnum.links = links;
        status = enumLinks(linksEnum);
        if (status == NO_ERROR) {
            srcEntity->updateLinks(linksEnum.links);
        }
        delete[] links;
    }

    return status;
}

status_t MediaController::setupLink(struct media_link_desc &linkDesc)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    int ret = xioctl(MEDIA_IOC_SETUP_LINK, &linkDesc);
    if (ret < 0) {
        LOGE("Link setup failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return status;
}

/**
 * Resets (disables) all links between entities
 */
status_t MediaController::resetLinks()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    for (const auto &entityDesciptors : mEntityDesciptors) {
        struct media_entity_desc entityDesc;
        struct media_links_enum linksEnum;
        entityDesc = {};
        linksEnum = {};
        struct media_link_desc *links = nullptr;

        entityDesc = entityDesciptors.second;
        links = new struct media_link_desc[entityDesc.links];
        memset(links, 0, sizeof(media_link_desc) * entityDesc.links);
        LOG1("@%s entity id: %d", __FUNCTION__, entityDesc.id);

        linksEnum.entity = entityDesc.id;
        linksEnum.pads = nullptr;
        linksEnum.links = links;
        status = enumLinks(linksEnum);
        if (status != NO_ERROR) {
            delete[] links;
            break;
        }
        // Disable all links, except the immutable ones
        for (int j = 0; j < entityDesc.links; j++) {
            if (links[j].flags & MEDIA_LNK_FL_IMMUTABLE)
                continue;

            links[j].flags &= ~MEDIA_LNK_FL_ENABLED;
            setupLink(links[j]);
        }
        delete[] links;
    }

    return status;
}

status_t MediaController::getMediaEntity(std::shared_ptr<MediaEntity> &entity, const char* name)
{
    LOG1("@%s, name:%s", __FUNCTION__, name);
    status_t status = NO_ERROR;

    std::string entityName(name);
    struct media_entity_desc entityDesc;
    struct media_links_enum linksEnum;
    entityDesc = {};
    linksEnum = {};
    struct media_link_desc *links = nullptr;
    struct media_pad_desc *pads = nullptr;

    // check whether the MediaEntity object has already been created
    std::map<std::string, std::shared_ptr<MediaEntity>>::iterator itEntities =
                                     mEntities.find(entityName);
    std::map<std::string, struct media_entity_desc>::iterator itEntityDesc =
                                     mEntityDesciptors.find(entityName);
    if (itEntities != mEntities.end()) {
        entity = itEntities->second;
    } else if (itEntityDesc != mEntityDesciptors.end()) {
        // MediaEntity object not yet created, so create it
        entityDesc = itEntityDesc->second;
        if (entityDesc.links > 0) {
            links = new struct media_link_desc[entityDesc.links];
            memset(links, 0, sizeof(media_link_desc) * entityDesc.links);
        }

        if (entityDesc.pads > 0) {
            pads = new struct media_pad_desc[entityDesc.pads];
            memset(pads, 0, sizeof(media_pad_desc) * entityDesc.pads);
        }

        LOG1("Creating entity - name: %s, id: %d, links: %d, pads: %d",
             entityDesc.name, entityDesc.id, entityDesc.links, entityDesc.pads);

        linksEnum.entity = entityDesc.id;
        linksEnum.pads = pads;
        linksEnum.links = links;
        status = enumLinks(linksEnum);

        if (status == NO_ERROR) {
            entity = std::make_shared<MediaEntity>(entityDesc, links, pads);
            mEntities.insert(std::make_pair(entityDesc.name, entity));
        }
    } else {
        status = UNKNOWN_ERROR;
    }

    delete[] links;
    delete[] pads;

    return status;
}

} /* namespace camera2 */
} /* namespace android */
