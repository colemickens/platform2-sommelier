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

#ifndef CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MTKMEDIACONTROLLER_H_
#define CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MTKMEDIACONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Errors.h"
#include "CommonUtilMacros.h"

#include "MediaEntity.h"
#include <linux/media.h>

using NSCam::status_t;
using std::map;
using std::string;
using std::vector;

/**
 * \class MediaController
 *
 * This class is used for discovering and configuring the internal topology
 * of a media device. Devices are modelled as an oriented graph of building
 * blocks called media entities. The media entities are connected to each other
 * through pads.
 *
 * Each media entity corresponds to a V4L2 subdevice. This class is also used
 * for configuring the V4L2 subdevices.
 */
class MtkMediaController {
 public:
  explicit MtkMediaController(const char* devname, const char* path);
  ~MtkMediaController();

  status_t init();
  status_t getMediaDevInfo(struct media_device_info* info);
  status_t getDevName(string* devname);
  status_t enumEntity(struct media_entity_desc* mediaEntityDesc);
  status_t enumLinks(struct media_links_enum* linksEnum);
  status_t setupLink(struct media_link_desc* linkDesc);
  status_t allocateRequest(int* requestFd);
  status_t queueRequest(int requestFd);
  status_t reInitRequest(int requestFd);
  status_t enumAllLinks();
  status_t resetAllLinks();

  status_t getMediaEntity(std::shared_ptr<MediaEntity>* entity,
                          const char* name);
  status_t getMediaEntityID(int* entityID, const char* name);
  status_t getLinkDescbyEntityName(struct media_link_desc* linkdesc,
                                   const char* name);
  status_t getLinkDescbyEntityName(struct media_link_desc* linkdesc,
                                   const char* srcname,
                                   const char* sinkname);
  int getfd() { return mFd; }
  status_t close();
  status_t disableLink(media_link_desc* link);
  status_t enableLink(media_link_desc* link);
  status_t enableAllLinks();
  status_t storeAllLinks();

 private:
  int xioctl(int request, void* arg) const;
  status_t open();
  status_t getDeviceInfo();
  status_t findMediaEntityById(int index,
                               struct media_entity_desc* mediaEntityDesc);

  status_t enumLink(struct media_links_enum* linksEnum);

 private:
  string mModelName;
  string mPath; /*!< path to device in file system, ex: /dev/media0 */
  int mFd;      /*!< file descriptor obtained when device is open */
  struct media_device_info mDeviceInfo; /*!< media controller device info */
  /*!< media entity descriptors, Key: entity name */
  map<string, struct media_entity_desc> mEntityDesciptors;
  /*!< MediaEntities, Key: entity name */
  map<string, std::shared_ptr<MediaEntity>> mEntities;
  /*!< media_link_desc, Key: sink entity ID */
  map<int, struct media_link_desc> mSinkEntitiesLinkDesc;
  /*!< media_link_desc, Key: sink entity ID */
  map<int, struct media_link_desc> mInitialSinkEntitiesLinkDesc;
  /*!< media_link_desc, Key: sink entity ID */
  map<int, struct media_link_desc> mSourceEntitiesLinkDesc;
  /*!< media_link_desc, Key: sink entity ID */
  map<int, struct media_link_desc> mInitialSourceEntitiesLinkDesc;
};  // class MediaController

#endif  // CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MTKMEDIACONTROLLER_H_
