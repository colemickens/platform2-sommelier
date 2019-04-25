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

#define LOG_TAG "MediaCtrlConfig"

#include <mtklibv4l2/MediaCtrlConfig.h>
#include <mtklibv4l2/MtkMediaController.h>

#include <string>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

using NSCam::BAD_VALUE;
using NSCam::NAME_NOT_FOUND;
using NSCam::NO_ERROR;
using NSCam::NO_INIT;
using NSCam::OK;
using NSCam::PERMISSION_DENIED;
using NSCam::UNKNOWN_ERROR;

#define DEFAULT_PADIDX 0
// the following define could be found in DIP-V4L2's device topology
enum Dip_Subdevice_Pad_Index {
  eP2_RAW_INPUT_PADIDX = 0,
  eP2_TUNING_PADIDX,
  eP2_VIPI_INPUT_PADIDX,
  eP2_LCEI_INPUT_PADIDX,
  eP2_MDP0_PADIDX,
  eP2_MDP1_PADIDX,
  eP2_IMG2_PADIDX,
  eP2_IMG3_PADIDX,
};
// the following define could be found in P1-V4L2's device topology
enum Cam_Subdevice_Pad_Index {
  eP1_META_INPUT_PADIDX = 0,
  eP1_MAINSTREAM_PADIDX,
  eP1_PACKEDOUT_PADIDX,
  eP1_META0_PADIDX,
  eP1_META1_PADIDX,
  eP1_META2_PADIDX,
  eP1_META3_PADIDX,
};

static std::map<MediaDeviceTag, MediaCtrl_P1Topology> gMediaCtrl_P1Topology = {
    {MEDIA_CONTROLLER_P1_OUT1,
     {MEDIA_CONTROLLER_P1_OUT1, "mtk-cam-p1", "mtk-cam-p1",
      "mtk-cam-p1 meta input", "mtk-cam-p1 partial meta 0",
      "mtk-cam-p1 partial meta 1", "mtk-cam-p1 partial meta 2",
      "mtk-cam-p1 partial meta 3", "mtk-cam-p1 main stream", ""}},
    {MEDIA_CONTROLLER_P1_OUT2,
     {MEDIA_CONTROLLER_P1_OUT2, "mtk-cam-p1", "mtk-cam-p1",
      "mtk-cam-p1 meta input", "mtk-cam-p1 partial meta 0",
      "mtk-cam-p1 partial meta 1", "mtk-cam-p1 partial meta 2",
      "mtk-cam-p1 partial meta 3", "mtk-cam-p1 main stream",
      "mtk-cam-p1 packed out"}},
};
static std::map<MediaDeviceTag, MediaCtrl_P2Topology> gMediaCtrl_P2Topology = {
    {MEDIA_CONTROLLER_P2_Preview_OUT1,
     {MEDIA_CONTROLLER_P2_Preview_OUT1, "mtk-cam-dip", "mtk-cam-dip preview",
      "mtk-cam-dip preview Raw Input", "mtk-cam-dip preview Tuning",
      "mtk-cam-dip preview MDP0", ""}},
    {MEDIA_CONTROLLER_P2_Preview_OUT2,
     {MEDIA_CONTROLLER_P2_Preview_OUT2, "mtk-cam-dip", "mtk-cam-dip preview",
      "mtk-cam-dip preview Raw Input", "mtk-cam-dip preview Tuning",
      "mtk-cam-dip preview MDP0", "mtk-cam-dip preview MDP1"}},
    {MEDIA_CONTROLLER_P2_Capture_OUT1,
     {MEDIA_CONTROLLER_P2_Capture_OUT1, "mtk-cam-dip", "mtk-cam-dip capture",
      "mtk-cam-dip capture Raw Input", "mtk-cam-dip capture Tuning",
      "mtk-cam-dip capture MDP0", ""}},
    {MEDIA_CONTROLLER_P2_Capture_OUT2,
     {MEDIA_CONTROLLER_P2_Capture_OUT2, "mtk-cam-dip", "mtk-cam-dip capture",
      "mtk-cam-dip capture Raw Input", "mtk-cam-dip capture Tuning",
      "mtk-cam-dip capture MDP0", "mtk-cam-dip capture MDP1"}},
    {MEDIA_CONTROLLER_P2_Record_OUT1,
     {MEDIA_CONTROLLER_P2_Record_OUT1, "mtk-cam-dip", "mtk-cam-dip preview",
      "mtk-cam-dip preview Raw Input", "mtk-cam-dip preview Tuning",
      "mtk-cam-dip preview MDP0", ""}},
    {MEDIA_CONTROLLER_P2_Record_OUT2,
     {MEDIA_CONTROLLER_P2_Record_OUT2, "mtk-cam-dip", "mtk-cam-dip preview",
      "mtk-cam-dip preview Raw Input", "mtk-cam-dip preview Tuning",
      "mtk-cam-dip preview MDP0", "mtk-cam-dip preview MDP1"}},
    {MEDIA_CONTROLLER_P2_Reprocessing_OUT1,
     {MEDIA_CONTROLLER_P2_Reprocessing_OUT1, "mtk-cam-dip",
      "mtk-cam-dip reprocess", "mtk-cam-dip reprocess Raw Input",
      "mtk-cam-dip reprocess Tuning", "mtk-cam-dip reprocess MDP0", ""}},
    {MEDIA_CONTROLLER_P2_Reprocessing_OUT2,
     {MEDIA_CONTROLLER_P2_Reprocessing_OUT2, "mtk-cam-dip",
      "mtk-cam-dip reprocess", "mtk-cam-dip reprocess Raw Input",
      "mtk-cam-dip reprocess Tuning", "mtk-cam-dip reprocess MDP0",
      "mtk-cam-dip reprocess MDP1"}},
};
static std::map<MediaDeviceTag, MediaCtrl_P2NewTopology>
    gMediaCtrl_P2NewTopology = {
        // Need confirm entity names with P2 driver
        {MEDIA_CONTROLLER_P2New_Preview_FD_3DNR_IN4OUT4,
         {MEDIA_CONTROLLER_P2New_Preview_FD_3DNR_IN4OUT4, "mtk-cam-dip",
          "mtk-cam-dip preview", "mtk-cam-dip preview Raw Input",
          "mtk-cam-dip preview Tuning", "mtk-cam-dip preview NR Input",
          "mtk-cam-dip preview Shading", "mtk-cam-dip preview MDP0",
          "mtk-cam-dip preview MDP1", "mtk-cam-dip preview IMG2",
          "mtk-cam-dip preview IMG3"}},
        {MEDIA_CONTROLLER_P2New_Capture_FD_3DNR_IN4OUT4,
         {MEDIA_CONTROLLER_P2New_Capture_FD_3DNR_IN4OUT4, "mtk-cam-dip",
          "mtk-cam-dip capture", "mtk-cam-dip capture Raw Input",
          "mtk-cam-dip capture Tuning", "mtk-cam-dip capture NR Input",
          "mtk-cam-dip capture Shading", "mtk-cam-dip capture MDP0",
          "mtk-cam-dip capture MDP1", "mtk-cam-dip capture IMG2",
          "mtk-cam-dip capture IMG3"}},
        {MEDIA_CONTROLLER_P2New_Reprocessing_FD_3DNR_IN4OUT4,
         {MEDIA_CONTROLLER_P2New_Reprocessing_FD_3DNR_IN4OUT4, "mtk-cam-dip",
          "mtk-cam-dip reprocess", "mtk-cam-dip reprocess Raw Input",
          "mtk-cam-dip reprocess Tuning", "mtk-cam-dip reprocess NR Input",
          "mtk-cam-dip reprocess Shading", "mtk-cam-dip reprocess MDP0",
          "mtk-cam-dip reprocess MDP1", "mtk-cam-dip reprocess IMG2",
          "mtk-cam-dip reprocess IMG3"}},
};

string MediaCtrlConfig::getMediaDeviceNameByTag(MediaDeviceTag mdevTag) {
  LOGD("[%s] mdevTag 0x%08x", __FUNCTION__, mdevTag);

  MediaCtrl_P1Topology* SubP1Topology = NULL;
  MediaCtrl_P2Topology* SubP2Topology = NULL;
  MediaCtrl_P2NewTopology* SubP2NewTopology = NULL;
  int Mdevcase = mdevTag & MEDIA_CONTROLLER_TAG;
  string modelName;
  switch (Mdevcase) {
    case MEDIA_CONTROLLER_P1_TAG: {
      std::map<MediaDeviceTag, MediaCtrl_P1Topology>::iterator itP1topology =
          gMediaCtrl_P1Topology.find(mdevTag);
      if (itP1topology != gMediaCtrl_P1Topology.end()) {
        SubP1Topology = &(itP1topology->second);
        modelName = SubP1Topology->mdev_name;
      } else {
        LOGW(
            "[%s] mdevTag/Mdevcase is not in P1 Media controller definition "
            ":0x%08x/0x%08x",
            __FUNCTION__, mdevTag, Mdevcase);
      }
      break;
    }
    case MEDIA_CONTROLLER_P2_TAG: {
      std::map<MediaDeviceTag, MediaCtrl_P2Topology>::iterator itP2topology =
          gMediaCtrl_P2Topology.find(mdevTag);
      std::map<MediaDeviceTag, MediaCtrl_P2NewTopology>::iterator
          itP2Newtopology = gMediaCtrl_P2NewTopology.find(mdevTag);
      if (itP2topology != gMediaCtrl_P2Topology.end()) {
        SubP2Topology = &(itP2topology->second);
        modelName = SubP2Topology->mdev_name;
      } else if (itP2Newtopology != gMediaCtrl_P2NewTopology.end()) {
        SubP2NewTopology = &(itP2Newtopology->second);
        modelName = SubP2NewTopology->mdev_name;
      } else {
        LOGW(
            "[%s] mdevTag/Mdevcase is not in P2 Media controller definition "
            ":0x%08x/0x%08x",
            __FUNCTION__, mdevTag, Mdevcase);
      }
      break;
    }
    default: {
      LOGE(
          "[%s] mdevTag/Mdevcase is not in Media controller definition "
          ":0x%08x/0x%08x",
          __FUNCTION__, mdevTag, Mdevcase);
      break;
    }
  }
  return modelName;
}

string MediaCtrlConfig::getMediaDevicePathByName(const string& modelName) {
  LOGD("[%s] Target name: %s", __FUNCTION__, modelName.c_str());
  const char* MEDIADEVICES = "media";
  const char* DEVICE_PATH = "/dev/";

  string mediaDevicePath;
  DIR* dir;
  dirent* dirEnt;

  vector<string> candidates;
  if ((dir = opendir(DEVICE_PATH)) != nullptr) {
    while ((dirEnt = readdir(dir)) != nullptr) {
      string candidatePath = dirEnt->d_name;
      size_t pos = candidatePath.find(MEDIADEVICES);
      if (pos != string::npos) {
        LOGD("Found media device candidate: %s", candidatePath.c_str());
        string found_one = DEVICE_PATH;
        found_one += candidatePath;
        candidates.push_back(found_one);
      }
    }
    closedir(dir);
  } else {
    LOGE("Failed to open directory: %s", DEVICE_PATH);
  }

  LOGD("candidates size %d", candidates.size());
  status_t retVal = NO_ERROR;
  for (const auto& candidate : candidates) {
    MtkMediaController controller(modelName.c_str(), candidate.c_str());
    retVal = controller.init();

    // We may run into devices that this HAL won't use -> skip to next
    if (retVal == PERMISSION_DENIED) {
      LOGD("Not enough permissions to access %s.", candidate.c_str());
      continue;
    } else if (retVal != NO_ERROR) {
      LOGD(" %s controller.init error value = %d.", candidate.c_str(), retVal);
      continue;
    }

    struct media_device_info info;
    int ret = controller.getMediaDevInfo(&info);
    if (ret != OK) {
      LOGE("Cannot get media device information.");
      continue;
    }

    if (strncmp(info.model, modelName.c_str(),
                MIN(sizeof(info.model), modelName.size())) == 0) {
      LOGD("Found device that matches: %s %s", modelName.c_str(),
           candidate.c_str());
      mediaDevicePath += candidate;
      ret = controller.close();
      break;
    }
    ret = controller.close();
  }

  return mediaDevicePath;
}

void MediaCtrlConfig::addVideoNodes(string name, MediaCtlConfig* config) {
  if (!name.empty() && config) {
    MediaCtlElement mediaCtlElement;
    mediaCtlElement.name = name;
    config->mVideoNodes.push_back(mediaCtlElement);
    LOGD("[%s] add videonode name: %s", __FUNCTION__, name.c_str());
  } else {
    LOGE("[%s] name empty or null pointer: %s/%p", __FUNCTION__, name.c_str(),
         config);
  }
}

void MediaCtrlConfig::addLinkParams(const string& srcName,
                                    int srcPad,
                                    const string& sinkName,
                                    int sinkPad,
                                    int enable,
                                    int flags,
                                    MediaCtlConfig* config) {
  if (!srcName.empty() && !sinkName.empty() && config) {
    MediaCtlLinkParams mediaCtlLinkParams;
    mediaCtlLinkParams.srcName = srcName;
    mediaCtlLinkParams.srcPad = srcPad;
    mediaCtlLinkParams.sinkName = sinkName;
    mediaCtlLinkParams.sinkPad = sinkPad;
    mediaCtlLinkParams.enable = enable;
    mediaCtlLinkParams.flags = flags;
    config->mLinkParams.push_back(mediaCtlLinkParams);
    LOGD("[%s] srcName:%s, Pad:%d, sinkName:%s, Pad:%d, enable:%d, flag:0x%08x",
         __FUNCTION__, srcName.c_str(), srcPad, sinkName.c_str(), sinkPad,
         enable, flags);
  } else {
    LOGE("[%s] srcname/sinkname empty or null pointer: %s/%s/%p", __FUNCTION__,
         srcName.c_str(), sinkName.c_str(), config);
  }
}

void MediaCtrlConfig::CreateMediaCtlGraph(MediaDeviceTag mdevTag,
                                          bool hasTuning,
                                          MediaCtlConfig* mediaCtlConfig) {
  LOGD("[%s] mdevTag 0x%08x", __FUNCTION__, mdevTag);

  bool bNewP2Topology = false;

  MediaCtrl_P1Topology* SubP1Topology = NULL;
  MediaCtrl_P2Topology* SubP2Topology = NULL;
  MediaCtrl_P2NewTopology* SubP2NewTopology = NULL;

  int Mdevcase = mdevTag & MEDIA_CONTROLLER_TAG;
  switch (Mdevcase) {
    case MEDIA_CONTROLLER_P1_TAG: {
      std::map<MediaDeviceTag, MediaCtrl_P1Topology>::iterator itP1topology =
          gMediaCtrl_P1Topology.find(mdevTag);
      if (itP1topology != gMediaCtrl_P1Topology.end()) {
        SubP1Topology = &(itP1topology->second);
      } else {
        LOGE(
            "[%s] mdevTag/Mdevcase is not in P1 Media controller definition "
            ":0x%08x/0x%08x",
            __FUNCTION__, mdevTag, Mdevcase);
        break;
      }

      addVideoNodes(SubP1Topology->hub_name, mediaCtlConfig);
      if (hasTuning) {
        addVideoNodes(SubP1Topology->tunig_source_name, mediaCtlConfig);
        addVideoNodes(SubP1Topology->tunig_sink1_name, mediaCtlConfig);
        addVideoNodes(SubP1Topology->tunig_sink2_name, mediaCtlConfig);
        addLinkParams(SubP1Topology->tunig_source_name, DEFAULT_PADIDX,
                      SubP1Topology->hub_name, eP1_META_INPUT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP1Topology->hub_name, eP1_META0_PADIDX,
                      SubP1Topology->tunig_sink1_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP1Topology->hub_name, eP1_META1_PADIDX,
                      SubP1Topology->tunig_sink2_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addVideoNodes(SubP1Topology->tunig_sink3_name, mediaCtlConfig);
        addLinkParams(SubP1Topology->hub_name, eP1_META2_PADIDX,
                      SubP1Topology->tunig_sink3_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addVideoNodes(SubP1Topology->tunig_sink4_name, mediaCtlConfig);
        addLinkParams(SubP1Topology->hub_name, eP1_META3_PADIDX,
                      SubP1Topology->tunig_sink4_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
      }
      addVideoNodes(SubP1Topology->imgo_sink_name, mediaCtlConfig);
      addVideoNodes(SubP1Topology->rrzo_sink_name, mediaCtlConfig);
      addLinkParams(SubP1Topology->hub_name, eP1_MAINSTREAM_PADIDX,
                    SubP1Topology->imgo_sink_name, DEFAULT_PADIDX, 1,
                    MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
      addLinkParams(SubP1Topology->hub_name, eP1_PACKEDOUT_PADIDX,
                    SubP1Topology->rrzo_sink_name, DEFAULT_PADIDX, 1,
                    MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
      break;
    }
    case MEDIA_CONTROLLER_P2_TAG: {
      std::map<MediaDeviceTag, MediaCtrl_P2Topology>::iterator itP2topology =
          gMediaCtrl_P2Topology.find(mdevTag);
      std::map<MediaDeviceTag, MediaCtrl_P2NewTopology>::iterator
          itP2Newtopology = gMediaCtrl_P2NewTopology.find(mdevTag);
      if (itP2topology != gMediaCtrl_P2Topology.end()) {
        SubP2Topology = &(itP2topology->second);
      } else if (itP2Newtopology != gMediaCtrl_P2NewTopology.end()) {
        SubP2NewTopology = &(itP2Newtopology->second);
        bNewP2Topology = true;
      } else {
        LOGE(
            "[%s] mdevTag/Mdevcase is not in P2 Media controller definition "
            ":0x%08x/0x%08x",
            __FUNCTION__, mdevTag, Mdevcase);
        break;
      }
      if (bNewP2Topology == false) {
        addVideoNodes(SubP2Topology->hub_name, mediaCtlConfig);
        addVideoNodes(SubP2Topology->raw_source_name, mediaCtlConfig);
        addVideoNodes(SubP2Topology->sink1_name, mediaCtlConfig);
        addVideoNodes(SubP2Topology->sink2_name, mediaCtlConfig);
        addLinkParams(SubP2Topology->raw_source_name, DEFAULT_PADIDX,
                      SubP2Topology->hub_name, eP2_RAW_INPUT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2Topology->hub_name, eP2_MDP0_PADIDX,
                      SubP2Topology->sink1_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2Topology->hub_name, eP2_MDP1_PADIDX,
                      SubP2Topology->sink2_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        if (hasTuning) {
          addVideoNodes(SubP2Topology->tunig_source_name, mediaCtlConfig);
          addLinkParams(SubP2Topology->tunig_source_name, DEFAULT_PADIDX,
                        SubP2Topology->hub_name, eP2_TUNING_PADIDX, 1,
                        MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        }
      } else {
        addVideoNodes(SubP2NewTopology->hub_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->raw_source_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->raw_source2_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->raw_source3_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->sink1_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->sink2_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->sink3_name, mediaCtlConfig);
        addVideoNodes(SubP2NewTopology->sink4_name, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->raw_source_name, DEFAULT_PADIDX,
                      SubP2NewTopology->hub_name, eP2_RAW_INPUT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->raw_source2_name, DEFAULT_PADIDX,
                      SubP2NewTopology->hub_name, eP2_VIPI_INPUT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->raw_source3_name, DEFAULT_PADIDX,
                      SubP2NewTopology->hub_name, eP2_LCEI_INPUT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->hub_name, eP2_MDP0_PADIDX,
                      SubP2NewTopology->sink1_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->hub_name, eP2_MDP1_PADIDX,
                      SubP2NewTopology->sink2_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->hub_name, eP2_IMG2_PADIDX,
                      SubP2NewTopology->sink3_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        addLinkParams(SubP2NewTopology->hub_name, eP2_IMG3_PADIDX,
                      SubP2NewTopology->sink4_name, DEFAULT_PADIDX, 1,
                      MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        if (hasTuning) {
          addVideoNodes(SubP2NewTopology->tunig_source_name, mediaCtlConfig);
          addLinkParams(SubP2NewTopology->tunig_source_name, DEFAULT_PADIDX,
                        SubP2NewTopology->hub_name, eP2_TUNING_PADIDX, 1,
                        MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        }
      }
      break;
    }
    default: {
      LOGE(
          "[%s] mdevTag/Mdevcase is not in Media controller definition "
          ":0x%08x/0x%08x",
          __FUNCTION__, mdevTag, Mdevcase);
      break;
    }
  }
}
