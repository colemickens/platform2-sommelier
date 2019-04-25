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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MtkCameraV4L2API"

#include <mtklibv4l2/MtkCameraV4L2API.h>

#include <cutils/compiler.h>  // for CC_LIKELY, CC_UNLIKELY

#include <map>      // std::map
#include <memory>   // std::shared_ptr
#include <string>   // std::string, std::make_shared
#include <utility>  // std::make_pair
#include <vector>   // std::vector

using NSCam::BAD_VALUE;
using NSCam::NAME_NOT_FOUND;
using NSCam::NO_ERROR;
using NSCam::NO_INIT;
using NSCam::OK;
using NSCam::PERMISSION_DENIED;
using NSCam::UNKNOWN_ERROR;

inline std::string FormatToString(int32_t format) {
  return std::string(reinterpret_cast<char*>(&format), 4);
}

#define QUERY_DRIVERSLINKFLAG 1
MtkCameraV4L2API::MtkCameraV4L2API()
    : mMdevCount(0), mIsAutoConfigPipeline(false), mHasTuning(false) {
  LOGD("[%p][%s] ", this, __FUNCTION__);
}

MtkCameraV4L2API::~MtkCameraV4L2API() {
  LOGD("[%p][%s] ", this, __FUNCTION__);
  for (auto it_media_control : mMediaControllers) {
    std::string dev_name;
    auto media_control = it_media_control.second;
    media_control->getDevName(&dev_name);
    LOGW(
        "media controller#%d of %s is not closed yet, "
        "force to reset and close",
        it_media_control.first, dev_name.c_str());
    if (media_control->resetAllLinks() != NO_ERROR)
      LOGW("%s resetAllLinks failed", dev_name.c_str());
    if (media_control->close() != NO_ERROR)
      LOGW("%s close failed", dev_name.c_str());
  }
}

status_t MtkCameraV4L2API::openMediaDevice(const string& modelname,
                                           int* index) {
  LOGD("[%p][%s] media device name %s", this, __FUNCTION__, modelname.c_str());
  status_t status = NO_ERROR;

  string mediaDevicePath = MediaCtrlConfig::getMediaDevicePathByName(modelname);
  auto mediaController = std::make_shared<MtkMediaController>(
      modelname.c_str(), mediaDevicePath.c_str());

  status = mediaController->init();
  if (status != NO_ERROR) {
    LOGE("open media device %s fail", modelname.c_str());
    return status;
  }

  mMdevCount++;
  *index = mMdevCount;
  mMediaControllers.insert(std::make_pair(mMdevCount, mediaController));

  LOGD("media device index is %d", *index);
  return status;
}

status_t MtkCameraV4L2API::getMediaDeviceInfo(int index,
                                              struct media_device_info* info) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  if (info == nullptr) {
    return BAD_VALUE;
  }
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;

  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  status = mediaController->getMediaDevInfo(info);
  if (status != NO_ERROR) {
    LOGE("(mdev %d) get media device info fail", index);
    return status;
  }

  return status;
}

status_t MtkCameraV4L2API::enumEntities(int index,
                                        struct media_entity_desc* entityDesc) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  if (entityDesc == nullptr) {
    return BAD_VALUE;
  }
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;

  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  status = mediaController->enumEntity(entityDesc);
  if (status != NO_ERROR) {
    LOGE("(mdev %d) enumEntity fail", index);
    return status;
  }

  return status;
}

status_t MtkCameraV4L2API::enumLinks(int index,
                                     struct media_links_enum* linksEnum) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  if (linksEnum == nullptr) {
    return BAD_VALUE;
  }
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;

  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  status = mediaController->enumLinks(linksEnum);
  if (status != NO_ERROR) {
    LOGE("(mdev %d) enumLinks fail", index);
    return status;
  }

  return status;
}

status_t MtkCameraV4L2API::setupLink(int index,
                                     struct media_link_desc* linkDesc) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  if (linkDesc == nullptr) {
    return BAD_VALUE;
  }
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;

  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  status = mediaController->setupLink(linkDesc);
  if (status != NO_ERROR) {
    LOGE("@%s: Link setup failed", __FUNCTION__);
    return status;
  }

  return status;
}

int MtkCameraV4L2API::openAndsetupAllLinks(
    MediaDeviceTag mdevtag,
    vector<std::shared_ptr<MediaEntity>>* mediaEntity,
    bool hasTuning) {
  LOGD("[%p][%s] ", this, __FUNCTION__);
  status_t status = NO_ERROR;

  int index = 0;
  status = openMediaDevice(mdevtag, hasTuning, &index);
  if (status != NO_ERROR) {
    return status;
  }
  status = setupAllLinks(index, mediaEntity);
  if (status != NO_ERROR) {
    return status;
  }

  return index;
}

status_t MtkCameraV4L2API::openMediaDevice(MediaDeviceTag mdevtag,
                                           bool hasTuning,
                                           int* index) {
  if (index == nullptr) {
    LOGE("invalid argument: index is nullptr");
    return BAD_VALUE;
  }

  LOGD("[%p][%s] media device Tag is %d", this, __FUNCTION__, mdevtag);

  status_t status = NO_ERROR;

  string drivername = MediaCtrlConfig::getMediaDeviceNameByTag(mdevtag);
  status = openMediaDevice(drivername, index);
  if (status != NO_ERROR) {
    LOGE("open media device %s fail", drivername.c_str());
    return status;
  }

  mMediaDeviceTag.insert(std::make_pair(*index, mdevtag));
  mHasTuning = hasTuning;
  return status;
}

status_t MtkCameraV4L2API::setupAllLinks(int index) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  status_t status = NO_ERROR;
  vector<std::shared_ptr<MediaEntity>> mediaEntity;

  status = setupAllLinks(index, &mediaEntity);
  if (status != NO_ERROR) {
    LOGE("setupAllLinks fail");
    return status;
  }

  return status;
}
status_t MtkCameraV4L2API::setupAllLinks(
    int index, vector<std::shared_ptr<MediaEntity>>* mediaEntity) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  status_t status = NO_ERROR;

  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  // A. Enum All Entities
  struct media_entity_desc entitydesc;
  int entitynum = 0;
  for (int i = 0;; i++) {
    entitydesc = {};
    entitydesc.id = entitynum;
    status = mediaController->enumEntity(&entitydesc);
    if (status != NO_ERROR) {
      LOGD("[%s] No more media entities NOT found with id:%d", __FUNCTION__, i);
      break;
    }
    entitynum = entitydesc.id;
  }

  // B. Enum All Entities Pad/links
  status = mediaController->enumAllLinks();
  if (status != NO_ERROR) {
    LOGE("[%s] mediaController->enumAllLinks fail", __FUNCTION__);
    return status;
  }

  // C. Create mediaCtlConfig
  MediaDeviceTag mdevtag;
  map<int, MediaDeviceTag>::iterator itmdevtag = mMediaDeviceTag.find(index);
  if (itmdevtag == mMediaDeviceTag.end()) {
    LOGE("itmdevtag (idx=%d) not found", index);
    return NAME_NOT_FOUND;
  }
  mdevtag = itmdevtag->second;

  MediaCtlConfig mediaCtlConfig;
  MediaCtrlConfig::CreateMediaCtlGraph(mdevtag, mHasTuning, &mediaCtlConfig);

  // D. Start mdev setup link
  std::shared_ptr<MediaEntity> srcEntity, sinkEntity;
  struct media_link_desc linkDesc = {};
  struct media_link_desc linkDesctemp = {};
  struct media_pad_desc srcPadDesc = {};
  struct media_pad_desc sinkPadDesc = {};
  for (unsigned int i = 0; i < mediaCtlConfig.mLinkParams.size(); i++) {
    MediaCtlLinkParams linkParams = mediaCtlConfig.mLinkParams[i];

    // 1. Get Source/Sink MediaEntity by entityName
    status =
        mediaController->getMediaEntity(&srcEntity, linkParams.srcName.c_str());
    if (status != NO_ERROR) {
      LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__,
           linkParams.srcName.c_str());
      return status;
    }

    status = mediaController->getMediaEntity(&sinkEntity,
                                             linkParams.sinkName.c_str());
    if (status != NO_ERROR) {
      LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__,
           linkParams.sinkName.c_str());
      return status;
    }

    // 2. Get Source/Sink pad info from MediaEntity by pad index
    CLEAR(&srcPadDesc);
    CLEAR(&sinkPadDesc);
    srcEntity->getPadDesc(&srcPadDesc, linkParams.srcPad);
    sinkEntity->getPadDesc(&sinkPadDesc, linkParams.sinkPad);
    CLEAR(&linkDesc);
    linkDesc.source = srcPadDesc;
    linkDesc.sink = sinkPadDesc;
    // 3. maintain flag
    if (QUERY_DRIVERSLINKFLAG) {
      CLEAR(&linkDesctemp);
      mediaController->getLinkDescbyEntityName(&linkDesctemp,
                                               linkParams.srcName.c_str(),
                                               linkParams.sinkName.c_str());
      if ((linkDesctemp.source.entity == srcPadDesc.entity) &&
          (linkDesctemp.source.index == srcPadDesc.index)) {
        // override MediaCtrlConfig flag setting
        LOGD("[QUERY_DRIVERSLINKFLAG] link found . Flags= 0x%8x -> 0x%8x",
             linkParams.flags, linkDesctemp.flags | linkParams.flags);
        linkParams.flags |= linkDesctemp.flags;
      }
    }
    if (linkParams.enable) {
      linkDesc.flags |= linkParams.flags;
    } else if (linkParams.flags & MEDIA_LNK_FL_DYNAMIC) {
      linkDesc.flags |= MEDIA_LNK_FL_DYNAMIC;
      linkDesc.flags &= ~MEDIA_LNK_FL_ENABLED;
    } else {
      linkDesc.flags &= ~MEDIA_LNK_FL_ENABLED;
    }

    status = mediaController->setupLink(&linkDesc);
    if (status != NO_ERROR) {
      LOGE("@%s: Link setup failed", __FUNCTION__);
      // return status;    //TODO
    }
  }
  status = mediaController->storeAllLinks();
  // C. Open all video node
  vector<std::shared_ptr<V4L2Device>> configuredNodes;
  std::shared_ptr<MediaEntity> entity;
  std::shared_ptr<V4L2Device> videoNode;
  for (unsigned int i = 0; i < mediaCtlConfig.mVideoNodes.size(); i++) {
    MediaCtlElement* element = &mediaCtlConfig.mVideoNodes[i];
    status = openVideoNode(mediaController, element->name.c_str(), &entity,
                           &videoNode);
    if (status != NO_ERROR) {
      LOGE("Cannot open video node (status = 0x%X)", status);
      return status;
    }
    mediaEntity->push_back(entity);
    configuredNodes.push_back(videoNode);
  }
  mMdevAutoConfiguredNodes.insert(std::make_pair(mMdevCount, configuredNodes));
  mIsAutoConfigPipeline = true;
  LOGD("[%p][%d][%s] END", this, index, __FUNCTION__);
  return status;
}
status_t MtkCameraV4L2API::disableLink(int index,
                                       DynamicLinkTag tag,
                                       const char* name) {
  LOGD("[%d][%s] Sink name = %s", index, __FUNCTION__, name);
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;
  struct media_link_desc linkDesc = {};
  if (tag == DYNAMIC_LINK_BYVIDEONAME) {
    status = mediaController->getLinkDescbyEntityName(&linkDesc, name);
    if (status != NO_ERROR) {
      LOGE("getLinkDescbyEntityName failed, name=%s, errcode=%d", name, status);
      return status;
    }
    status = mediaController->disableLink(&linkDesc);
    if (status != NO_ERROR) {
      LOGE("disableLink returns fail, errcode=%d", status);
      return status;
    }
  } else if (tag == DYNAMIC_LINK_BYLINKDESC) {
    // To-Do : dynamic disable link for users using media_link_desc

  } else {
    LOGD("[%s] Dynamic setup link tag is not supported , %d", __FUNCTION__,
         tag);
  }
  return status;
}
status_t MtkCameraV4L2API::enableLink(int index,
                                      DynamicLinkTag tag,
                                      const char* name) {
  LOGD("[%d][%s] Sink name = %s", index, __FUNCTION__, name);
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;
  struct media_link_desc linkDesc = {};
  if (tag == DYNAMIC_LINK_BYVIDEONAME) {
    status = mediaController->getLinkDescbyEntityName(&linkDesc, name);
    if (status != NO_ERROR) {
      LOGE("getLinkDescbyEntityName failed, name=%s, errcode=%d", name, status);
      return status;
    }
    status = mediaController->enableLink(&linkDesc);
    if (status != NO_ERROR) {
      LOGE("enableLink returns fail, errcode=%d", status);
      return status;
    }
  } else if (tag == DYNAMIC_LINK_BYLINKDESC) {
    // To-Do : dynamic disable link for users using media_link_desc

  } else {
    LOGD("[%s] Dynamic setup link tag is not supported , %d", __FUNCTION__,
         tag);
  }
  return status;
}

status_t MtkCameraV4L2API::enableAllLinks(int index) {
  LOGD("[%d][%s]", index, __FUNCTION__);
  status_t status = NO_ERROR;
  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;
  status = mediaController->enableAllLinks();
  return status;
}

status_t MtkCameraV4L2API::resetAllLinks(int index) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  status_t status = NO_ERROR;

  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  status = mediaController->resetAllLinks();
  if (status != NO_ERROR) {
    LOGE("@%s: resetAllLinks failed", __FUNCTION__);
  }

  return status;
}

status_t MtkCameraV4L2API::closeMediaDevice(int index) {
  LOGD("[%p][%d][%s] ", this, index, __FUNCTION__);
  status_t status = NO_ERROR;

  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);
  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }
  mediaController = it->second;

  status = mediaController->close();
  if (status != NO_ERROR) {
    LOGE("@%s: close failed", __FUNCTION__);
  } else {
    mMediaControllers.erase(it);
  }

  return status;
}

status_t MtkCameraV4L2API::openVideoNode(
    const std::shared_ptr<MtkMediaController>& mediaController,
    const char* entityName,
    std::shared_ptr<MediaEntity>* entity,
    std::shared_ptr<V4L2Device>* videoNode) {
  LOGD("[%p][%s] entityName %s", this, __FUNCTION__, entityName);
  status_t status = UNKNOWN_ERROR;

  if (mediaController != NULL && entityName != NULL) {
    status = mediaController->getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
      LOGE("Getting MediaEntity \"%s\" failed", entityName);
      return status;
    }

    status = (*entity)->getDevice(videoNode);
    if (status != NO_ERROR) {
      LOGE("Error opening device \"%s\"", entityName);
      return status;
    }

    EntityNameMap entityNameMap;
    entityNameMap.entityName = entityName;
    entityNameMap.devName = (*videoNode)->Name();
    mEntityNamemapDevName.push_back(entityNameMap);
  }
  return status;
}

status_t MtkCameraV4L2API::allocateRequest(int index, int* requestFd) {
  LOGD("[%s][%p] index:%d ", __FUNCTION__, this, index);

  if (requestFd == nullptr) {
    LOGE("invalid argument (requestFd is nullptr)");
    return BAD_VALUE;
  }

  status_t status = NO_ERROR;

  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);

  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }

  mediaController = it->second;

  status = mediaController->allocateRequest(requestFd);
  if (status != NO_ERROR || *requestFd == 0) {
    LOGE("@%s: allocateRequest failed", __FUNCTION__);
  }

  return status;
}

status_t MtkCameraV4L2API::queueRequest(int index, int requestFd) {
  LOGD("[%s][%p] fd:%d ", __FUNCTION__, this, requestFd);
  status_t status = NO_ERROR;

  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);

  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }

  mediaController = it->second;

  status = mediaController->queueRequest(requestFd);
  if (status != NO_ERROR) {
    LOGE("@%s: queueRequest failed", __FUNCTION__);
  }

  return status;
}

status_t MtkCameraV4L2API::reInitRequest(int index, int requestFd) {
  LOGD("[%s][%p] fd:%d ", __FUNCTION__, this, requestFd);
  status_t status = NO_ERROR;

  std::shared_ptr<MtkMediaController> mediaController;
  map<int, std::shared_ptr<MtkMediaController>>::iterator it =
      mMediaControllers.find(index);

  if (CC_UNLIKELY(it == mMediaControllers.end())) {
    return NO_INIT;
  }

  mediaController = it->second;

  status = mediaController->reInitRequest(requestFd);
  if (status != NO_ERROR) {
    LOGE("@%s: queueRequest failed", __FUNCTION__);
  }

  return status;
}
