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

#define LOG_TAG "V4L2StreamNode"

#include <_videodev2.h>  // V4L2_PIX_FMT_MTISP_SBGGR8, ... etc
#include <CommonUtilMacros.h>
#include <cutils/compiler.h>
#include <fcntl.h>
#include <mtkcam/utils/imgbuf/ImageBufferHeap.h>
#include <mtkcam/utils/std/Format.h>
#include <mtkcam/utils/std/Log.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mtkcam/drv/iopipe/src/common/V4L2StreamNode.h"

#include <map>
#include <memory>
#include <utility>

using NSCam::status_t;
using NSCam::v4l2::BufInfo;
using NSCam::v4l2::V4L2StreamNode;

namespace NSCam {
namespace v4l2 {

/* maximum 3 buffers for tuning */
const unsigned int STREAM_NODE_BUFFERS = 3;

struct ProfileInfo {
  uint32_t ColorProfile;
  uint32_t V4L2_ColorSpace;
  uint32_t V4L2_Quantization;
};

static const ProfileInfo Profile_Mapper[] = {
    {NSCam::eCOLORPROFILE_UNKNOWN, V4L2_COLORSPACE_DEFAULT,
     V4L2_QUANTIZATION_FULL_RANGE},
    {NSCam::eCOLORPROFILE_BT601_LIMITED, V4L2_COLORSPACE_DEFAULT,
     V4L2_QUANTIZATION_LIM_RANGE},
    {NSCam::eCOLORPROFILE_BT601_FULL, V4L2_COLORSPACE_DEFAULT,
     V4L2_QUANTIZATION_FULL_RANGE},
    {NSCam::eCOLORPROFILE_BT709_LIMITED, V4L2_COLORSPACE_REC709,
     V4L2_QUANTIZATION_LIM_RANGE},
    {NSCam::eCOLORPROFILE_BT709_FULL, V4L2_COLORSPACE_REC709,
     V4L2_QUANTIZATION_FULL_RANGE},
    {NSCam::eCOLORPROFILE_BT2020_LIMITED, V4L2_COLORSPACE_BT2020,
     V4L2_QUANTIZATION_LIM_RANGE},
    {NSCam::eCOLORPROFILE_BT2020_FULL, V4L2_COLORSPACE_BT2020,
     V4L2_QUANTIZATION_FULL_RANGE},
    {NSCam::eCOLORPROFILE_JPEG, V4L2_COLORSPACE_JPEG,
     V4L2_QUANTIZATION_FULL_RANGE}};

struct FormatInfo {
  int32_t ImageFormat;
  int32_t PixelCode;
  std::string fullName;
  std::string shortName;
};

static bool IS_MULTIPLANAR(uint32_t format) {
  switch (format) {
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_YVU420M:
      return true;
    default:
      return false;
  }
}

static const FormatInfo Format_Mapper[] = {
    {NSCam::eImgFmt_YUY2, V4L2_PIX_FMT_YUYV, "V4L2_PIX_FMT_YUYV", "YUYV"},
    {NSCam::eImgFmt_NV12, V4L2_PIX_FMT_NV12M, "V4L2_PIX_FMT_NV12M", "NV12"},
    {NSCam::eImgFmt_YV12, V4L2_PIX_FMT_YVU420, "V4L2_PIX_FMT_YVU420", "YV12"},
    // bayer order expansion
    {NSCam::eImgFmt_BAYER8_BGGR, V4L2_PIX_FMT_MTISP_SBGGR8,
     "V4L2_PIX_FMT_MTISP_SBGGR8", "MTISP_SBGGR8"},
    {NSCam::eImgFmt_BAYER8_GBRG, V4L2_PIX_FMT_MTISP_SGBRG8,
     "V4L2_PIX_FMT_MTISP_SGBRG8", "MTISP_SGBRG8"},
    {NSCam::eImgFmt_BAYER8_GRBG, V4L2_PIX_FMT_MTISP_SGRBG8,
     "V4L2_PIX_FMT_MTISP_SGRBG8", "MTISP_SGRBG8"},
    {NSCam::eImgFmt_BAYER8_RGGB, V4L2_PIX_FMT_MTISP_SRGGB8,
     "V4L2_PIX_FMT_MTISP_SRGGB8", "MTISP_SRGGB8"},
    {NSCam::eImgFmt_BAYER10_BGGR, V4L2_PIX_FMT_MTISP_SBGGR10,
     "V4L2_PIX_FMT_MTISP_SBGGR10", "MTISP_SBGGR10"},
    {NSCam::eImgFmt_BAYER10_GBRG, V4L2_PIX_FMT_MTISP_SGBRG10,
     "V4L2_PIX_FMT_MTISP_SGBRG10", "MTISP_SGBRG10"},
    {NSCam::eImgFmt_BAYER10_GRBG, V4L2_PIX_FMT_MTISP_SGRBG10,
     "V4L2_PIX_FMT_MTISP_SGRBG10", "MTISP_SGRBG10"},
    {NSCam::eImgFmt_BAYER10_RGGB, V4L2_PIX_FMT_MTISP_SRGGB10,
     "V4L2_PIX_FMT_MTISP_SRGGB10", "MTISP_SRGGB10"},
    {NSCam::eImgFmt_BAYER12_BGGR, V4L2_PIX_FMT_MTISP_SBGGR12,
     "V4L2_PIX_FMT_MTISP_SBGGR12", "MTISP_SBGGR12"},
    {NSCam::eImgFmt_BAYER12_GBRG, V4L2_PIX_FMT_MTISP_SGBRG12,
     "V4L2_PIX_FMT_MTISP_SGBRG12", "MTISP_SGBRG12"},
    {NSCam::eImgFmt_BAYER12_GRBG, V4L2_PIX_FMT_MTISP_SGRBG12,
     "V4L2_PIX_FMT_MTISP_SGRBG12", "MTISP_SGRBG12"},
    {NSCam::eImgFmt_BAYER12_RGGB, V4L2_PIX_FMT_MTISP_SRGGB12,
     "V4L2_PIX_FMT_MTISP_SRGGB12", "MTISP_SRGGB12"},
    {NSCam::eImgFmt_BAYER14_BGGR, V4L2_PIX_FMT_MTISP_SBGGR14,
     "V4L2_PIX_FMT_MTISP_SBGGR14", "MTISP_SBGGR14"},
    {NSCam::eImgFmt_BAYER14_GBRG, V4L2_PIX_FMT_MTISP_SGBRG14,
     "V4L2_PIX_FMT_MTISP_SGBRG14", "MTISP_SGBRG14"},
    {NSCam::eImgFmt_BAYER14_GRBG, V4L2_PIX_FMT_MTISP_SGRBG14,
     "V4L2_PIX_FMT_MTISP_SGRBG14", "MTISP_SGRBG14"},
    {NSCam::eImgFmt_BAYER14_RGGB, V4L2_PIX_FMT_MTISP_SRGGB14,
     "V4L2_PIX_FMT_MTISP_SRGGB14", "MTISP_SRGGB14"},
    {NSCam::eImgFmt_FG_BAYER8_BGGR, V4L2_PIX_FMT_MTISP_SBGGR8F,
     "V4L2_PIX_FMT_MTISP_SBGGR8F", "MTISP_SBGGR8F"},
    {NSCam::eImgFmt_FG_BAYER8_GBRG, V4L2_PIX_FMT_MTISP_SGBRG8F,
     "V4L2_PIX_FMT_MTISP_SGBRG8F", "MTISP_SGBRG8F"},
    {NSCam::eImgFmt_FG_BAYER8_GRBG, V4L2_PIX_FMT_MTISP_SGRBG8F,
     "V4L2_PIX_FMT_MTISP_SGRBG8F", "MTISP_SGRBG8F"},
    {NSCam::eImgFmt_FG_BAYER8_RGGB, V4L2_PIX_FMT_MTISP_SRGGB8F,
     "V4L2_PIX_FMT_MTISP_SRGGB8F", "MTISP_SRGGB8F"},
    {NSCam::eImgFmt_FG_BAYER10_BGGR, V4L2_PIX_FMT_MTISP_SBGGR10F,
     "V4L2_PIX_FMT_MTISP_SBGGR10F", "MTISP_SBGGR10F"},
    {NSCam::eImgFmt_FG_BAYER10_GBRG, V4L2_PIX_FMT_MTISP_SGBRG10F,
     "V4L2_PIX_FMT_MTISP_SGBRG10F", "MTISP_SGBRG10F"},
    {NSCam::eImgFmt_FG_BAYER10_GRBG, V4L2_PIX_FMT_MTISP_SGRBG10F,
     "V4L2_PIX_FMT_MTISP_SGRBG10F", "MTISP_SGRBG10F"},
    {NSCam::eImgFmt_FG_BAYER10_RGGB, V4L2_PIX_FMT_MTISP_SRGGB10F,
     "V4L2_PIX_FMT_MTISP_SRGGB10F", "MTISP_SRGGB10F"},
    {NSCam::eImgFmt_FG_BAYER12_BGGR, V4L2_PIX_FMT_MTISP_SBGGR12F,
     "V4L2_PIX_FMT_MTISP_SBGGR12F", "MTISP_SBGGR12F"},
    {NSCam::eImgFmt_FG_BAYER12_GBRG, V4L2_PIX_FMT_MTISP_SGBRG12F,
     "V4L2_PIX_FMT_MTISP_SGBRG12F", "MTISP_SGBRG12F"},
    {NSCam::eImgFmt_FG_BAYER12_GRBG, V4L2_PIX_FMT_MTISP_SGRBG12F,
     "V4L2_PIX_FMT_MTISP_SGRBG12F", "MTISP_SGRBG12F"},
    {NSCam::eImgFmt_FG_BAYER12_RGGB, V4L2_PIX_FMT_MTISP_SRGGB12F,
     "V4L2_PIX_FMT_MTISP_SRGGB12F", "MTISP_SRGGB12F"},
    {NSCam::eImgFmt_FG_BAYER14_BGGR, V4L2_PIX_FMT_MTISP_SBGGR14F,
     "V4L2_PIX_FMT_MTISP_SBGGR14F", "MTISP_SBGGR14F"},
    {NSCam::eImgFmt_FG_BAYER14_GBRG, V4L2_PIX_FMT_MTISP_SGBRG14F,
     "V4L2_PIX_FMT_MTISP_SGBRG14F", "MTISP_SGBRG14F"},
    {NSCam::eImgFmt_FG_BAYER14_GRBG, V4L2_PIX_FMT_MTISP_SGRBG14F,
     "V4L2_PIX_FMT_MTISP_SGRBG14F", "MTISP_SGRBG14F"},
    {NSCam::eImgFmt_FG_BAYER14_RGGB, V4L2_PIX_FMT_MTISP_SRGGB14F,
     "V4L2_PIX_FMT_MTISP_SRGGB14F", "MTISP_SRGGB14F"}};

static const std::map<V4L2StreamNode::ID, std::string> s_ID_Mapper = {
    // P1
    {V4L2StreamNode::ID_P1_SUBDEV, "mtk-cam-p1"},
    {V4L2StreamNode::ID_P1_MAIN_STREAM, "mtk-cam-p1 main stream"},
    {V4L2StreamNode::ID_P1_SUB_STREAM, "mtk-cam-p1 packed out"},
    {V4L2StreamNode::ID_P1_META1, "mtk-cam-p1 partial meta 0"},
    {V4L2StreamNode::ID_P1_META2, "mtk-cam-p1 partial meta 1"},
    {V4L2StreamNode::ID_P1_META3, "mtk-cam-p1 partial meta 2"},
    {V4L2StreamNode::ID_P1_META4, "mtk-cam-p1 partial meta 3"},
    {V4L2StreamNode::ID_P1_TUNING, "mtk-cam-p1 meta input"},
    // P2
    {V4L2StreamNode::ID_P2_SUBDEV, "mtk-cam-dip preview"},
    {V4L2StreamNode::ID_P2_RAW_INPUT, "mtk-cam-dip preview Raw Input"},
    {V4L2StreamNode::ID_P2_TUNING, "mtk-cam-dip preview Tuning"},
    {V4L2StreamNode::ID_P2_VIPI, "mtk-cam-dip preview NR Input"},
    {V4L2StreamNode::ID_P2_LCEI, "mtk-cam-dip preview Shading"},
    {V4L2StreamNode::ID_P2_MDP0, "mtk-cam-dip preview MDP0"},
    {V4L2StreamNode::ID_P2_MDP1, "mtk-cam-dip preview MDP1"},
    {V4L2StreamNode::ID_P2_IMG2, "mtk-cam-dip preview IMG2"},
    {V4L2StreamNode::ID_P2_IMG3, "mtk-cam-dip preview IMG3"},
    {V4L2StreamNode::ID_P2_CAP_SUBDEV, "mtk-cam-dip capture"},
    {V4L2StreamNode::ID_P2_CAP_RAW_INPUT, "mtk-cam-dip capture Raw Input"},
    {V4L2StreamNode::ID_P2_CAP_TUNING, "mtk-cam-dip capture Tuning"},
    {V4L2StreamNode::ID_P2_CAP_VIPI, "mtk-cam-dip capture NR Input"},
    {V4L2StreamNode::ID_P2_CAP_LCEI, "mtk-cam-dip capture Shading"},
    {V4L2StreamNode::ID_P2_CAP_MDP0, "mtk-cam-dip capture MDP0"},
    {V4L2StreamNode::ID_P2_CAP_MDP1, "mtk-cam-dip capture MDP1"},
    {V4L2StreamNode::ID_P2_CAP_IMG2, "mtk-cam-dip capture IMG2"},
    {V4L2StreamNode::ID_P2_CAP_IMG3, "mtk-cam-dip capture IMG3"},
    {V4L2StreamNode::ID_P2_REP_SUBDEV, "mtk-cam-dip reprocess"},
    {V4L2StreamNode::ID_P2_REP_RAW_INPUT, "mtk-cam-dip reprocess Raw Input"},
    {V4L2StreamNode::ID_P2_REP_TUNING, "mtk-cam-dip reprocess Tuning"},
    {V4L2StreamNode::ID_P2_REP_VIPI, "mtk-cam-dip reprocess NR Input"},
    {V4L2StreamNode::ID_P2_REP_LCEI, "mtk-cam-dip reprocess Shading"},
    {V4L2StreamNode::ID_P2_REP_MDP0, "mtk-cam-dip reprocess MDP0"},
    {V4L2StreamNode::ID_P2_REP_MDP1, "mtk-cam-dip reprocess MDP1"},
    {V4L2StreamNode::ID_P2_REP_IMG2, "mtk-cam-dip reprocess IMG2"},
    {V4L2StreamNode::ID_P2_REP_IMG3, "mtk-cam-dip reprocess IMG3"},
};

/* find the stream node ID by the given name */
static V4L2StreamNode::ID find_stream_node_id(const std::string& node_name) {
  /* complexity: O(N) */
  auto _begin = s_ID_Mapper.begin();
  while (_begin != s_ID_Mapper.end()) {
    if (_begin->second == node_name) {
      return _begin->first;
    }
    ++_begin;
  }
  return V4L2StreamNode::ID_UNKNOWN;
}

V4L2StreamNode::V4L2StreamNode(std::shared_ptr<V4L2VideoNode> node,
                               std::string const name)
    : mState(StreamNodeState::CLOSED),
      mName(name),
      mType(V4L2_MEMORY_DMABUF),
      mNode(node),
      mId(find_stream_node_id(name)),
      mBufferPoolSize(0),
      mActive(false),
      mTransform(0) {
  if (!mNode->IsOpened()) {
    LOGE("@%s device %s is not open yet", __FUNCTION__, name.c_str());
    return;
  }
  mState = StreamNodeState::OPEN;
}

V4L2StreamNode::~V4L2StreamNode() {
  LOGD("+");
  if (mState != StreamNodeState::CLOSED) {
    if (mType == V4L2_MEMORY_MMAP) {
      for (auto& it : mBuffers) {
        auto addr = mMappAddrs.find(it.Index());
        if (addr != mMappAddrs.end()) {
          LOGI("un-MapMemory addr %p length %u", addr->second, it.Length(0));
          if (munmap(addr->second, it.Length(0)) != NO_ERROR) {
            LOGE("munmap fail");
          }
        }
        auto fd = mFds.find(it.Index());
        if (fd != mFds.end()) {
          LOGI("close fd %d", fd->second);
          if (close(fd->second) != NO_ERROR) {
            LOGE("close fail");
          }
        }
      }
    }
  }
}

status_t V4L2StreamNode::setFormatLocked(
    IImageBufferAllocator::ImgParam* imgParam) {
  LOGD("setFormatLocked imgParam+");
  if (mState != StreamNodeState::OPEN) {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  V4L2Format v4l2_fmt;
  int fmt = bayerOrderTransform(imgParam->imgFormat, imgParam->sensorOrder);

  v4l2_fmt.SetType(mNode->GetBufferType());
  v4l2_fmt.SetBytesPerLine(0, 0);
  v4l2_fmt.SetSizeImage(0, 0);
  if (fmt != eImgFmt_BLOB) {
    size_t i = 0;
    for (; i < ARRAY_SIZE(Format_Mapper); i++) {
      if (fmt == Format_Mapper[i].ImageFormat) {
        v4l2_fmt.SetPixelFormat(Format_Mapper[i].PixelCode);
        LOGI("Image format (0x%x->%s)", fmt, Format_Mapper[i].fullName.c_str());
        break;
      }
      LOGI("Image format (%s)", Format_Mapper[i].fullName.c_str());
    }
    if (i == ARRAY_SIZE(Format_Mapper)) {
      LOGE("Unsupported format 0x%x", fmt);
      return -EINVAL;
    }
    v4l2_fmt.SetWidth(imgParam->imgSize.w);
    v4l2_fmt.SetHeight(imgParam->imgSize.h);
    i = 0;
    for (; i < ARRAY_SIZE(Profile_Mapper); i++) {
      if (imgParam->colorProfile == Profile_Mapper[i].ColorProfile) {
        v4l2_fmt.SetColorSpace(Profile_Mapper[i].V4L2_ColorSpace);
        v4l2_fmt.SetQuantization(Profile_Mapper[i].V4L2_Quantization);
        LOGI("Color Space (0x%x->0x%x)", imgParam->colorProfile,
             Profile_Mapper[i].V4L2_ColorSpace);
        break;
      }
      LOGI("Color Profile (0x%x)", Profile_Mapper[i].ColorProfile);
    }
    if (i == ARRAY_SIZE(Profile_Mapper)) {
      LOGE("Unsupported color profile 0x%x", imgParam->colorProfile);
      return -EINVAL;
    }
    if (IS_MULTIPLANAR(v4l2_fmt.PixelFormat())) {
      for (i = 0;
           i < NSCam::Utils::Format::queryPlaneCount(imgParam->imgFormat);
           i++) {
        v4l2_fmt.SetBytesPerLine(imgParam->bufStridesInBytes[i], i);
        MY_LOGI("plane %d, bpp %u %dx%d", i, imgParam->bufStridesInBytes[i],
                imgParam->imgSize.w, imgParam->imgSize.h);
      }
    }
  }
  status_t ret = mNode->SetFormat(v4l2_fmt);
  CheckError(ret != NO_ERROR, ret, "SetFormat failed");
  ret = mNode->GetFormat(&mFormat);
  CheckError(ret != NO_ERROR, ret, "GetFormat failed");

  // should not use incomplete v4l2_fmt
  // update for meta data
  if ((fmt == eImgFmt_BLOB) && (imgParam)) {
    IImageBufferAllocator::ImgParam update =
        IImageBufferAllocator::ImgParam(mFormat.SizeImage(0), 0);
    *imgParam = update;
    LOGD("update meta data for blob buffer");
  }

  mState = StreamNodeState::CONFIGURED;
  return NO_ERROR;
}

status_t V4L2StreamNode::setupBuffersLocked() {
  LOGD("+");

  if (mState != StreamNodeState::OPEN &&
      mState != StreamNodeState::CONFIGURED) {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  status_t ret = mNode->SetupBuffers(
      (mBufferPoolSize > 0) ? mBufferPoolSize : STREAM_NODE_BUFFERS, false,
      mType, &mBuffers);
  CheckError(ret != NO_ERROR, ret, "@%s SetupBuffers failed", __FUNCTION__);
  for (auto& it : mBuffers) {
    if (it.Length(0) != mFormat.SizeImage(0)) {
      LOGW("@%s incosistent size (%zu vs %zu)", __FUNCTION__, it.Length(0),
           mFormat.SizeImage(0));
    }

    if (mType == V4L2_MEMORY_MMAP) {
      std::vector<void*> mapped;
      ret = mNode->MapMemory(it.Index(), PROT_READ | PROT_WRITE, MAP_SHARED,
                             &mapped);
      CheckError(ret != NO_ERROR || mapped.size() > 1, ret, "MapMemory failed");
      LOGI("MapMemory: idx: %d, addr: %p", it.Index(), mapped[0]);
      mMappAddrs.insert(std::make_pair(it.Index(), mapped[0]));

      std::vector<int> fds;
      ret = mNode->ExportFrame(it.Index(), &fds);
      CheckError(ret != NO_ERROR || fds.size() > 1, ret, "ExportFrame failed");
      LOGI("ExportFrame: idx: %d, fd: %d", it.Index(), fds[0]);

      mFds.insert(std::make_pair(it.Index(), fds[0]));
    }
    mFreeBuffers.insert(std::make_pair(it.Index(), &it));
    LOGI("SetupBuffers: idx: %d, vb: %p", it.Index(), &it);
  }
  mState = StreamNodeState::PREPARED;
  return NO_ERROR;
}

status_t V4L2StreamNode::startLocked() {
  LOGD("+");
  if (mState != StreamNodeState::PREPARED) {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  status_t ret = mNode->Start();
  CheckError(ret != NO_ERROR, ret, "Start failed");
  mState = StreamNodeState::STARTED;
  return NO_ERROR;
}

status_t V4L2StreamNode::stopLocked() {
  LOGD("+");
  if (mState != StreamNodeState::PREPARED &&
      mState != StreamNodeState::STARTED) {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  status_t ret = mNode->Stop();
  CheckError(ret != NO_ERROR, ret, "Stop failed");
  mState = StreamNodeState::STOPED;
  return NO_ERROR;
}

status_t V4L2StreamNode::setTransformLocked(BufInfo const& buf) {
  int rotation, flip;
  v4l2_queryctrl queryctrl = {};
  int transform = buf.mTransform;

  switch (transform) {
#define TransCase(iTrans, iRot, iFlip) \
  case (iTrans):                       \
    rotation = (iRot);                 \
    flip = (iFlip);                    \
    break;
    TransCase(0, 0, 0) TransCase(eTransform_FLIP_H, 0, 1)
        TransCase(eTransform_FLIP_V, 180, 1) TransCase(eTransform_ROT_90, 90, 0)
            TransCase(eTransform_ROT_180, 180, 0)
                TransCase(eTransform_FLIP_H | eTransform_ROT_90, 270, 1)
                    TransCase(eTransform_FLIP_V | eTransform_ROT_90, 90, 1)
                        TransCase(eTransform_ROT_270, 270, 0) default
        : LOGE("not supported transform(0x%x)", transform);
    rotation = 0;
    flip = 0;
    return BAD_VALUE;
#undef TransCase
  }

  queryctrl.id = V4L2_CID_ROTATE;

  status_t ret = mNode->QueryControl(&queryctrl);
  if (ret != NO_ERROR || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    LOGI("@%s QueryControl failed", __FUNCTION__);
  } else {
    ret = mNode->GetControl(V4L2_CID_ROTATE, &mTransform);
    if (ret == NO_ERROR && mTransform != rotation) {
      ret = mNode->SetControl(V4L2_CID_ROTATE, rotation);
      CheckError(ret != NO_ERROR, ret, "@%s SetControl:Rotate failed",
                 __FUNCTION__);
      mTransform = transform;
    }
  }
  return NO_ERROR;
}

bool V4L2StreamNode::isStart() {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");
  return (mState == StreamNodeState::STARTED);
}

bool V4L2StreamNode::isPrepared() {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");
  return (mState == StreamNodeState::PREPARED);
}

bool V4L2StreamNode::isActive() {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");
  return mActive;
}

status_t V4L2StreamNode::start() {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");

  if (mState == StreamNodeState::PREPARED) {
    CheckError(startLocked() != NO_ERROR, -EINVAL, "start failed");
  } else {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  LOGD("-");
  return NO_ERROR;
}

status_t V4L2StreamNode::stop() {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");

  if ((mState == StreamNodeState::PREPARED) ||
      (mState == StreamNodeState::STARTED)) {
    CheckError(stopLocked() != NO_ERROR, -EINVAL, "stop failed");
  } else {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  LOGD("-");
  return NO_ERROR;
}

status_t V4L2StreamNode::setFormatAnGetdBuffers(
    IImageBufferAllocator::ImgParam* imgParam,
    std::vector<std::shared_ptr<IImageBuffer> >* buffers) {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");

  if (mState == StreamNodeState::STARTED) {
    LOGE("Invalid device state");
    return -EINVAL;
  }
  CheckError(!buffers->empty(), -EINVAL, "invalid buffers");

  if (mState == StreamNodeState::OPEN) {
    CheckError(setFormatLocked(imgParam) != NO_ERROR, -EINVAL,
               "setFormat failed");
  }
  if (mState == StreamNodeState::CONFIGURED) {
    mType = V4L2_MEMORY_MMAP;
    CheckError(setupBuffersLocked() != NO_ERROR, -EINVAL,
               "setupBuffers failed");
  }

  for (auto& it : mBuffers) {
    auto fd = mFds.find(it.Index());
    auto va = mMappAddrs.find(it.Index());
    if (fd == mFds.end() || va == mMappAddrs.end()) {
      LOGE("@%s search failed", __FUNCTION__);
      return -EINVAL;
    }
    PortBufInfo_v1 port =
        PortBufInfo_v1(fd->second, (uintptr_t)va->second, 0, 0, 0);
    std::shared_ptr<NSCam::ImageBufferHeap> heap =
        ImageBufferHeap::create(LOG_TAG, *imgParam, port);
    CheckError(heap == nullptr, -EINVAL, "create heap failed");

    if (imgParam->imgFormat != eImgFmt_BLOB) {
      std::shared_ptr<IImageBuffer> buffer = heap->createImageBuffer();
      CheckError(buffer == nullptr, -EINVAL, "create image buffer failed");
      if (mType == V4L2_MEMORY_MMAP) {
        mMMAPedImages[buffer.get()] = &it;
        LOGD("MMAP relationship (vb2 index, IImageBuffer)=(%d, %p)", it.Index(),
             buffer.get());
      }
      buffers->push_back(std::move(buffer));
    } else {
      std::shared_ptr<IImageBuffer> buffer =
          heap->createImageBuffer_FromBlobHeap(0, imgParam->bufSize);
      CheckError(buffer == nullptr, -EINVAL, "create meta buffer failed");
      if (mType == V4L2_MEMORY_MMAP) {
        mMMAPedImages[buffer.get()] = &it;
        LOGD("MMAP relationship (vb2 index, IImageBuffer)=(%d, %p)", it.Index(),
             buffer.get());
      }
      buffers->push_back(std::move(buffer));
    }
  }
  return NO_ERROR;
}

status_t V4L2StreamNode::setBufFormat(
    IImageBufferAllocator::ImgParam* imgParam) {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");

  if (mState == StreamNodeState::OPEN) {
    CheckError(setFormatLocked(imgParam) != NO_ERROR, -EINVAL,
               "setFormat failed");
  } else {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  LOGD("-");
  return NO_ERROR;
}

status_t V4L2StreamNode::setupBuffers() {
  LOGD("+");
  std::lock_guard<std::mutex> _l(mOpLock);

  if (mState == StreamNodeState::CONFIGURED) {
    CheckError(setupBuffersLocked() != NO_ERROR, -EINVAL,
               "setupBuffers failed");
  } else {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  LOGD("-");
  return NO_ERROR;
}

status_t V4L2StreamNode::setBufPoolSize(int size) {
  std::lock_guard<std::mutex> _l(mOpLock);
  LOGD("+");
  if (mState == StreamNodeState::OPEN ||
      mState == StreamNodeState::CONFIGURED) {
    mBufferPoolSize = size;
  } else {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  LOGD("-");
  return NO_ERROR;
}

status_t V4L2StreamNode::setActive(bool active, bool* changed) {
  std::lock_guard<std::mutex> _l(mOpLock);

  if (mActive != active) {
    *changed = true;
    mActive = active;
  } else {
    *changed = false;
  }
  return NO_ERROR;
}

status_t V4L2StreamNode::enque(BufInfo const& buf,
                               bool lazystartLocked,
                               std::shared_ptr<V4L2Subdevice> sub_device) {
  LOGD("+");
  V4L2Buffer* vb = nullptr;
  IImageBuffer* const& b = buf.mBuffer;
  if (b == nullptr) {
    LOGE("invalid buffer");
    return -EFAULT;
  }

  std::lock_guard<std::mutex> _l(mOpLock);

  if (mState == StreamNodeState::OPEN) {
    MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
    MUINT32 bufStridesInBytes[3] = {0, 0, 0};
    for (int i = 0; i < b->getPlaneCount(); ++i) {
      bufStridesInBytes[i] = b->getBufStridesInBytes(i);
      MY_LOGI("plane %d stride %u", i, bufStridesInBytes[i]);
    }

    IImageBufferAllocator::ImgParam imgParam(
        b->getImgFormat(), b->getImgSize(), bufStridesInBytes,
        bufBoundaryInBytes, b->getPlaneCount());

    CheckError(setFormatLocked(&imgParam) != NO_ERROR, -EINVAL,
               "setFormat failed");
    // should not use incomplete v4l2_fmt
    if (mFormat.SizeImage(0) != b->getBufSizeInBytes(0)) {
      LOGW("@%s incosistent size (%zu vs %zu)", __FUNCTION__,
           mFormat.SizeImage(0), b->getBufSizeInBytes(0));
    }
  }
  if (mState == StreamNodeState::CONFIGURED) {
    CheckError(setupBuffersLocked() != NO_ERROR, -EINVAL,
               "setupBuffers failed");
  }
  if ((!lazystartLocked) && (mState == StreamNodeState::PREPARED)) {
    CheckError(startLocked() != NO_ERROR, -EINVAL, "start failed");
  }

  /* since MMAP buffer doesn't support dynamic change buffer, the buffer
   * must be the one requested from driver */
  if (mType == V4L2_MEMORY_MMAP) {
    /* find out vb2 index by IImageBuffer */
    auto el = mMMAPedImages.find(b);
    if (CC_UNLIKELY(el == mMMAPedImages.end())) {
      LOGE(
          "Not found related vb2 index by IImageBuffer* %p, check the "
          "given IImageBuffer* is valid or not",
          b);
      return -EFAULT;
    }

    vb = el->second;
    if (CC_UNLIKELY(vb == nullptr)) {
      LOGE("Unexpected error, IImageBuffer*(%p) indicated to a null pointer",
           el->first);
      return -EFAULT;
    }
    mImageBuffers.insert(std::make_pair(vb->Index(), b));
    mV4L2Buffers.insert(std::make_pair(vb->Index(), vb));

    mUsedBuffers.insert(std::make_pair(vb->Index(), vb));
    mFreeBuffers.erase(vb->Index());
  } else {
    auto it = mImageBuffers.begin();
    for (; it != mImageBuffers.end(); ++it) {
      if (it->second != nullptr && it->second->getFD() == b->getFD()) {
        auto search = mV4L2Buffers.find(it->first);
        CheckError(search == mV4L2Buffers.end(), -EINVAL, "search failed");
        vb = search->second;
        LOGI("Found in ImageBuffers Maps, idx: %d, vb->img(fd): %p->%p(%d)",
             it->first, vb, b, b->getFD());
        break;
      }
    }
    if (it == mImageBuffers.end()) {
      bool match = false;
      for (auto jt = mBuffers.begin(); jt != mBuffers.end(); ++jt) {
        auto search = mV4L2Buffers.find(jt->Index());
        if (search == mV4L2Buffers.end()) {
          vb = &(*jt);
          LOGD("%s fmt %x %dx%d", mName.c_str(), b->getImgFormat(),
               b->getImgSize().w, b->getImgSize().h);
          for (int i = 0; i < b->getPlaneCount(); i++) {
            if (!IS_MULTIPLANAR(mFormat.PixelFormat()) && i > 0) {
              break;
            }
            switch (mType) {
              case V4L2_MEMORY_DMABUF:
                vb->SetFd(b->getFD(i), i);
                LOGD("plane %d Set Fd %d Offset %lld", i, vb->Fd(i),
                     b->getImageBufferHeap()->getBufOffsetInBytes(i));
                vb->SetDataOffset(
                    b->getImageBufferHeap()->getBufOffsetInBytes(i), i);
                vb->SetLength(mFormat.SizeImage(i) + vb->DataOffset(i), i);
                break;
              case V4L2_MEMORY_USERPTR:
                vb->SetUserptr(b->getBufVA(i), i);
                LOGI("Set Userptr %u", vb->Userptr(i));
                break;
              default:
                LOGE("wrong buffer type:%d for enque", mType);
                return -EINVAL;
            }
          }
          mImageBuffers.insert(std::make_pair(vb->Index(), b));
          mV4L2Buffers.insert(std::make_pair(vb->Index(), vb));
          LOGI("Cache from v4l2 buffer pool, idx: %d, vb->img(fd): %p->%p(%d)",
               vb->Index(), vb, b, b->getFD());
          match = true;
          break;
        }
      }
      if (match == false) {
        if (CC_LIKELY(mType == V4L2_MEMORY_DMABUF)) {
          if (mFreeBuffers.empty()) {
            LOGE("no available buffer for replacement");
            return -ENOTEMPTY;
          }
          // pick up first free buffer for replacement
          auto it = mFreeBuffers.begin();
          auto search_img = mImageBuffers.find(it->first);
          auto search_v4l2 = mV4L2Buffers.find(it->first);
          if (CC_UNLIKELY((search_img == mImageBuffers.end()) ||
                          (search_v4l2 == mV4L2Buffers.end()))) {
            LOGE("free buffer map fail, cancel buffer replacement flow");
            return -EFAULT;
          }
          LOGD("execute replace buffer flow");
          vb = search_v4l2->second;
          for (int i = 0; i < b->getPlaneCount(); i++) {
            if (!IS_MULTIPLANAR(mFormat.PixelFormat()) && i > 0) {
              break;
            }
            LOGD("plane %d Set Fd %d Offset %lld", i, vb->Fd(i),
                 b->getImageBufferHeap()->getBufOffsetInBytes(i));
            vb->SetFd(b->getFD(i), i);
            vb->SetDataOffset(
                b->getImageBufferHeap()->getBufOffsetInBytes(i), i);
            vb->SetLength(mFormat.SizeImage(i) + vb->DataOffset(i), i);
          }
          search_img->second = b;
        } else {
          LOGE("search failed");
          return -EINVAL;
        }
      }
    }
    mUsedBuffers.insert(std::make_pair(vb->Index(), vb));
    mFreeBuffers.erase(vb->Index());
  }

  /*
   * if buf.mRequestFD was <= 0, it means this v4l2_buffer is without
   * request api.
   */
  if (buf.mRequestFD <= 0) {
    vb->ResetRequestFd();
  } else {
    vb->SetRequestFd(buf.mRequestFD);
  }
  vb->SetBytesUsed(vb->Length(0), 0);

  /*set selection*/
  if ((sub_device != nullptr) &&
      (buf.FrameBased.mCropRect.w_fractional == PAD_MDP0)) {
    struct v4l2_subdev_selection crop = {};
    if (strstr(mName.c_str(), "MDP0"))
      crop.pad = PAD_MDP0;
    else if (strstr(mName.c_str(), "MDP1"))
      crop.pad = PAD_MDP1;
    else
      crop.pad = PAD_INVALID;

    if (crop.pad != PAD_INVALID) {
      crop.which = V4L2_SUBDEV_FORMAT_ACTIVE, crop.target = V4L2_SEL_TGT_CROP;
      crop.flags = V4L2_SEL_FLAG_LE;
      crop.r.width = buf.FrameBased.mCropRect.s.w;
      crop.r.height = buf.FrameBased.mCropRect.s.h;
      crop.r.left = buf.FrameBased.mCropRect.p_integral.x;
      crop.r.top = buf.FrameBased.mCropRect.p_integral.y;

      if (sub_device->SetSelection(crop) != NO_ERROR)
        LOGW("Sub-device set selection failed. Output without selection.");
    }
  }

  // set rotation
  if (strstr(mName.c_str(), "MDP0"))
    if (setTransformLocked(buf) != NO_ERROR)
      LOGW("Set buffer rotation by setTtraensform() failed. (%d -> %d)",
           mTransform, buf.mTransform);

  // enque
  status_t ret = mNode->PutFrame(vb);
  CheckError(ret != NO_ERROR, ret, "PutFrame failed");
  LOGD(
      "put port=%d, name=%s, vb(index=%d, magic_num=%d, request_api=%#x) "
      "imgbuf(fd=%d, %p)",
      buf.mPortID.index, mName.c_str(), vb->Index(), buf.magic_num,
      buf.mRequestFD, vb->Fd(0), b);

  return NO_ERROR;
}

status_t V4L2StreamNode::deque(BufInfo* p_buf) {
  std::lock_guard<std::mutex> _l(mOpLock);
  BufInfo& buf = *p_buf;
  if (mState != StreamNodeState::STARTED) {
    LOGE("Invalid device state");
    return -EINVAL;
  }

  V4L2Buffer vb;
  timeval time;
  IImageBuffer*& buffer = buf.mBuffer;
  status_t ret = mNode->GrabFrame(&vb);
  CheckError(ret < 0, ret, "GrabFrame failed");

  auto v = mUsedBuffers.find(vb.Index());
  CheckError(v == mUsedBuffers.end(), ret, "search failed");
  mFreeBuffers.insert(std::make_pair(vb.Index(), v->second));
  mUsedBuffers.erase(vb.Index());

  auto b = mImageBuffers.find(vb.Index());
  CheckError(b == mImageBuffers.end(), ret, "search failed");
  buffer = b->second;

  mImageBuffers.erase(vb.Index());
  mV4L2Buffers.erase(vb.Index());

  /* update v4l2::BufInfo */
  buf.magic_num = 0;
  buf.mSequenceNum = vb.Sequence();
  buf.mRequestFD = vb.RequestFd();
  time = vb.Timestamp();
  buf.timestamp = (time.tv_sec * (int64_t)1000000000) + (time.tv_usec * 1000);
  buf.mSize = mFormat.SizeImage(0);
  LOGD(
      "deque success, port=%d, vb(index=%d, sequence=%d, request_api=%#x) "
      "imgbuf(fd=%d, %p, size=%d),%lds:%ldus",
      buf.mPortID.index, vb.Index(), buf.mSequenceNum, buf.mRequestFD, vb.Fd(0),
      buffer, buf.mSize, time.tv_sec, time.tv_usec);

  return NO_ERROR;
}

}  // namespace v4l2
}  // namespace NSCam
