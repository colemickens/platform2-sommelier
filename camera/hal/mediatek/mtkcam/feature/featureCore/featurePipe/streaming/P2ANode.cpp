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

#include <featurePipe/common/include/DebugControl.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#define PIPE_CLASS_TAG "P2ANode_2"
#define PIPE_TRACE TRACE_P2A_NODE
#include <featurePipe/common/include/PipeLog.h>
#include <mtkcam/feature/featureCore/featurePipe/streaming/P2ANode.h>
#include "P2CamContext.h"
#include "TuningHelper.h"

using NSCam::NSIoPipe::CrspInfo;
using NSCam::NSIoPipe::EPIPE_IMG3O_CRSPINFO_CMD;
using NSCam::NSIoPipe::Input;
using NSCam::NSIoPipe::ModuleInfo;
using NSCam::NSIoPipe::Output;
using NSCam::NSIoPipe::PORT_IMG3O;
using NSCam::NSIoPipe::PORT_WDMAO;
using NSCam::v4l2::ENormalStreamTag_FE;
using NSCam::v4l2::ENormalStreamTag_FM;
using NSCam::v4l2::ENormalStreamTag_Normal;
using NSCam::v4l2::ENormalStreamTag_Vss;
using NSImageio::NSIspio::EPortIndex_DEPI;  // left
using NSImageio::NSIspio::EPortIndex_DMGI;  // right
using NSImageio::NSIspio::EPortIndex_FEO;
using NSImageio::NSIspio::EPortIndex_IMG2O;
using NSImageio::NSIspio::EPortIndex_IMG3O;
using NSImageio::NSIspio::EPortIndex_IMGI;
using NSImageio::NSIspio::EPortIndex_LCEI;
using NSImageio::NSIspio::EPortIndex_MFBO;
using NSImageio::NSIspio::EPortIndex_TUNING;
using NSImageio::NSIspio::EPortIndex_VIPI;
using NSImageio::NSIspio::EPortIndex_WDMAO;
using NSImageio::NSIspio::EPortIndex_WROTO;

static int cw = 0;
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

static MUINT32 calImgOffset(std::shared_ptr<IImageBuffer> pIMGBuffer,
                            const MRect& tmpRect) {
  MUINT32 u4PixelToBytes = 0;

  MINT imgFormat = pIMGBuffer->getImgFormat();

  if (imgFormat == eImgFmt_YV12) {
    u4PixelToBytes = 1;
  } else if (imgFormat == eImgFmt_YUY2) {
    u4PixelToBytes = 2;
  } else {
    MY_LOGW("unsupported image format %d", imgFormat);
  }

  return tmpRect.p.y * pIMGBuffer->getBufStridesInBytes(0) +
         tmpRect.p.x * u4PixelToBytes;  // in byte
}

P2ANode::P2ANode(const char* name)
    : StreamingFeatureNode(name),
      mp3A(NULL),
      mNormalStream(NULL),
      mFullImgPoolAllocateNeed(0),
      mCropMode(CROP_MODE_NONE),
      mLastDualParamValid(MFALSE) {
  TRACE_FUNC_ENTER();

  this->addWaitQueue(&mRequests);

  m3dnrLogLevel = getPropertyValue("vendor.camera.3dnr.log.level", 0);
  bDump3DNR = getPropertyValue("debug.3dnr.dump.enable", 0);
  TRACE_FUNC_EXIT();
}

P2ANode::~P2ANode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MVOID P2ANode::setNormalStream(
    std::shared_ptr<NSCam::v4l2::INormalStream> stream, MUINT32 version) {
  TRACE_FUNC_ENTER();
  MY_LOGI("setNormalStream+");
  mNormalStream = stream;
  mDipVersion = version;
  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::configNormalStream(const StreamConfigure config) {
  MY_LOGI("configNormalStream+");
  if (mNormalStream != nullptr && config.mInStreams.size() > 0 &&
      config.mOutStreams.size() > 0) {
    MBOOL ret = mNormalStream->init("P2AStreaming", config,
                                    v4l2::ENormalStreamTag_3DNR);
    if (ret != MTRUE) {
      MY_LOGE("NormalStream init failed");
      return MFALSE;
    }
    ret = mNormalStream->requestBuffers(EPortIndex_TUNING,
                                        IImageBufferAllocator::ImgParam(0, 0),
                                        &mTuningBuffers);
    if (ret != MTRUE) {
      MY_LOGE("NormalStream requestBuffers failed");
      return MFALSE;
    }
    mTuningBuffers_All = mTuningBuffers;
  } else {
    MY_LOGE("mNormalStream is NULL");
  }
  return MTRUE;
}

MVOID P2ANode::setFullImgPool(const std::shared_ptr<IBufferPool>& pool,
                              MUINT32 allocate) {
  TRACE_FUNC_ENTER();
  mFullImgPool = pool;
  mFullImgPoolAllocateNeed = allocate;
  TRACE_FUNC_EXIT();
}

MVOID P2ANode::setPureImgPool(
    const std::unordered_map<MUINT32, std::shared_ptr<IBufferPool>>& poolMap) {
  TRACE_FUNC_ENTER();
  mPureImgPoolMap = poolMap;
  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::onInit() {
  TRACE_FUNC_ENTER();
  StreamingFeatureNode::onInit();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onUninit() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onThreadStart() {
  TRACE_FUNC_ENTER();

  if (mFullImgPoolAllocateNeed && mFullImgPool != NULL) {
    Timer timer(MTRUE);
    mFullImgPool->allocate(mFullImgPoolAllocateNeed);
    timer.stop();
    MY_LOGD("mFullImg %s %d buf in %d ms", STR_ALLOCATE,
            mFullImgPoolAllocateNeed, timer.getElapsed());
  }
  init3A();
  initP2();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onThreadStop() {
  TRACE_FUNC_ENTER();
  this->waitNormalStreamBaseDone();
  uninitP2();
  uninit3A();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::onData(DataID id, const RequestPtr& data) {
  TRACE_FUNC_ENTER();
  MY_LOGI("@@ Frame %d: %s arrived", data->mRequestNo, ID2Name(id));
  MBOOL ret = MFALSE;

  switch (id) {
    case ID_ROOT_TO_P2A:
      mRequests.enque(data);
      ret = MTRUE;
      break;
    default:
      ret = MFALSE;
      break;
  }

  TRACE_FUNC_EXIT();
  return ret;
}
IOPolicyType P2ANode::getIOPolicy(StreamType /*stream*/,
                                  const StreamingReqInfo& reqInfo) const {
  IOPolicyType policy = IOPOLICY_INOUT;
  if (HAS_3DNR(reqInfo.mFeatureMask) && reqInfo.isMaster()) {
    policy = IOPOLICY_LOOPBACK;
  }

  return policy;
}

MBOOL P2ANode::onThreadLoop() {
  TRACE_FUNC("Waitloop");
  RequestPtr request;

  P2_CAM_TRACE_CALL(TRACE_DEFAULT);
  if (!waitAllQueue()) {
    return MFALSE;
  }

  if (!mRequests.deque(&request)) {
    MY_LOGE("Request deque out of sync");
    return MFALSE;
  } else if (request == NULL) {
    MY_LOGE("Request out of sync");
    return MFALSE;
  }
  TRACE_FUNC_ENTER();

  request->mTimer.startP2A();
  processP2A(request);
  request->mTimer.stopP2A();  // When NormalStream callback, stopP2A will be
                              // called again to record this frame duration

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL isSameTuning(const SFPIOMap& io1, const SFPIOMap& io2, MUINT32 sensorID) {
  return (io1.getTuning(sensorID).mFlag == io2.getTuning(sensorID).mFlag);
}

MBOOL P2ANode::processP2A(const RequestPtr& request) {
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);

  TRACE_FUNC_ENTER();
  P2AEnqueData data;
  P2ATuningIndex tuningIndex;
  data.mRequest = request;
  SFPIOManager& ioMgr = request->mSFPIOManager;
  if (ioMgr.countAll() == 0) {
    MY_LOGW("No output frame exist in P2ANode, directly let SFP return.");
    HelpReq req(FeaturePipeParam::MSG_FRAME_DONE);
    handleData(ID_P2A_TO_HELPER, HelperData(req, request));
    return MFALSE;
  }

  QParams param;
  // --- Start prepare General QParam
  request->mTimer.startP2ATuning();
  if (mPipeUsage.isDynamicTuning()) {
    // TODO(MTK) This should consider put after prepare3DNR()
    prepareRawTuning(&param, request, &data, &tuningIndex);
  } else {
    prepareQParams(&param, request, &tuningIndex);
  }
  request->mTimer.stopP2ATuning();
  MY_LOGD("prepare3DNR +.");
  if (request->need3DNR()) {
    if (!prepare3DNR(&param, request, tuningIndex)) {
      getP2CamContext(request->getMasterID())->setPrevFullImg(NULL);
    }
  }
  MY_LOGD("prepare3DNR-.");
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "PrepareOutput");
  if (ioMgr.countNonLarge() != 0) {
    prepareNonMdpIO(&param, request, &data, tuningIndex);
    prepareMasterMdpOuts(&param, request, &data, tuningIndex);
    prepareSlaveOuts(&param, request, &data, tuningIndex);
  }

  if (ioMgr.countLarge() != 0) {
    if (tuningIndex.isLargeMasterValid()) {
      prepareLargeMdpOuts(&param, request, tuningIndex.mLargeMaster,
                          request->mMasterID);
    }
    if (tuningIndex.isLargeSlaveValid()) {
      prepareLargeMdpOuts(&param, request, tuningIndex.mLargeSlave,
                          request->mSlaveID);
    }
  }
  P2_CAM_TRACE_END(TRACE_ADVANCED);

  // ---- Prepare QParam Done ----
  if (request->needPrintIO()) {
    printIO(request, param);
  }
  enqueFeatureStream(&param, &data);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MVOID P2ANode::onNormalStreamBaseCB(QParams* pParams,
                                    const P2AEnqueData& data) {
  // This function is not thread safe,
  // avoid accessing P2ANode class members
  TRACE_FUNC_ENTER();
  for (auto&& frame : pParams->mvFrameParams) {
    if (bDump3DNR == 1) {
      for (size_t i = 0; i < frame.mvOut.size(); i++) {
        char filename[256] = {0};
        if (frame.mvOut[i].mPortID.index == PORT_IMG3O.index) {
          snprintf(filename, sizeof(filename),
                   "%s/p2_out_IMG3O_%d_%d_%d_%d.yv12", DUMP_PATH,
                   frame.mvOut[i].mBuffer->getImgSize().w,
                   frame.mvOut[i].mBuffer->getImgSize().h,
                   frame.mvOut[i].mTransform, cw);
        }
        if (frame.mvOut[i].mPortID.index == PORT_WDMAO.index) {
          snprintf(filename, sizeof(filename),
                   "%s/p2_out_WDMA_%d_%d_%d_%d.nv12", DUMP_PATH,
                   frame.mvOut[i].mBuffer->getImgSize().w,
                   frame.mvOut[i].mBuffer->getImgSize().h,
                   frame.mvOut[i].mTransform, cw);
          cw++;
        }
        MY_LOGD("onNormalStreamBaseCBdump %s", filename);
        frame.mvOut.at(i).mBuffer->saveToFile(filename);
      }
    }
    for (size_t i = 0; i < frame.mvExtraParam.size(); i++) {
      MUINT cmdIdx = frame.mvExtraParam[i].CmdIdx;
      if (cmdIdx == EPIPE_IMG3O_CRSPINFO_CMD) {
        CrspInfo* extraParam =
            static_cast<CrspInfo*>(frame.mvExtraParam[i].moduleStruct);
        if (extraParam) {
          delete extraParam;
        }
      }
    }
  }

  {
    std::lock_guard<std::mutex> _l(mTuningLock);
    for (auto& tuning : data.tuningBufs) {
      mTuningBuffers.push_back(tuning);
    }
  }

  RequestPtr request = data.mRequest;
  if (request == NULL) {
    MY_LOGE("Missing request");
  } else {
    request->mTimer.stopEnqueP2A();
    MY_LOGI("sensor(%d) Frame %d enque done in %d ms, result = %d",
            mSensorIndex, request->mRequestNo,
            request->mTimer.getElapsedEnqueP2A(), pParams->mDequeSuccess);

    if (!pParams->mDequeSuccess) {
      MY_LOGW("Frame %d enque result failed", request->mRequestNo);
    }

    request->updateResult(pParams->mDequeSuccess);
    handleResultData(request, data);
    request->mTimer.stopP2A();
  }

  this->decExtThreadDependency();
  TRACE_FUNC_EXIT();
}

MVOID P2ANode::handleResultData(const RequestPtr& request,
                                const P2AEnqueData& data) {
  // This function is not thread safe,
  // because it is called by onQParamsCB,
  // avoid accessing P2ANode class members
  TRACE_FUNC_ENTER();
  BasicImg full;
  if (data.mNextFullImg.mBuffer != NULL) {
    full = data.mNextFullImg;
  } else {
    full = data.mFullImg;
  }
  {
    HelpReq req(FeaturePipeParam::MSG_FRAME_DONE);
    handleData(ID_P2A_TO_HELPER, HelperData(req, request));
  }
  if (request->needNddDump()) {
    if (data.mFullImg.mBuffer != NULL) {
      TuningUtils::FILE_DUMP_NAMING_HINT hint =
          request->mP2Pack.getSensorData(request->mMasterID).mNDDHint;
      data.mFullImg.mBuffer->getImageBuffer()->syncCache(eCACHECTRL_INVALID);
      dumpNddData(&hint, data.mFullImg.mBuffer->getImageBufferPtr(),
                  EPortIndex_IMG3O);
    }
  }

  if (request->needDump()) {
    if (data.mFullImg.mBuffer != NULL) {
      data.mFullImg.mBuffer->getImageBuffer()->syncCache(eCACHECTRL_INVALID);
      dumpData(data.mRequest, data.mFullImg.mBuffer->getImageBufferPtr(),
               "full");
    }
    if (data.mNextFullImg.mBuffer != NULL) {
      data.mNextFullImg.mBuffer->getImageBuffer()->syncCache(
          eCACHECTRL_INVALID);
      dumpData(data.mRequest, data.mNextFullImg.mBuffer->getImageBufferPtr(),
               "nextfull");
    }
    if (data.mSlaveFullImg.mBuffer != NULL) {
      data.mSlaveFullImg.mBuffer->getImageBuffer()->syncCache(
          eCACHECTRL_INVALID);
      dumpData(data.mRequest, data.mSlaveFullImg.mBuffer->getImageBufferPtr(),
               "slaveFull");
    }
    if (data.mSlaveNextFullImg.mBuffer != NULL) {
      data.mSlaveNextFullImg.mBuffer->getImageBuffer()->syncCache(
          eCACHECTRL_INVALID);
      dumpData(data.mRequest,
               data.mSlaveNextFullImg.mBuffer->getImageBufferPtr(),
               "slaveNextfull");
    }
    if (data.mPureImg != NULL) {
      data.mPureImg->getImageBuffer()->syncCache(eCACHECTRL_INVALID);
      dumpData(data.mRequest, data.mPureImg->getImageBufferPtr(), "pure");
    }
    if (data.mSlavePureImg != NULL) {
      data.mSlavePureImg->getImageBuffer()->syncCache(eCACHECTRL_INVALID);
      dumpData(data.mRequest, data.mSlavePureImg->getImageBufferPtr(),
               "slavePure");
    }
    if (data.mFMResult.FM_B.Register_Medium != NULL) {
      dumpData(data.mRequest,
               data.mFMResult.FM_B.Register_Medium->getImageBufferPtr(),
               "fm_reg_m");
    }
    if (data.mFovP2AResult.mFEO_Master != NULL) {
      dumpData(data.mRequest,
               data.mFovP2AResult.mFEO_Master->getImageBufferPtr(),
               "mFEO_Master");
    }
    if (data.mFovP2AResult.mFEO_Slave != NULL) {
      dumpData(data.mRequest,
               data.mFovP2AResult.mFEO_Slave->getImageBufferPtr(),
               "mFEO_Slave");
    }
    if (data.mFovP2AResult.mFMO_MtoS != NULL) {
      dumpData(data.mRequest, data.mFovP2AResult.mFMO_MtoS->getImageBufferPtr(),
               "mFMO_MtoS");
    }
    if (data.mFovP2AResult.mFMO_StoM != NULL) {
      dumpData(data.mRequest, data.mFovP2AResult.mFMO_StoM->getImageBufferPtr(),
               "mFMO_StoM");
    }
  }
  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::initP2() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  TRACE_FUNC_EXIT();
  return ret;
}

MVOID P2ANode::uninitP2() {
  TRACE_FUNC_ENTER();

  for (auto& it : mTuningBuffers_All) {
    it->unlockBuf("V4L2");
  }

  mNormalStream = NULL;

  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepareQParams(QParams* params,
                              const RequestPtr& request,
                              P2ATuningIndex* tuningIndex) {
  TRACE_FUNC_ENTER();
  MY_LOGI("P2ANode::prepareQParams");

  params->mvFrameParams.clear();
  params->mvFrameParams.push_back(FrameParams());
  FrameParams& master = params->mvFrameParams.at(0);
  request->getMasterFrameBasic(&master);
  request->getMasterFrameInput(&master);
  request->getMasterFrameTuning(&master);
  prepareStreamTag(params, request);
  tuningIndex->mGenMaster = 0;

  MUINT32 slaveID = request->mSlaveID;
  if (request->needFullImg(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()), slaveID) ||
      request->needNextFullImg(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()),
          slaveID) /*|| request->needFOVFEFM()*/) {
    if (!request->hasSlave(slaveID)) {
      MY_LOGE(
          "Failed to get slave feature params. Cannot copy slave FrameParams!");
      return MFALSE;
    }
    FeaturePipeParam& fparam_slave = request->getSlave(slaveID);
    if (!fparam_slave.mQParams.mvFrameParams.size()) {
      MY_LOGE(
          "Slave QParam's FrameParam Size = 0. Cannot copy slave FrameParams!");
      return MFALSE;
    }
    params->mvFrameParams.push_back(fparam_slave.mQParams.mvFrameParams.at(0));
    tuningIndex->mGenSlave = params->mvFrameParams.size() - 1;
    //
    FrameParams& f = params->mvFrameParams.at(tuningIndex->mGenSlave);
    f.mSensorIdx = slaveID;
    f.mStreamTag = ENormalStreamTag_Normal;
    f.mvOut.clear();
    f.mvCropRsInfo.clear();
    f.mvExtraParam.clear();
  }
  TRACE_FUNC_EXIT();
  return MFALSE;
}

MBOOL P2ANode::prepareStreamTag(QParams* params, const RequestPtr& request) {
  TRACE_FUNC_ENTER();
  (void)(request);
  if (params->mvFrameParams.size()) {
    // 1. if non-TimeSharing --> use the original assgined stream tag, ex:
    // ENormalStreamTag_Normal
    // 2. if TimeSharing -> use ENormalStreamTag_Vss
    if (mPipeUsage.supportTimeSharing()) {
      params->mvFrameParams.at(0).mStreamTag = ENormalStreamTag_Vss;
    }
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::prepareFullImgFromInput(const RequestPtr& request,
                                       P2AEnqueData* data) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  IImageBuffer* input = nullptr;
  if ((input = request->getMasterInputBuffer()) == nullptr) {
    MY_LOGE("Cannot get input image buffer");
    ret = MFALSE;
  } else {
    data->mFullImg.mBuffer = std::make_shared<IIBuffer_IImageBuffer>(input);
    if (data->mFullImg.mBuffer == nullptr) {
      MY_LOGE("OOM: failed to allocate IIBuffer");
      ret = MFALSE;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2ANode::prepareNonMdpIO(QParams* params,
                               const RequestPtr& request,
                               P2AEnqueData* data,
                               const P2ATuningIndex& tuningIndex) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  if (!tuningIndex.isMasterMainValid()) {
    MY_LOGE(
        "Both master General Normal/Pure & Physical tuning not exist! Can not "
        "prepare non mdp out img.");
    return MFALSE;
  }
  MUINT32 masterIndex = tuningIndex.getMasterMainIndex();
  MBOOL isGenNormalRun = (masterIndex == (MUINT32)tuningIndex.mGenMaster);
  FrameParams& frame = params->mvFrameParams.at(masterIndex);

  FrameInInfo inInfo;
  getFrameInInfo(&inInfo, frame);
  const MUINT32 masterID = request->mMasterID;

  prepareFDImg(&frame, request, data);
  prepareFDCrop(&frame, request, data);

  if (request->isForceIMG3O() ||
      (request->needFullImg(
           std::dynamic_pointer_cast<P2ANode>(shared_from_this()), masterID) &&
       isGenNormalRun)) {
    prepareFullImg(&frame, request, &(data->mFullImg), inInfo, masterID);
    // Full Img no need crop
  }

  if (request->need3DNR() && isGenNormalRun &&
      getP2CamContext(request->getMasterID())->getPrevFullImg() != NULL) {
    prepareVIPI(&frame, request, data);
  }
  getP2CamContext(request->getMasterID())
      ->setPrevFullImg((request->need3DNR() ? data->mFullImg.mBuffer : NULL));

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2ANode::prepareMasterMdpOuts(QParams* params,
                                    const RequestPtr& request,
                                    P2AEnqueData* data,
                                    const P2ATuningIndex& tuningIndex) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  if (!tuningIndex.isMasterMainValid()) {
    MY_LOGE(
        "Both master General & Physical tuning not exist! Can not prepare "
        "output img.");
    return MFALSE;
  }

  MUINT32 sID = request->mMasterID;
  // Handle Pure First
  if (tuningIndex.isPureMasterValid()) {
    FrameParams& frame =
        params->mvFrameParams.at((MUINT32)tuningIndex.mPureMaster);
    preparePureImg(&frame, request, &(data->mPureImg), sID);
  }

  // Handle Main Frame, it maybe normal or pure or physical frame
  MUINT32 masterIndex = tuningIndex.getMasterMainIndex();
  FrameParams& frame = params->mvFrameParams.at(masterIndex);
  FrameInInfo inInfo;
  getFrameInInfo(&inInfo, frame);
  MBOOL needExtraPhyRun = (tuningIndex.isPhyMasterValid()) &&
                          (tuningIndex.mPhyMaster != MINT32(masterIndex));
  SFPOutput output;
  std::vector<SFPOutput> outList;
  outList.reserve(5);
  // Internal Buffer has highest prority, need to push back first.
  if (request->needDisplayOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this())) &&
      request->getDisplayOutput(&output)) {
    outList.push_back(output);
  }

  if (request->needRecordOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this())) &&
      request->getRecordOutput(&output)) {
    outList.push_back(output);
  }

  if (request->needExtraOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()))) {
    request->getExtraOutputs(&outList);  // extra output maybe more than 1.
  }

  if (!needExtraPhyRun &&
      request->needPhysicalOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()), sID) &&
      request->getPhysicalOutput(&output, sID)) {
    outList.push_back(output);
  }

  if (data->mFullImg.mBuffer == nullptr && needFullForExtraOut(&outList)) {
    prepareFullImg(&frame, request, &(data->mFullImg), inInfo, sID);
    if (!mPipeUsage.supportIMG3O()) {
      MY_LOGD(
          "Need Full img but different crop may not supportted! All output "
          "using p2amdp.");
      data->mRemainingOutputs = outList;
      outList.clear();
    }
  }
  prepareOneMdpFrameParam(&frame, outList, &(data->mRemainingOutputs));
  if (data->mRemainingOutputs.size() > 0) {
    prepareExtraMdpCrop(data->mFullImg, &(data->mRemainingOutputs));
  }

  //--- Additional Run for physical output ---
  if (needExtraPhyRun &&
      request->needPhysicalOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()), sID) &&
      request->getPhysicalOutput(&output, sID)) {
    std::vector<SFPOutput> phyOutList;
    phyOutList.push_back(output);
    prepareMdpFrameParam(params, (MUINT32)tuningIndex.mPhyMaster, phyOutList);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2ANode::prepareSlaveOuts(QParams* params,
                                const RequestPtr& request,
                                P2AEnqueData* data,
                                const P2ATuningIndex& tuningIndex) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  if (!tuningIndex.isSlaveMainValid()) {
    TRACE_FUNC(
        "Both slave General Normal/Pure & Physical tuning not exist! Can not "
        "prepare output img.");
    return MFALSE;
  }
  const MUINT32 sID = request->mSlaveID;
  // Handle Pure First
  if (tuningIndex.isPureSlaveValid()) {
    FrameParams& frame =
        params->mvFrameParams.at((MUINT32)tuningIndex.mPureSlave);
    preparePureImg(&frame, request, &(data->mSlavePureImg), sID);
  }

  // Handle Main Frame, it maybe normal or pure or physical frame
  MUINT32 slaveIndex = tuningIndex.getSlaveMainIndex();
  MBOOL needExtraPhyRun = (tuningIndex.isPhySlaveValid()) &&
                          (tuningIndex.mPhySlave != MINT32(slaveIndex));
  MBOOL isGenNormalRun = (slaveIndex == (MUINT32)tuningIndex.mGenSlave);
  FrameParams& frame = params->mvFrameParams.at((MUINT32)slaveIndex);
  FrameInInfo inInfo;
  getFrameInInfo(&inInfo, frame);

  std::vector<SFPOutput> outList;
  outList.reserve(2);

  if (request->needFullImg(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()), sID) &&
      isGenNormalRun) {
    prepareFullImg(&frame, request, &(data->mSlaveFullImg), inInfo, sID);
    // Full Img no need crop
  }
  SFPOutput output;
  if (!needExtraPhyRun &&
      request->needPhysicalOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()), sID) &&
      request->getPhysicalOutput(&output, sID)) {
    outList.push_back(output);
  }
  prepareMdpFrameParam(params, slaveIndex,
                       outList);  // no consider more MDP run for slave
  //--- Additional Run for physical output ---
  if (needExtraPhyRun &&
      request->needPhysicalOutput(
          std::dynamic_pointer_cast<P2ANode>(shared_from_this()), sID) &&
      request->getPhysicalOutput(&output, sID)) {
    std::vector<SFPOutput> phyOutList;
    phyOutList.push_back(output);
    prepareMdpFrameParam(params, (MUINT32)tuningIndex.mPhySlave, outList);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL P2ANode::prepareLargeMdpOuts(QParams* params,
                                   const RequestPtr& request,
                                   MINT32 frameIndex,
                                   MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  std::vector<SFPOutput> outList;
  if (!request->getLargeOutputs(&outList, sensorID)) {
    MY_LOGE("Get Large Out List failed! sID(%d), QFrameInd(%d)", sensorID,
            frameIndex);
  }

#if 1  // MTK_DP_ENABLE
  prepareMdpFrameParam(params, frameIndex,
                       outList);  // no consider more MDP run for slave
#endif
  TRACE_FUNC_EXIT();
  return ret;
}

MVOID P2ANode::prepareFullImg(FrameParams* frame,
                              const RequestPtr& request,
                              BasicImg* outImg,
                              const FrameInInfo& inInfo,
                              MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  // TODO(MTK) Pool maybe not the same in different sensors
  MY_LOGD("3dnr img3o: Frame %d FullImgPool=(%d/%d)", request->mRequestNo,
          mFullImgPool->peakAvailableSize(), mFullImgPool->peakPoolSize());
  outImg->mBuffer = mFullImgPool->requestIIBuffer();
  std::shared_ptr<IImageBuffer> pIMGBuffer = outImg->mBuffer->getImageBuffer();

  const SrcCropInfo srcCropInfo = request->getSrcCropInfo(sensorID);
  const MRect& srcCrop = srcCropInfo.mSrcCrop;
  outImg->mDomainOffset = srcCrop.p;
  pIMGBuffer->setTimestamp(inInfo.timestamp);
  if (!pIMGBuffer->setExtParam(srcCrop.s)) {
    MY_LOGE("Full Img setExtParm Fail!, target size(%dx%d)", srcCrop.s.w,
            srcCrop.s.h);
  }

  if (mPipeUsage.supportIMG3O()) {
    Output output;
    output.mPortID = PortID(EPortType_Memory, EPortIndex_IMG3O, PORTID_OUT);
    output.mBuffer = pIMGBuffer.get();
    if (srcCropInfo.mIsSrcCrop) {
      output.mOffsetInBytes = calImgOffset(pIMGBuffer, srcCrop);  // in byte
      // new driver interface to overwrite mOffsetInBytes
      CrspInfo* crspParam = new CrspInfo();
      crspParam->m_CrspInfo.p_integral.x = srcCrop.p.x;
      crspParam->m_CrspInfo.p_integral.y = srcCrop.p.y;
      crspParam->m_CrspInfo.s.w = srcCrop.s.w;
      crspParam->m_CrspInfo.s.h = srcCrop.s.h;
      ExtraParam extraParam;
      extraParam.CmdIdx = EPIPE_IMG3O_CRSPINFO_CMD;
      extraParam.moduleStruct = static_cast<void*>(crspParam);
      frame->mvExtraParam.push_back(extraParam);
    }
    frame->mvOut.push_back(output);
  } else {
    SFPOutput sfpOut;
    sfpOut.mBuffer = pIMGBuffer.get();
    sfpOut.mTransform = 0;
    sfpOut.mCropRect = srcCrop;
    sfpOut.mCropDstSize = srcCrop.s;
    pushSFPOutToMdp(frame, NSCam::NSIoPipe::PORT_WDMAO, sfpOut);
  }

  TRACE_FUNC_EXIT();
}

MVOID P2ANode::preparePureImg(FrameParams* frame,
                              const RequestPtr& request,
                              ImgBuffer* outImg,
                              MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  const std::shared_ptr<IBufferPool>& pool = mPureImgPoolMap[sensorID];
  if (pool == NULL) {
    MY_LOGE("Pure pool Null!!! sId(%d), can not generate pure", sensorID);
    return;
  }
  TRACE_FUNC("Frame %d PureImgPool=(%d/%d)", request->mRequestNo,
             pool->peakAvailableSize(), pool->peakPoolSize());
  *outImg = pool->requestIIBuffer();
  std::shared_ptr<IImageBuffer> pIMGBuffer = (*outImg)->getImageBuffer();

  const SrcCropInfo srcCropInfo = request->getSrcCropInfo(sensorID);
  const MRect& srcCrop = srcCropInfo.mSrcCrop;
  FrameInInfo inInfo;
  getFrameInInfo(&inInfo, *frame);
  pIMGBuffer->setTimestamp(inInfo.timestamp);
  // TODO(MTK) custom size may be different from sensorID & pure YUV
  MSize vendorPureImgSize = mPipeUsage.supportVendorCusSize()
                                ? mPipeUsage.getStreamingSize()
                                : srcCropInfo.mSrcCrop.s;
  if (!pIMGBuffer->setExtParam(srcCrop.s)) {
    MY_LOGE("sId(%d) Pure Img setExtParm Fail!, target size(%dx%d)", sensorID,
            vendorPureImgSize.w, vendorPureImgSize.h);
  }

  SFPOutput sfpOut;
  sfpOut.mBuffer = pIMGBuffer.get();
  sfpOut.mTransform = 0;
  sfpOut.mCropRect = srcCrop;
  sfpOut.mCropDstSize = vendorPureImgSize;
  pushSFPOutToMdp(frame, NSCam::NSIoPipe::PORT_WDMAO, sfpOut);

  TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareVIPI(FrameParams* frame,
                           const RequestPtr& request,
                           P2AEnqueData* data) {
  TRACE_FUNC_ENTER();
  MY_LOGD("3dnr prepareVIPI+.");
  data->mPrevFullImg =
      getP2CamContext(request->getMasterID())->getPrevFullImg();
  Input input;
  input.mPortID = PortID(EPortType_Memory, EPortIndex_VIPI, PORTID_IN);
  input.mBuffer = data->mPrevFullImg->getImageBufferPtr();
  frame->mvIn.push_back(input);
  MBOOL bDump3DNR = MFALSE;
  bDump3DNR = ::property_get_int32("debug.3dnr.dump.enable", 0);
  if (bDump3DNR == 1) {
    char filename[256] = {0};
    snprintf(filename, sizeof(filename), "%s/p2_out_VIPI_%d_%d_%d_%d.yv12",
             DUMP_PATH, input.mBuffer->getImgSize().w,
             input.mBuffer->getImgSize().h, input.mTransform, cw);
    input.mBuffer->saveToFile(filename);
  }
  MY_LOGD("3dnr prepareVIPI-.");
  TRACE_FUNC_EXIT();
}
MVOID P2ANode::prepareFDImg(FrameParams* frame,
                            const RequestPtr& request,
                            P2AEnqueData* data) {
  TRACE_FUNC_ENTER();
  (void)(data);
  SFPOutput sfpOut;
  if (request->getFDOutput(&sfpOut)) {
    Output out;
    sfpOut.convertTo(&out);
    out.mPortID.index = EPortIndex_IMG2O;
    frame->mvOut.push_back(out);
  }
  TRACE_FUNC_EXIT();
}

MVOID P2ANode::prepareFDCrop(FrameParams* frame,
                             const RequestPtr& request,
                             P2AEnqueData* data) {
  TRACE_FUNC_ENTER();
  (void)(data);
  SFPOutput output;
  if (request->getFDOutput(&output)) {
    if (!output.isCropValid()) {
      MY_LOGD("default fd crop");
      output.mCropDstSize = output.mBuffer->getImgSize();
      output.mCropRect = MRect(MPoint(0, 0), output.mCropDstSize);
    }
    Feature::P2Util::push_crop(frame, IMG2O_CROP_GROUP, output.mCropRect,
                               output.mCropDstSize);
  }

  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepareExtraMdpCrop(const BasicImg& fullImg,
                                   std::vector<SFPOutput>* leftOutList) {
  TRACE_FUNC_ENTER();
  if (fullImg.mBuffer == NULL) {
    MY_LOGE("Need Extra MDP but Full Image is NULL !!");
  }
  for (auto&& sfpOut : *leftOutList) {
    sfpOut.mCropRect.p.x =
        fmax(sfpOut.mCropRect.p.x - fullImg.mDomainOffset.x, 0.0f);
    sfpOut.mCropRect.p.y =
        fmax(sfpOut.mCropRect.p.y - fullImg.mDomainOffset.y, 0.0f);
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::needFullForExtraOut(std::vector<SFPOutput>* outList) {
  static const MUINT32 maxMDPOut = 2;
  if (outList->size() > maxMDPOut) {
    return MTRUE;
  }
  MUINT32 rotCnt = 0;
  for (auto&& out : *outList) {
    if (out.mTransform != 0) {
      rotCnt += 1;
    }
  }
  return (rotCnt > 1);
}

MVOID P2ANode::enqueFeatureStream(NSCam::NSIoPipe::QParams* pParams,
                                  P2AEnqueData* data) {
  TRACE_FUNC_ENTER();
  MY_LOGI("sensor(%d) Frame %d enque start", mSensorIndex,
          data->mRequest->mRequestNo);
  data->mRequest->mTimer.startEnqueP2A();
  this->incExtThreadDependency();
  this->enqueNormalStreamBase(mNormalStream, pParams, *data);
  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::init3A() {
  TRACE_FUNC_ENTER();

  if (mp3A == NULL && SUPPORT_3A_HAL) {
    MAKE_Hal3A(
        mp3A, [](IHal3A* p) { p->destroyInstance(PIPE_CLASS_TAG); },
        mSensorIndex, PIPE_CLASS_TAG);
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MVOID P2ANode::uninit3A() {
  TRACE_FUNC_ENTER();

  if (mp3A) {
    // turn OFF 'pull up ISO value to gain FPS'
    AE_Pline_Limitation_T params;
    params.bEnable = MFALSE;  // disable
    params.bEquivalent = MTRUE;
    params.u4IncreaseISO_x100 = 100;
    params.u4IncreaseShutter_x100 = 100;
    mp3A->send3ACtrl(E3ACtrl_SetAEPlineLimitation, (MINTPTR)&params, 0);

    // destroy the instance
    mp3A = NULL;
  }

  TRACE_FUNC_EXIT();
}

MBOOL P2ANode::prepare3A(NSCam::NSIoPipe::QParams* params,
                         const RequestPtr& request) {
  TRACE_FUNC_ENTER();
  (void)params;
  (void)request;
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::prepareOneRawTuning(NSCam::NSIoPipe::QParams* params,
                                   const RequestPtr& request,
                                   const SFPIOMap& ioMap,
                                   std::shared_ptr<IImageBuffer>* tuningBuf,
                                   MUINT32 sensorID,
                                   Feature::P2Util::P2ObjPtr* p2ObjPtr,
                                   MBOOL needMetaOut,
                                   TuningHelper::Scene scene) {
  TRACE_FUNC_ENTER();
  const SFPSensorTuning& tuning = ioMap.getTuning(sensorID);
  const SFPSensorInput& sensorIn = request->getSensorInput(sensorID);
  VarMap& varMap = request->getSensorVarMap(sensorID);
  IImageBuffer* imgi = nullptr;
  if (tuning.isRRZOin()) {
    imgi = sensorIn.mRRZO;
  } else {
    imgi = sensorIn.mIMGO;
  }
  if (imgi == NULL) {
    MY_LOGE("Invalid input buffer");
    TRACE_FUNC_EXIT();
    return MFALSE;
  }

  Feature::P2Util::P2Pack newPack = Feature::P2Util::P2Pack(
      request->mP2Pack, request->mP2Pack.mLog, sensorID);
  TuningHelper::Input tuningIn(newPack, tuningBuf);
  tuningIn.mSensorInput = sensorIn;
  tuningIn.mTargetTuning = tuning;
  tuningIn.mSensorID = sensorID;
  tuningIn.mp3A = getP2CamContext(sensorID)->get3A();
  tuningIn.mTag = NSCam::v4l2::ENormalStreamTag_Normal;
  tuningIn.mUniqueKey = request->mRequestNo;
  tuningIn.mP2ObjPtr = *p2ObjPtr;
  tuningIn.mScene = scene;

  if (needMetaOut) {
    if (varMap.tryGet<MRect>(VAR_FD_CROP_ACTIVE_REGION,
                             &(tuningIn.mExtraMetaParam.fdCrop))) {
      tuningIn.mExtraMetaParam.isFDCropValid = MTRUE;
    }
  }

  NR3D::NR3DTuningInfo nr3dTuning;
  if (!tuning.isDisable3DNR() && scene == TuningHelper::Tuning_Normal) {
    NR3D::NR3DMVInfo dftMvInfo;
    nr3dTuning.canEnable3dnrOnFrame =
        varMap.get<MBOOL>(VAR_3DNR_CAN_ENABLE_ON_FRAME, MFALSE);
    nr3dTuning.isoThreshold = varMap.get<MUINT32>(VAR_3DNR_ISO_THRESHOLD, 100);
    nr3dTuning.mvInfo =
        varMap.get<NR3D::NR3DMVInfo>(VAR_3DNR_MV_INFO, dftMvInfo);
    nr3dTuning.inputSize = imgi->getImgSize();
    nr3dTuning.inputCrop.s = nr3dTuning.inputSize;
    nr3dTuning.inputCrop.p = MPoint(0, 0);
    tuningIn.mpNr3dTuningInfo = nr3dTuning;
  }
  FrameParams frameParam;
  if (!TuningHelper::process3A_P2A_Raw2Yuv(
          tuningIn, &frameParam, needMetaOut ? ioMap.mHalOut : NULL,
          needMetaOut ? ioMap.mAppOut : NULL)) {
    MY_LOGE(
        "Prepare Raw Tuning Failed! Path(%s), "
        "sensor(%d),frameNo(%d),mvFrameSize(%zu)",
        ioMap.pathName(), sensorID, request->mRequestNo,
        params->mvFrameParams.size());
    return MFALSE;
  }
  params->mvFrameParams.push_back(frameParam);

  TRACE_FUNC_EXIT();

  return MTRUE;
}

MINT32 P2ANode::addTuningFrameParam(MUINT32 sensorID,
                                    const SFPIOMap& ioMap,
                                    QParams* params,
                                    const RequestPtr& request,
                                    P2AEnqueData* data,
                                    TuningHelper::Scene scene) {
  MBOOL needMetaOut =
      ((ioMap.isGenPath() && sensorID != request->getMasterID()) ||
       (scene != TuningHelper::Tuning_Normal))
          ? MFALSE
          : MTRUE;
  /*add for tuning flow*/
  std::unique_lock<std::mutex> _l(mTuningLock, std::defer_lock);
  _l.lock();
  if (mTuningBuffers.empty()) {
    MY_LOGE("No tuning buffer,mTuningBuffers size:%d", mTuningBuffers.size());
    return -1;
  }
  auto tuning = mTuningBuffers[0];
  _l.unlock();
  std::shared_ptr<P2ASrzRecord> srzRec = std::make_shared<P2ASrzRecord>();
  Feature::P2Util::P2ObjPtr p2Ptr;
  p2Ptr.srz4 = &(srzRec->srz4);
  p2Ptr.hasPQ = MFALSE;
  data->tuningBufs.push_back(tuning);
  data->tuningSrzs.push_back(srzRec);
  memset(reinterpret_cast<void*>(tuning->getBufVA(0)), 0,
         tuning->getBitstreamSize());
  if (prepareOneRawTuning(params, request, ioMap, &tuning, sensorID, &p2Ptr,
                          needMetaOut, scene)) {
    _l.lock();
    mTuningBuffers.erase(mTuningBuffers.begin());
    _l.unlock();
    return params->mvFrameParams.size() - 1;
  }
  return -1;
}

MBOOL P2ANode::prepareRawTuning(QParams* params,
                                const RequestPtr& request,
                                P2AEnqueData* data,
                                P2ATuningIndex* tuningIndex) {
  TRACE_FUNC_ENTER();
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);
  MBOOL dualSlaveValid = request->isSlaveParamValid();
  SFPIOManager& ioMgr = request->mSFPIOManager;
  const SFPIOMap& generalIO = ioMgr.getFirstGeneralIO();
  const SFPIOMap& masterPhyIO = ioMgr.getPhysicalIO(request->mMasterID);
  const SFPIOMap& slavePhyIO = ioMgr.getPhysicalIO(request->mSlaveID);
  const SFPIOMap& masterLargeIO = ioMgr.getLargeIO(request->mMasterID);
  const SFPIOMap& slaveLargeIO = ioMgr.getLargeIO(request->mSlaveID);

  if (generalIO.isValid()) {
    // -- Master ---
    if (needNormalYuv(request->mMasterID, request)) {
      tuningIndex->mGenMaster = addTuningFrameParam(
          request->mMasterID, generalIO, params, request, data);
    }
    if (needPureYuv(request->mMasterID, request)) {
      tuningIndex->mPureMaster =
          addTuningFrameParam(request->mMasterID, generalIO, params, request,
                              data, TuningHelper::Tuning_Pure);
    }

    // -- Slave ---
    if (dualSlaveValid && needNormalYuv(request->mSlaveID, request)) {
      tuningIndex->mGenSlave = addTuningFrameParam(request->mSlaveID, generalIO,
                                                   params, request, data);
    }

    if (dualSlaveValid && needPureYuv(request->mSlaveID, request)) {
      tuningIndex->mPureSlave =
          addTuningFrameParam(request->mSlaveID, generalIO, params, request,
                              data, TuningHelper::Tuning_Pure);
    }

    MY_LOGE_IF(
        !(tuningIndex->isGenMasterValid() || tuningIndex->isPureMasterValid()),
        "GeneralIO valid but General tuning master inValid !!");
  }

  if (masterPhyIO.isValid()) {
    MBOOL masterFrameValid =
        tuningIndex->isGenMasterValid() || tuningIndex->isPureMasterValid();
    if (!SFPIOMap::isSameTuning(masterPhyIO, generalIO, request->mMasterID) ||
        !masterFrameValid) {
      tuningIndex->mPhyMaster = addTuningFrameParam(
          request->mMasterID, masterPhyIO, params, request, data);
    } else {
      tuningIndex->mPhyMaster = tuningIndex->isPureMasterValid()
                                    ? tuningIndex->mPureMaster
                                    : tuningIndex->mGenMaster;
    }
  }

  if (slavePhyIO.isValid() && dualSlaveValid) {
    MBOOL slaveFrameValid =
        tuningIndex->isGenSlaveValid() || tuningIndex->isPureSlaveValid();
    if (SFPIOMap::isSameTuning(slavePhyIO, generalIO, request->mSlaveID) ||
        !slaveFrameValid) {
      tuningIndex->mPhySlave = addTuningFrameParam(
          request->mSlaveID, slavePhyIO, params, request, data);
    } else {
      tuningIndex->mPhySlave = tuningIndex->isPureSlaveValid()
                                   ? tuningIndex->mPureSlave
                                   : tuningIndex->mGenSlave;
    }
  }

  if (masterLargeIO.isValid()) {
    tuningIndex->mLargeMaster = addTuningFrameParam(
        request->mMasterID, masterLargeIO, params, request, data);
  }

  if (slaveLargeIO.isValid()) {
    tuningIndex->mLargeSlave = addTuningFrameParam(
        request->mSlaveID, slaveLargeIO, params, request, data);
  }

  MY_LOGI_IF((mLastDualParamValid != dualSlaveValid),
             "Dual Slave valid (%d)->(%d). slaveID(%d)", mLastDualParamValid,
             dualSlaveValid, request->mSlaveID);
  MY_LOGD(
      "req(%d), TuningIndex, (GN/GP/Ph/L),master(%d/%d/%d/%d), "
      "slave(%d/%d/%d/%d)",
      request->mRequestNo, tuningIndex->mGenMaster, tuningIndex->mPureMaster,
      tuningIndex->mPhyMaster, tuningIndex->mLargeMaster,
      tuningIndex->mGenSlave, tuningIndex->mPureSlave, tuningIndex->mPhySlave,
      tuningIndex->mLargeSlave);

  mLastDualParamValid = dualSlaveValid;

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL P2ANode::needPureYuv(MUINT32 /*sensorID*/,
                           const RequestPtr& /*request*/) {
  // TODO(MTK) need check sensor  & request
  return mPipeUsage.supportPure();
}

MBOOL P2ANode::needNormalYuv(MUINT32 sensorID, const RequestPtr& request) {
  if (sensorID == request->mMasterID) {
    return request->needDisplayOutput(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this())) ||
           request->needRecordOutput(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this())) ||
           request->needExtraOutput(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this())) ||
           request->needFullImg(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this()),
               sensorID) ||
           request->needNextFullImg(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this()), sensorID)
           //|| request->needFOVFEFM()
           ||
           !needPureYuv(
               sensorID,
               request);  // in master general, normal or pure must exist one.
  } else {
    return request->needFullImg(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this()),
               sensorID) ||
           request->needNextFullImg(
               std::dynamic_pointer_cast<P2ANode>(shared_from_this()),
               sensorID);
  }
}
MBOOL P2ANode::prepare3DNR(NSCam::NSIoPipe::QParams* params,
                           const RequestPtr& request,
                           const P2ATuningIndex& tuningIndex) {
  TRACE_FUNC_ENTER();

  MUINT32 sensorID = request->mMasterID;
  const SrcCropInfo srcCropInfo = request->getSrcCropInfo(sensorID);
  const MRect& postCropSize = srcCropInfo.mSrcCrop;
  MY_LOGD("aaaa_cropInfo_test: w=%d, h=%d", postCropSize.s.w, postCropSize.s.h);
  NR3D::NR3DMVInfo defaultMvInfo;
  NR3D::NR3DMVInfo mvInfo =
      request->getVar<NR3D::NR3DMVInfo>(VAR_3DNR_MV_INFO, defaultMvInfo);
  eis_region tmpEisRegion;
  tmpEisRegion.gmvX = mvInfo.gmvX;
  tmpEisRegion.gmvY = mvInfo.gmvY;
  tmpEisRegion.x_int = mvInfo.x_int;
  tmpEisRegion.y_int = mvInfo.y_int;
  tmpEisRegion.confX = mvInfo.confX;
  tmpEisRegion.confY = mvInfo.confY;
  MINT32 iso = 200;
  iso = request->getVar<MUINT32>(VAR_3DNR_ISO, iso);
  MINT32 isoThreshold = 0;
  isoThreshold = request->getVar<MUINT32>(VAR_3DNR_ISO_THRESHOLD, 0);
  MBOOL res = do3dnrFlow(params, request, postCropSize, srcCropInfo.mRRZOSize,
                         tmpEisRegion, iso, isoThreshold, request->mRequestNo,
                         tuningIndex);
  TRACE_FUNC_EXIT();
  return res;
}
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
