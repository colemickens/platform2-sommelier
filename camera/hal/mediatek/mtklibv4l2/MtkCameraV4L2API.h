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

#ifndef CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MTKCAMERAV4L2API_H_
#define CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MTKCAMERAV4L2API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Errors.h"
#include "CommonUtilMacros.h"

#include "MtkMediaController.h"
#include "MediaCtrlConfig.h"
#include "MediaEntity.h"
#include "cros-camera/v4l2_device.h"

#include <linux/media.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <mtkcam/def/common.h>

using cros::V4L2Buffer;
using cros::V4L2Device;
using cros::V4L2Format;
using cros::V4L2Subdevice;
using cros::V4L2VideoNode;
using NSCam::status_t;
using std::map;
using std::string;
using std::vector;

enum DynamicLinkTag {
  DYNAMIC_LINK_BYVIDEONAME,
  DYNAMIC_LINK_BYLINKDESC,
  DYNAMIC_LINK_TAG_NUM
};

class VISIBILITY_PUBLIC MtkCameraV4L2API {
 public:
  MtkCameraV4L2API();
  ~MtkCameraV4L2API();
  status_t openMediaDevice(const string& drivername, int* index);
  status_t getMediaDeviceInfo(int index, struct media_device_info* info);
  status_t enumEntities(int index, struct media_entity_desc* entitydesc);
  status_t enumLinks(int index, struct media_links_enum* linksEnum);
  status_t setupLink(int index, struct media_link_desc* linkDesc);

  /*
   * To setup all links according to the given mdevtag, and retrieve all
   * media entities. If this method return a value which is smaller than
   * 0, it indicateds to GNU C error code.
   *  @param mdevtag: pre-defined tag that describes the topology which
   *                  Mediatek ISP driver supports.
   *  @param mediaEntity: an output container that stored the media entites
   *                      in the topology.
   *  @param hasTuning: true to enable tuning video device(s), false to disable
   *                    tuning video device(s).
   *  @return: The index of media device.
   */
  int openAndsetupAllLinks(MediaDeviceTag mdevtag,
                           vector<std::shared_ptr<MediaEntity>>* mediaEntity,
                           bool hasTuning = true);
  status_t openMediaDevice(MediaDeviceTag mdevtag, bool hasTuning, int* index);
  status_t setupAllLinks(int index);
  status_t resetAllLinks(int index = 1);
  status_t closeMediaDevice(int index = 1);
  status_t allocateRequest(int index, int* requestFd);
  status_t queueRequest(int index, int requestFd);
  status_t reInitRequest(int index, int requestFd);

  status_t pollVideoDevice(int fd, int timeout, int mdev_index);
  status_t MapMemory(int fd,
                     unsigned int index,
                     int prot,
                     int flags,
                     std::vector<void*>* mapped,
                     int mdev_index);

  status_t disableLink(int index, DynamicLinkTag tag, const char* name);
  status_t enableLink(int index, DynamicLinkTag tag, const char* name);
  status_t enableAllLinks(int index);

 private:
  status_t setupAllLinks(int index,
                         vector<std::shared_ptr<MediaEntity>>* mediaEntity);

  status_t openVideoNode(
      const std::shared_ptr<MtkMediaController>& mediaController,
      const char* entityName,
      std::shared_ptr<MediaEntity>* entity,
      std::shared_ptr<V4L2Device>* videoNode);

 private:
  map<int, std::shared_ptr<MtkMediaController>> mMediaControllers;
  map<int, MediaDeviceTag> mMediaDeviceTag;

  map<int, vector<std::shared_ptr<V4L2Device>>> mMdevAutoConfiguredNodes;
  map<int, vector<std::shared_ptr<V4L2Device>>> mMdevUserConfiguredNodes;

  vector<EntityNameMap> mEntityNamemapDevName;

  int mMdevCount;
  bool mIsAutoConfigPipeline;
  bool mHasTuning;
};  // class MtkCameraV4L2API

#endif  // CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MTKCAMERAV4L2API_H_
