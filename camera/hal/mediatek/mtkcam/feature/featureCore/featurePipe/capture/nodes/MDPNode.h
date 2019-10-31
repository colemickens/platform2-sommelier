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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_MDPNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_MDPNODE_H_

#include "../CaptureFeatureNode.h"

#include <mtkcam/utils/hw/IFDContainer.h>
#include <property_lib.h>
#include <string>
#include <vector>

// v4l2 header
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
/* getopt_long() */
#include <getopt.h>
/* low-level i/o */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
/* for videodev2.h */
#include <asm/types.h>
#include <linux/videodev2.h>
/* for pthread*/
#include <pthread.h>
#include <semaphore.h>
#include <sys/prctl.h>

#define NUM_MDP_BUFFER 3

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

struct PlanesInfo {
  MUINT32 length;
  MUINT32 data_offset;
  MUINT32 planes_num;
};

struct V4l2MdpInfo {
  int fd;
  std::string device_name;
  struct v4l2_capability v4l2_cap;
  v4l2_buffer mdpInBuffer;
  PlanesInfo inBufferInfo;
  v4l2_buffer mdpOutBuffer;
  PlanesInfo outBufferInfo;
};

class MDPNode : public CaptureFeatureNode {
 public:
  MDPNode(NodeID_T nid, const char* name);
  virtual ~MDPNode();

 public:
  virtual MBOOL onData(DataID id, const RequestPtr& data);

 public:
  virtual MERROR evaluate(CaptureFeatureInferenceData* rInference);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadStop();
  virtual MBOOL onThreadLoop();

 private:
  virtual MBOOL onRequestProcess(RequestPtr&);
  virtual MVOID onRequestFinish(RequestPtr&);
  MUINT32 formatTrans(MUINT32 format);
  MUINT32 calBytesUsed(int w, int h, MUINT32 format);
  int createInputBuffers(IImageBuffer* buffer, MRect crop);
  int createOutputBuffers(IImageBuffer* buffer, int rotate, MRect crop);
  void displayPlaneInfo(struct v4l2_plane_pix_format* plane_fmt,
                        unsigned int planes);
  int displayFormat(struct v4l2_format* fmt);
  int rotTrans(int transform);
  MBOOL runMDPDeque();
  MVOID releaseV4l2Buffer();

  struct BufferItem {
    IImageBuffer* mpImageBuffer;
    MUINT32 mTransform;
    MRect mCrop;
    MBOOL mIsCapture;
  };
  std::vector<BufferItem> mBufferItems;

 private:
  WaitQueue<RequestPtr> mRequests;
  V4l2MdpInfo mV4l2MdpInfo;
  MBOOL mDebugDump;
  MBOOL mM2MMdpDump;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_MDPNODE_H_
