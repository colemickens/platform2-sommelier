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

#include <common/include/DebugControl.h>

#define PIPE_CLASS_TAG "MDPNode"
#define PIPE_TRACE TRACE_MDP_NODE
#include <capture/nodes/MDPNode.h>
#include <common/include/PipeLog.h>

#include <memory>
#include <mtkcam/utils/exif/DebugExifUtils.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#include "base/strings/stringprintf.h"
#include <string>

using NSCam::TuningUtils::FILE_DUMP_NAMING_HINT;
using NSCam::TuningUtils::YUV_PORT_UNDEFINED;

#define FD_TOLERENCE (600000000)
#define VIDEO_DEV_NAME "/dev/video"
#define MTK_MDP_DRIVER_NAME "mtk-mdp3"
#define DEBUG_MDP_CAP

static int xioctl(int fd, int request, void* arg) {
  int r;

  do {
    r = ioctl(fd, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}

static int Format2PlaneFmt(unsigned int fmt, unsigned int* nplanes) {
  if (fmt == V4L2_PIX_FMT_NV12 || fmt == V4L2_PIX_FMT_NV21 ||
      fmt == V4L2_PIX_FMT_NV16 || fmt == V4L2_PIX_FMT_YVU420 ||
      fmt == V4L2_PIX_FMT_YUV420 || fmt == V4L2_PIX_FMT_YUV422P ||
      fmt == V4L2_PIX_FMT_YUYV || fmt == V4L2_PIX_FMT_UYVY ||
      fmt == V4L2_PIX_FMT_YVYU || fmt == V4L2_PIX_FMT_VYUY ||
      fmt == V4L2_PIX_FMT_RGB565 || fmt == V4L2_PIX_FMT_RGB24 ||
      fmt == V4L2_PIX_FMT_BGR24 || fmt == V4L2_PIX_FMT_ARGB32 ||
      fmt == V4L2_PIX_FMT_ABGR32 || fmt == V4L2_PIX_FMT_XRGB32 ||
      fmt == V4L2_PIX_FMT_XBGR32) {
    *nplanes = 1;
  } else if (fmt == V4L2_PIX_FMT_NV12M || fmt == V4L2_PIX_FMT_NV16M ||
             fmt == V4L2_PIX_FMT_NV21M) {
    *nplanes = 2;
  } else if (fmt == V4L2_PIX_FMT_YUV420M || fmt == V4L2_PIX_FMT_YVU420M) {
    *nplanes = 3;
  }

  return 0;
}

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

MDPNode::MDPNode(NodeID_T nid, const char* name)
    : CaptureFeatureNode(nid, name) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mRequests);
  mDebugDump = property_get_int32("vendor.debug.camera.p2.dump", 0) > 0;
  mM2MMdpDump =
      property_get_int32("vendor.debug.camera.p2.m2m.mdp.dump", 0) > 0;
  TRACE_FUNC_EXIT();
}

MDPNode::~MDPNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL MDPNode::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  MY_LOGD_IF(mLogLevel, "Frame %d: %s arrived", pRequest->getRequestNo(),
             PathID2Name(id));
  mRequests.enque(pRequest);
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL MDPNode::onInit() {
  TRACE_FUNC_ENTER();
  CaptureFeatureNode::onInit();
  char dev_name[64];
  int i = 0;
  int fd = -1;
  struct v4l2_capability& cap = mV4l2MdpInfo.v4l2_cap;
  for (i = 0; i < 64; i++) {
    snprintf(dev_name, sizeof(dev_name), "%s%d", VIDEO_DEV_NAME, i);
    fd = open(dev_name, O_RDWR, 0);
    if (-1 == fd) {
      continue;
    } else {
      memset(&cap, 0, sizeof(cap));
      if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        MY_LOGE("query mdp device capability fail");
        return MFALSE;
      } else {
        if (strcmp(reinterpret_cast<char*>(cap.driver), MTK_MDP_DRIVER_NAME)) {
          close(fd);
          continue;
        }
        MY_LOGD("video %d", i);
#ifdef DEBUG_MDP_CAP
        MY_LOGD("\nVIDOOC_QUERYCAP");
        MY_LOGD("the mdp driver is %s", cap.driver);
        MY_LOGD("the mdp card is %s", cap.card);
        MY_LOGD("the mdp bus info is %s", cap.bus_info);
        MY_LOGD("the version is %d", cap.version);
        MY_LOGD("the capabilities is %u (%x)", cap.capabilities,
                cap.device_caps);
#endif
        mV4l2MdpInfo.device_name = dev_name;
        mV4l2MdpInfo.fd = fd;
        break;
      }
    }
  }
  if (i >= 64) {
    return MFALSE;
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL MDPNode::onUninit() {
  TRACE_FUNC_ENTER();
  if (-1 == close(mV4l2MdpInfo.fd)) {
    MY_LOGE("error : close mdp failed!");
    return MFALSE;
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL MDPNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL MDPNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MUINT32 MDPNode::formatTrans(MUINT32 format) {
  MUINT32 transFormat;
  switch (format) {
    case eImgFmt_YUY2:
      transFormat = V4L2_PIX_FMT_YUYV;
      break;
    case eImgFmt_YVYU:
      transFormat = V4L2_PIX_FMT_YVYU;
      break;
    case eImgFmt_UYVY:
      transFormat = V4L2_PIX_FMT_UYVY;
      break;
    case eImgFmt_VYUY:
      transFormat = V4L2_PIX_FMT_VYUY;
      break;
    case eImgFmt_NV12:
      transFormat = V4L2_PIX_FMT_NV12;
      break;
    case eImgFmt_NV21:
      transFormat = V4L2_PIX_FMT_NV21;
      break;
    case eImgFmt_YV12:
      transFormat = V4L2_PIX_FMT_YVU420;
      break;
  }
  return transFormat;
}

int MDPNode::rotTrans(int transform) {
  int rotate;
  switch (transform) {
    case HAL_TRANSFORM_ROT_90:
      rotate = 90;
      break;
    case HAL_TRANSFORM_ROT_180:
      rotate = 180;
      break;
    case HAL_TRANSFORM_ROT_270:
      rotate = 270;
      break;
    default:
      rotate = 0;
  }
  return rotate;
}

MUINT32 MDPNode::calBytesUsed(int w, int h, MUINT32 format) {
  float gain = 2;
  switch (format) {
    case eImgFmt_YUY2:
    case eImgFmt_YVYU:
    case eImgFmt_UYVY:
    case eImgFmt_VYUY:
      gain = 2;
      break;
    case eImgFmt_NV12:
    case eImgFmt_NV21:
    case eImgFmt_YV12:
      gain = 1.5;
      break;
  }
  return w * h * gain;
}

void MDPNode::displayPlaneInfo(struct v4l2_plane_pix_format* plane_fmt,
                               unsigned int planes) {
  MY_LOGD("plane_fmt(%d):", planes);
  for (int i = 0; i < planes; i++) {
    MY_LOGD("%d: bytesperline=%u, sizeimage=%u", i, plane_fmt[i].bytesperline,
            plane_fmt[i].sizeimage);
  }
}

/* Only use MPLANE */
int MDPNode::displayFormat(struct v4l2_format* fmt) {
  struct v4l2_pix_format_mplane* pix_mp = &fmt->fmt.pix_mp;
  int i;

  MY_LOGD(
      "FMT: %dx%d, fourcc=%c%c%c%c (0x%08x), field=0x%x, colorspace=0x%08x, "
      "num_planes=%d, ",
      pix_mp->width, pix_mp->height, pix_mp->pixelformat & 0xff,
      (pix_mp->pixelformat >> 8) & 0xff, (pix_mp->pixelformat >> 16) & 0xff,
      (pix_mp->pixelformat >> 24) & 0xff, pix_mp->pixelformat, pix_mp->field,
      pix_mp->colorspace, pix_mp->num_planes);

  for (i = 0; i < pix_mp->num_planes; i++) {
    displayPlaneInfo(pix_mp->plane_fmt, pix_mp->num_planes);
  }

  return 0;
}

int MDPNode::createInputBuffers(IImageBuffer* buffer, MRect crop) {
  struct v4l2_plane planes[VIDEO_MAX_PLANES];
  struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];
  struct v4l2_format format;
  struct v4l2_selection sel;
  struct v4l2_requestbuffers reqbufs;

  MY_LOGD("Create input buffers start");

  MUINT32 width = buffer->getImgSize().w;
  MUINT32 height = buffer->getImgSize().h;
  MUINT32 pixelformat = formatTrans(buffer->getImgFormat());
  Format2PlaneFmt(pixelformat, &mV4l2MdpInfo.inBufferInfo.planes_num);
  unsigned int nplanes = mV4l2MdpInfo.inBufferInfo.planes_num;

  memset(plane_fmt, 0, sizeof(plane_fmt));
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = width;
  format.fmt.pix_mp.height = height;
  format.fmt.pix_mp.pixelformat = pixelformat;
  format.fmt.pix_mp.num_planes = nplanes;
  for (int i = 0; i < nplanes; ++i) {
    format.fmt.pix_mp.plane_fmt[i].sizeimage = buffer->getBufSizeInBytes(i);
    format.fmt.pix_mp.plane_fmt[i].bytesperline =
        buffer->getBufStridesInBytes(i);
  }
  displayFormat(&format);

  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_S_FMT, &format) < 0) {
    MY_LOGE("ioctl set input VIDIOC_S_FMT fail");
  }
  displayFormat(&format);

  memset(&sel, 0, sizeof(sel));
  sel.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  sel.target = V4L2_SEL_TGT_CROP;
  sel.r.left = crop.p.x;
  sel.r.top = crop.p.y;
  sel.r.width = crop.s.w;
  sel.r.height = crop.s.h;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_S_SELECTION, &sel) < 0) {
    MY_LOGE("ioctl set input VIDIOC_S_SELECTION fail");
  }

  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 1;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
    MY_LOGE("ioctl set input VIDIOC_REQBUFS fail");
  }
  MY_LOGD("request input buffer count %d", reqbufs.count);
  for (int i = 0; i < reqbufs.count; i++) {
    v4l2_buffer& buffer = mV4l2MdpInfo.mdpInBuffer;
    PlanesInfo& in = mV4l2MdpInfo.inBufferInfo;
    memset(&(buffer), 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.index = i;
    buffer.m.planes = planes;
    buffer.length = nplanes;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_QUERYBUF, &buffer) < 0) {
      MY_LOGE("ioctl set input VIDIOC_QUERYBUF fail");
    }
    in.data_offset = buffer.m.planes[0].data_offset;
    in.length = buffer.m.planes[0].length;
  }
  return OK;
}

int MDPNode::createOutputBuffers(IImageBuffer* buffer, int rotate, MRect crop) {
  struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];
  struct v4l2_control control;
  struct v4l2_format format;
  struct v4l2_selection sel;
  struct v4l2_requestbuffers reqbufs;
  struct v4l2_plane planes[VIDEO_MAX_PLANES];

  MY_LOGD("Create output buffers start");

  MUINT32 width = buffer->getImgSize().w;
  MUINT32 height = buffer->getImgSize().h;
  MUINT32 pixelformat = formatTrans(buffer->getImgFormat());
  Format2PlaneFmt(pixelformat, &mV4l2MdpInfo.outBufferInfo.planes_num);
  unsigned int nplanes = mV4l2MdpInfo.outBufferInfo.planes_num;

  memset(plane_fmt, 0, sizeof(plane_fmt));
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = width;
  format.fmt.pix_mp.height = height;
  format.fmt.pix_mp.pixelformat = pixelformat;
  format.fmt.pix_mp.num_planes = nplanes;
  for (int i = 0; i < nplanes; ++i) {
    format.fmt.pix_mp.plane_fmt[i].sizeimage = buffer->getBufSizeInBytes(i);
    format.fmt.pix_mp.plane_fmt[i].bytesperline =
        buffer->getBufStridesInBytes(i);
  }

  displayFormat(&format);
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_S_FMT, &format) < 0) {
    MY_LOGE("ioctl set output VIDIOC_REQBUFS fail");
  }
  displayFormat(&format);

  memset(&sel, 0, sizeof(sel));
  sel.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  sel.target = V4L2_SEL_TGT_COMPOSE;
  sel.r.left = 0;
  sel.r.top = 0;
  sel.r.width = width;
  sel.r.height = height;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_S_SELECTION, &sel) < 0) {
    MY_LOGE("ioctl set input VIDIOC_S_SELECTION fail");
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ROTATE;
  control.value = rotate;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_S_CTRL, &control) < 0) {
    MY_LOGE("ioctl set V4L2_CID_ROTATE fail");
  }

  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 1;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
    MY_LOGE("ioctl set output VIDIOC_REQBUFS fail");
  }
  MY_LOGD("request output buffer count %d", reqbufs.count);
  for (int i = 0; i < reqbufs.count; i++) {
    v4l2_buffer& buffer = mV4l2MdpInfo.mdpOutBuffer;
    PlanesInfo& out = mV4l2MdpInfo.outBufferInfo;
    memset(&(buffer), 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.index = i;
    buffer.m.planes = planes;
    buffer.length = nplanes;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_QUERYBUF, &buffer) < 0) {
      MY_LOGE("ioctl set output VIDIOC_QUERYBUF fail");
    }
    out.data_offset = buffer.m.planes[0].data_offset;
    out.length = buffer.m.planes[0].length;
  }

  return OK;
}

MBOOL MDPNode::runMDPDeque() {
  TRACE_FUNC("runMDPDeque+++");
  struct v4l2_buffer outBuf;
  struct v4l2_plane out_planes[VIDEO_MAX_PLANES];
  memset(&outBuf, 0, sizeof(outBuf));
  memset(&out_planes, 0, sizeof(out_planes));
  outBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  outBuf.memory = V4L2_MEMORY_DMABUF;
  outBuf.m.planes = out_planes;
  outBuf.length = mV4l2MdpInfo.inBufferInfo.planes_num;

  struct v4l2_buffer capBuf;
  struct v4l2_plane cap_planes[VIDEO_MAX_PLANES];
  memset(&capBuf, 0, sizeof(capBuf));
  memset(&cap_planes, 0, sizeof(cap_planes));
  capBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  capBuf.memory = V4L2_MEMORY_DMABUF;
  capBuf.m.planes = cap_planes;
  capBuf.length = mV4l2MdpInfo.outBufferInfo.planes_num;

  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_DQBUF, &outBuf) < 0) {
    MY_LOGE("ioctl out VIDIOC_DQBUF fail");
  }
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_DQBUF, &capBuf) < 0) {
    MY_LOGE("ioctl cap VIDIOC_DQBUF fail");
  }
  TRACE_FUNC("runMDPDeque---");
  return MTRUE;
}

MVOID MDPNode::releaseV4l2Buffer() {
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
    MY_LOGE("release input v4l2 buffer fail");
  }

  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (xioctl(mV4l2MdpInfo.fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
    MY_LOGE("release output v4l2 buffer fail");
  }
}

MBOOL MDPNode::onThreadLoop() {
  TRACE_FUNC("Waitloop");
  RequestPtr pRequest;

  if (!waitAllQueue()) {
    return MFALSE;
  }

  if (!mRequests.deque(&pRequest)) {
    MY_LOGE("Request deque out of sync");
    return MFALSE;
  } else if (pRequest == NULL) {
    MY_LOGE("Request out of sync");
    return MFALSE;
  }
  TRACE_FUNC_ENTER();

  incExtThreadDependency();
  pRequest->mTimer.startMDP();
  TRACE_FUNC("Frame %d in MDP", pRequest->getRequestNo());
  onRequestProcess(pRequest);
  pRequest->mTimer.stopMDP();

  onRequestFinish(pRequest);
  decExtThreadDependency();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL MDPNode::onRequestProcess(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  CAM_TRACE_FMT_BEGIN("mdp:process|r%df%d", requestNo, frameNo);
  MY_LOGD("+, R/F Num: %d/%d", requestNo, frameNo);

  MBOOL ret = MTRUE;
  MINT32 iMagicNo;

  mBufferItems.clear();

  auto pNodeReq = pRequest->getNodeRequest(NID_MDP);

  if (pNodeReq == nullptr) {
    return MFALSE;
  }

  IMetadata* pInHalMeta = pNodeReq->acquireMetadata(MID_MAIN_IN_HAL);
  IMetadata* pInAppMeta = pNodeReq->acquireMetadata(MID_MAIN_IN_APP);

  if (pInHalMeta != nullptr) {
    tryGetMetadata<MINT32>(pInHalMeta, MTK_P1NODE_PROCESSOR_MAGICNUM,
                           &iMagicNo);
  }

  // Input
  BufferID_T uIBufFull = pNodeReq->mapBufferID(TID_MAIN_FULL_YUV, INPUT);
  IImageBuffer* pSrcBuffer = pNodeReq->acquireBuffer(uIBufFull);
  if (pSrcBuffer == nullptr) {
    MY_LOGE("no source image!");
    return BAD_VALUE;
  }

  // Output
  std::string str = base::StringPrintf("Resized(%d) R/F/M:%d/%d/%d", 0,
                                       pRequest->getRequestNo(),
                                       pRequest->getFrameNo(), iMagicNo);

  str += base::StringPrintf(
      ", src info: Size(%dx%d), fmt(%d)", pSrcBuffer->getImgSize().w,
      pSrcBuffer->getImgSize().h, pSrcBuffer->getImgFormat());

  std::shared_ptr<CropCalculator::Factor> pFactor =
      mpCropCalculator->getFactor(pInAppMeta, pInHalMeta);

  for (TypeID_T typeId = 0; typeId < NUM_OF_TYPE; typeId++) {
    BufferID_T bufId = pNodeReq->mapBufferID(typeId, OUTPUT);
    if (bufId == NULL_BUFFER) {
      continue;
    }

    IImageBuffer* pDstBuffer = pNodeReq->acquireBuffer(bufId);
    if (pDstBuffer == nullptr) {
      continue;
    }

    mBufferItems.emplace_back();
    BufferItem& item = mBufferItems.back();

    item.mIsCapture = (bufId == BID_MAIN_OUT_JPEG);
    item.mpImageBuffer = pDstBuffer;
    item.mTransform = pNodeReq->getImageTransform(bufId);
    int rotate = rotTrans(item.mTransform);

    MSize dstSize = pDstBuffer->getImgSize();
    if (item.mTransform & eTransform_ROT_90) {
      dstSize = MSize(dstSize.h, dstSize.w);
    }
    MRect& crop = item.mCrop;
    mpCropCalculator->evaluate(pFactor, dstSize, &crop);

    createInputBuffers(pSrcBuffer, crop);

    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_STREAMON, &type) < 0) {
      MY_LOGE("ioctl stream on V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE fail");
    }

    createOutputBuffers(pDstBuffer, rotate, crop);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_STREAMON, &type) < 0) {
      MY_LOGE("ioctl stream on V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE fail");
    }

    struct v4l2_plane qInbuf_planes[VIDEO_MAX_PLANES];
    memset(&qInbuf_planes, 0, sizeof(qInbuf_planes));
    for (size_t i = 0; i < mV4l2MdpInfo.inBufferInfo.planes_num; i++) {
      qInbuf_planes[i].bytesused =
          calBytesUsed(pSrcBuffer->getImgSize().w, pSrcBuffer->getImgSize().h,
                       pSrcBuffer->getImgFormat());
      qInbuf_planes[i].length = mV4l2MdpInfo.inBufferInfo.length;
      qInbuf_planes[i].data_offset =
          pSrcBuffer->getImageBufferHeap()->getBufOffsetInBytes(i);
      qInbuf_planes[i].m.fd = pSrcBuffer->getFD(i);
    }
    mV4l2MdpInfo.mdpInBuffer.m.planes = qInbuf_planes;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_QBUF, &mV4l2MdpInfo.mdpInBuffer) < 0) {
      MY_LOGE("ioctl VIDIOC_QBUF out error");
    }

    struct v4l2_plane qOutbuf_planes[VIDEO_MAX_PLANES];
    memset(&qOutbuf_planes, 0, sizeof(qOutbuf_planes));
    for (size_t i = 0; i < mV4l2MdpInfo.outBufferInfo.planes_num; i++) {
      qOutbuf_planes[i].bytesused = 0;
      qOutbuf_planes[i].length = mV4l2MdpInfo.outBufferInfo.length;
      qOutbuf_planes[i].data_offset =
          pDstBuffer->getImageBufferHeap()->getBufOffsetInBytes(i);
      qOutbuf_planes[i].m.fd = pDstBuffer->getFD(i);
    }
    mV4l2MdpInfo.mdpOutBuffer.m.planes = qOutbuf_planes;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_QBUF, &mV4l2MdpInfo.mdpOutBuffer) < 0) {
      MY_LOGE("ioctl VIDIOC_QBUF cap error");
    }

    runMDPDeque();
    if (pDstBuffer != nullptr && mM2MMdpDump) {
      char filename[256] = {0};
      snprintf(filename, sizeof(filename), "%s/M2M_MDP_out_%d_%d_%d.yuyv",
               DUMP_PATH, requestNo, pDstBuffer->getImgSize().w,
               pDstBuffer->getImgSize().h);
      pDstBuffer->saveToFile(filename);
    }
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_STREAMOFF, &type) < 0) {
      MY_LOGE("ioctl stream off V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE fail");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(mV4l2MdpInfo.fd, VIDIOC_STREAMOFF, &type) < 0) {
      MY_LOGE("ioctl stream off V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE fail");
    }

    releaseV4l2Buffer();

    str += base::StringPrintf(
        ", dst info: Type(%s) Rot(%d) Crop(%d,%d)(%dx%d) Size(%dx%d) fmt(%d) "
        "Cap(%d)",
        TypeID2Name(typeId), item.mTransform, crop.p.x, crop.p.y, crop.s.w,
        crop.s.h, pDstBuffer->getImgSize().w, pDstBuffer->getImgSize().h,
        pDstBuffer->getImgFormat(), item.mIsCapture);
  }

  MY_LOGD("%s", str.c_str());

  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
  CAM_TRACE_FMT_END();
  return ret;
}

MVOID MDPNode::onRequestFinish(RequestPtr& pRequest) {
  pRequest->decNodeReference(NID_MDP);
  MY_LOGD("mdpnode request finish");
  if (mDebugDump) {
    auto pNodeReq = pRequest->getNodeRequest(NID_MDP);

    // Get Unique Key
    MINT32 iUniqueKey = 0;
    IMetadata* pInHalMeta = pNodeReq->acquireMetadata(MID_MAIN_IN_HAL);
    tryGetMetadata<MINT32>(pInHalMeta, MTK_PIPELINE_UNIQUE_KEY, &iUniqueKey);

    char filename[256] = {0};
    FILE_DUMP_NAMING_HINT hint;
    hint.UniqueKey = iUniqueKey;
    hint.RequestNo = pRequest->getRequestNo();
    hint.FrameNo = pRequest->getFrameNo();
    extract_by_SensorOpenId(&hint, mSensorIndex);

    for (TypeID_T typeId = 0; typeId < NUM_OF_TYPE; typeId++) {
      BufferID_T bufId = pNodeReq->mapBufferID(typeId, OUTPUT);
      if (bufId == NULL_BUFFER) {
        continue;
      }

      IImageBuffer* pImgBuf = pNodeReq->acquireBuffer(bufId);
      if (pImgBuf == NULL) {
        continue;
      }

      extract(&hint, pImgBuf);
      genFileName_YUV(filename, sizeof(filename), &hint, YUV_PORT_UNDEFINED);
      pImgBuf->saveToFile(filename);
      MY_LOGD("dump output:%s", filename);
    }
  }

  dispatch(pRequest);
}

MERROR MDPNode::evaluate(CaptureFeatureInferenceData* rInfer) {
  auto& srcData = rInfer->getSharedSrcData();
  auto& dstData = rInfer->getSharedDstData();
  auto& features = rInfer->getSharedFeatures();
  auto& metadatas = rInfer->getSharedMetadatas();

  srcData.emplace_back();
  auto& src_0 = srcData.back();
  src_0.mTypeId = TID_MAIN_FULL_YUV;
  src_0.mSizeId = SID_FULL;

  dstData.emplace_back();
  auto& dst_1 = dstData.back();
  dst_1.mTypeId = TID_MAIN_CROP1_YUV;

  dstData.emplace_back();
  auto& dst_2 = dstData.back();
  dst_2.mTypeId = TID_MAIN_CROP2_YUV;

  dstData.emplace_back();
  auto& dst_3 = dstData.back();
  dst_3.mTypeId = TID_THUMBNAIL;

  dstData.emplace_back();
  auto& dst_4 = dstData.back();
  dst_4.mTypeId = TID_JPEG;

  metadatas.push_back(MID_MAIN_IN_P1_DYNAMIC);
  metadatas.push_back(MID_MAIN_IN_APP);
  metadatas.push_back(MID_MAIN_IN_HAL);

  rInfer->addNodeIO(NID_MDP, srcData, dstData, metadatas, features);

  return OK;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
