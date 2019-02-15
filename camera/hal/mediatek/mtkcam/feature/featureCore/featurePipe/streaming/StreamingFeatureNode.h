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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURENODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURENODE_H_

#include <featurePipe/common/include/CamThreadNode.h>
#include <featurePipe/common/include/IOUtil.h>
#include <featurePipe/common/include/SeqUtil.h>
#include "StreamingFeature_Common.h"
#include "StreamingFeatureData.h"
#include "StreamingFeaturePipeUsage.h"
#include "MtkHeader.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
enum StreamingFeatureDataID {
  ID_INVALID,
  ID_ROOT_ENQUE,
  ID_ROOT_TO_P2A,
  ID_ROOT_TO_RSC,
  ID_ROOT_TO_DEPTH,
  ID_P2A_TO_WARP_FULLIMG,
  ID_P2A_TO_EIS_P2DONE,
  ID_P2A_TO_EIS_FM,
  ID_P2A_TO_PMDP,
  ID_P2A_TO_HELPER,
  ID_PMDP_TO_HELPER,
  ID_BOKEH_TO_HELPER,
  ID_WARP_TO_HELPER,
  ID_EIS_TO_WARP,
  ID_P2A_TO_VENDOR_FULLIMG,
  ID_BOKEH_TO_VENDOR_FULLIMG,
  ID_VENDOR_TO_NEXT,
  ID_VMDP_TO_NEXT_FULLIMG,
  ID_VMDP_TO_HELPER,
  ID_RSC_TO_HELPER,
  ID_RSC_TO_EIS,
  ID_PREV_TO_DUMMY_FULLIMG,
  ID_DUMMY_TO_NEXT_FULLIMG,
  ID_DEPTH_TO_BOKEH,
  ID_DEPTH_TO_VENDOR,

  ID_P2A_TO_FOV_FEFM,
  ID_P2A_TO_FOV_FULLIMG,
  ID_P2A_TO_FOV_WARP,
  ID_FOV_TO_FOV_WARP,
  ID_FOV_TO_EIS_WARP,
  ID_FOV_WARP_TO_HELPER,
  ID_FOV_WARP_TO_VENDOR,
  ID_FOV_TO_EIS_FULLIMG,
  ID_P2A_TO_N3DP2,
  ID_N3DP2_TO_N3D,
  ID_N3D_TO_HELPER,
  ID_N3D_TO_VMDP,
  ID_RSC_TO_P2A,
};

class StreamingReqInfo {
 public:
  const MUINT32 mFrameNo;
  const MUINT32 mFeatureMask;
  const MUINT32 mMasterID;
  const MUINT32 mSensorID;
  MVOID makeDebugStr() {
    mStr = base::StringPrintf("No(%d), fmask(0x%08x), sID(%d), masterID(%d)",
                              mFrameNo, mFeatureMask, mSensorID, mMasterID);
  }
  const char* dump() const { return mStr.c_str(); }
  MBOOL isMaster() const { return mMasterID == mSensorID; }

  StreamingReqInfo(MUINT32 fno, MUINT32 mask, MUINT32 mID, MUINT32 sID)
      : mFrameNo(fno), mFeatureMask(mask), mMasterID(mID), mSensorID(sID) {
    makeDebugStr();
  }

 private:
  std::string mStr = std::string("");
};

class NodeSignal {
 public:
  enum Signal {
    SIGNAL_GPU_READY = 0x01 << 0,
  };

  enum Status {
    STATUS_IN_FLUSH = 0x01 << 0,
  };

  NodeSignal();
  virtual ~NodeSignal();
  MVOID setSignal(Signal signal);
  MVOID clearSignal(Signal signal);
  MBOOL getSignal(Signal signal);
  MVOID waitSignal(Signal signal);

  MVOID setStatus(Status status);
  MVOID clearStatus(Status status);
  MBOOL getStatus(Status status);

 private:
  std::mutex mMutex;
  std::condition_variable mCondition;
  MUINT32 mSignal;
  MUINT32 mStatus;
};

class StreamingFeatureDataHandler {
 public:
  typedef StreamingFeatureDataID DataID;

 public:
  virtual ~StreamingFeatureDataHandler();
  virtual MBOOL onData(DataID, const RequestPtr&) { return MFALSE; }
  virtual MBOOL onData(DataID, const ImgBufferData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const FaceData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const FMData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const CBMsgData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const HelperData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const RSCData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const FOVP2AData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const FOVData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const BasicImgData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const N3DData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const DepthImgData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const P2AMDPReqData&) { return MFALSE; }
  virtual MBOOL onData(DataID, const TPIData&) { return MFALSE; }

  static const char* ID2Name(DataID id);

  template <typename T>
  static unsigned getSeq(const T& data) {
    return data.mRequest->mRequestNo;
  }
  static unsigned getSeq(const RequestPtr& data) { return data->mRequestNo; }

  static const bool supportSeq = true;
};

class StreamingFeatureNode : public StreamingFeatureDataHandler,
                             public CamThreadNode<StreamingFeatureDataHandler> {
 public:
  typedef CamGraph<StreamingFeatureNode> Graph_T;
  typedef StreamingFeatureDataHandler Handler_T;

 public:
  explicit StreamingFeatureNode(const char* name);
  virtual ~StreamingFeatureNode();
  MVOID setSensorIndex(MUINT32 sensorIndex);
  MVOID setPipeUsage(const StreamingFeaturePipeUsage& usage);
  MVOID setNodeSignal(const std::shared_ptr<NodeSignal>& nodeSignal);

  virtual IOPolicyType getIOPolicy(StreamType stream,
                                   const StreamingReqInfo& reqInfo) const;
  virtual MBOOL getInputBufferPool(const StreamingReqInfo& reqInfo,
                                   std::shared_ptr<IBufferPool>* pool);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit() { return MTRUE; }
  virtual MBOOL onThreadStart() { return MTRUE; }
  virtual MBOOL onThreadStop() { return MTRUE; }
  virtual MBOOL onThreadLoop() = 0;

  static MBOOL dumpNddData(TuningUtils::FILE_DUMP_NAMING_HINT* hint,
                           IImageBuffer* buffer,
                           MUINT32 portIndex);
  MVOID drawScanLine(IImageBuffer* buffer);
  MVOID printIO(const RequestPtr& request, const QParams& params);
  static MBOOL syncAndDump(const RequestPtr& request,
                           const BasicImg& img,
                           const char* fmt,
                           ...);
  static MBOOL syncAndDump(const RequestPtr& request,
                           const ImgBuffer& img,
                           const char* fmt,
                           ...);
  static MBOOL dumpData(const RequestPtr& request,
                        const ImgBuffer& buffer,
                        const char* fmt,
                        ...);
  static MBOOL dumpData(const RequestPtr& request,
                        const BasicImg& buffer,
                        const char* fmt,
                        ...);
  static MBOOL dumpData(const RequestPtr& request,
                        IImageBuffer* buffer,
                        const char* fmt,
                        ...);
  static MBOOL dumpNamedData(const RequestPtr& request,
                             IImageBuffer* buffer,
                             const char* name);
  static MUINT32 dumpData(const char* buffer,
                          MUINT32 size,
                          const char* filename);
  static MBOOL loadData(IImageBuffer* buffer, const char* filename);
  static MUINT32 loadData(char* buffer, size_t size, const char* filename);

 protected:
  MUINT32 mSensorIndex;
  MINT32 mNodeDebugLV;
  StreamingFeaturePipeUsage mPipeUsage;
  std::shared_ptr<NodeSignal> mNodeSignal;
  std::unique_ptr<DebugScanLine> mDebugScanLine;
};
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURENODE_H_
