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
#define PIPE_CLASS_TAG "Data"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_DATA
#include <featurePipe/common/include/PipeLog.h>
#include <memory>
#include <mtkcam/feature/featureCore/featurePipe/streaming/StreamingFeatureData.h>
#include "StreamingFeatureNode.h"
#include "StreamingFeature_Common.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using NSCam::Feature::P2Util::P2Pack;
using NSCam::Feature::P2Util::P2SensorData;
using NSCam::NSIoPipe::MCropRect;
using NSCam::NSIoPipe::QParams;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

std::unordered_map<MUINT32, std::string>
    StreamingFeatureRequest::mFeatureMaskNameMap;

StreamingFeatureRequest::StreamingFeatureRequest(
    const StreamingFeaturePipeUsage& pipeUsage,
    const FeaturePipeParam& extParam,
    MUINT32 requestNo,
    MUINT32 recordNo /*, const EISQState &eisQ*/)
    : mExtParam(extParam),
      mQParams(mExtParam.mQParams),
      mvFrameParams(mQParams.mvFrameParams),
      mPipeUsage(pipeUsage),
      mVarMap(mExtParam.mVarMap),
      mP2Pack(mExtParam.mP2Pack),
      mSFPIOManager(mExtParam.mSFPIOManager),
      mMasterID(pipeUsage.getSensorIndex()),
      mIORequestMap(
          {{mMasterID, IORequest<StreamingFeatureNode, StreamingReqInfo>()}}),
      mMasterIOReq(mIORequestMap[mMasterID]),
      mFeatureMask(extParam.mFeatureMask),
      mRequestNo(requestNo),
      mRecordNo(recordNo),
      mMWFrameNo(extParam.mP2Pack.getFrameData().mMWFrameNo),
      mAppMode(IStreamingFeaturePipe::APP_PHOTO_PREVIEW),
      mDebugDump(0),
      mP2DumpType(mExtParam.mDumpType),
      mSlaveParamMap(mExtParam.mSlaveParamMap),
      mDisplayFPSCounter(nullptr),
      mFrameFPSCounter(nullptr),
      mResult(MTRUE),
      mNeedDump(MFALSE),
      mForceIMG3O(MFALSE),
      mForceWarpPass(MFALSE),
      mForceGpuOut(NO_FORCE),
      mForceGpuRGBA(MFALSE),
      mForcePrintIO(MFALSE),
      mIs4K2K(MFALSE) {
  mQParams.mDequeSuccess = MFALSE;
  for (unsigned i = 0, n = mvFrameParams.size(); i < n; ++i) {
    mvFrameParams.at(i).UniqueKey = mRequestNo;
  }
  mTPILog = mPipeUsage.supportVendorLog();
  if (mPipeUsage.supportVendorDebug()) {
    mTPILog = mTPILog || property_get_int32(KEY_DEBUG_TPI_LOG, mTPILog);
    mTPIDump = property_get_int32(KEY_DEBUG_TPI_DUMP, mTPIDump);
    mTPIScan = property_get_int32(KEY_DEBUG_TPI_SCAN, mTPIScan);
    mTPIBypass = property_get_int32(KEY_DEBUG_TPI_BYPASS, mTPIBypass);
  }

  mHasGeneralOutput =
      hasDisplayOutput() || hasRecordOutput() || hasExtraOutput();

  mTimer.start();
}

StreamingFeatureRequest::~StreamingFeatureRequest() {
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);

  double frameFPS = 0, displayFPS = 0;
  if (mDisplayFPSCounter) {
    mDisplayFPSCounter->update(mTimer.getDisplayMark());
    displayFPS = mDisplayFPSCounter->getFPS();
  }
  if (mFrameFPSCounter) {
    mFrameFPSCounter->update(mTimer.getFrameMark());
    frameFPS = mFrameFPSCounter->getFPS();
  }
  mTimer.print(mRequestNo, mRecordNo, displayFPS, frameFPS);
}

MVOID StreamingFeatureRequest::setDisplayFPSCounter(
    std::shared_ptr<FPSCounter> counter) {
  mDisplayFPSCounter = counter;
}

MVOID StreamingFeatureRequest::setFrameFPSCounter(
    std::shared_ptr<FPSCounter> counter) {
  mFrameFPSCounter = counter;
}

MBOOL StreamingFeatureRequest::updateSFPIO() {
  if (!mPipeUsage.isQParamIOValid() && mSFPIOManager.countAll() == 0) {
    MY_LOGE("QParamIO invalid with SFPIOMap size is 0 !!. Can not continue.");
    return MFALSE;
  }

  if (mPipeUsage.isQParamIOValid()) {  // Hal1 Used
    createIOMapByQParams();
  }
  return MTRUE;
}

MVOID StreamingFeatureRequest::calSizeInfo() {
  SrcCropInfo cInfo;
  calNonLargeSrcCrop(&cInfo, mMasterID);
  mNonLargeSrcCrops[mMasterID] = cInfo;

  mFullImgSize = cInfo.mSrcCrop.s;
  mIs4K2K = NSFeaturePipe::is4K2K(mFullImgSize);

  if (mSlaveID != INVALID_SENSOR) {
    calNonLargeSrcCrop(&cInfo, mSlaveID);
    mNonLargeSrcCrops[mSlaveID] = cInfo;
  }
}

MVOID StreamingFeatureRequest::createIOMapByQParams() {
  TRACE_FUNC_ENTER();
  if (mSFPIOManager.countNonLarge() != 0) {
    MY_LOGE(
        "IOMap already exist before converting QParam to SFPIO!! nonLarge(%d)",
        mSFPIOManager.countNonLarge());
    return;
  }
  SFPIOMap ioMap;

  ioMap.mPathType = PATH_GENERAL;

  if (mvFrameParams.size() > 0) {
    parseIO(mMasterID, mvFrameParams[0], mVarMap, &ioMap, &mSFPIOManager);
  }
  // Add slave input
  if ((mSlaveID != INVALID_SENSOR && hasSlave(mSlaveID))) {
    FeaturePipeParam& fparam_slave = getSlave(mSlaveID);
    if (fparam_slave.mQParams.mvFrameParams.size() > 0) {
      parseIO(mSlaveID, fparam_slave.mQParams.mvFrameParams[0],
              fparam_slave.mVarMap, &ioMap, &mSFPIOManager);
    }
  }
  ioMap.mHalOut =
      mVarMap.get<NSCam::IMetadata*>(VAR_HAL1_HAL_OUT_METADATA, NULL);
  ioMap.mAppOut =
      mVarMap.get<NSCam::IMetadata*>(VAR_HAL1_APP_OUT_METADATA, NULL);

  mSFPIOManager.addGeneral(ioMap);

  TRACE_FUNC_EXIT();
}

MBOOL StreamingFeatureRequest::updateResult(MBOOL result) {
  mResult = (mResult && result);
  mQParams.mDequeSuccess = mResult;
  return mResult;
}

MBOOL StreamingFeatureRequest::doExtCallback(FeaturePipeParam::MSG_TYPE msg) {
  MBOOL ret = MFALSE;
  if (msg == FeaturePipeParam::MSG_FRAME_DONE) {
    mTimer.stop();
  }
  if (mExtParam.mCallback) {
    ret = mExtParam.mCallback(msg, &mExtParam);
  }
  return ret;
}

MSize StreamingFeatureRequest::getMasterInputSize() {
  MSize inputSize = MSize(0, 0);
  const IImageBuffer* buffer = getMasterInputBuffer();
  if (buffer) {
    inputSize = buffer->getImgSize();
  }
  return inputSize;
}

MVOID StreamingFeatureRequest::calNonLargeSrcCrop(SrcCropInfo* info,
                                                  MUINT32 sensorID) {
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  const SFPSensorInput& sensorIn = mSFPIOManager.getInput(sensorID);
  SFPSensorTuning tuning;

  if (generalIO.isValid() && generalIO.hasTuning(sensorID)) {
    tuning = generalIO.getTuning(sensorID);
  } else if (mSFPIOManager.hasPhysicalIO(sensorID)) {
    tuning = mSFPIOManager.getPhysicalIO(sensorID).getTuning(sensorID);
  }

  info->mIMGOSize =
      (sensorIn.mIMGO != NULL) ? sensorIn.mIMGO->getImgSize() : MSize(0, 0);
  info->mRRZOSize = (sensorIn.mRRZO != NULL)
                        ? sensorIn.mRRZO->getImgSize()
                        : mP2Pack.isValid()
                              ? mP2Pack.getSensorData(sensorID).mP1OutSize
                              : getSensorVarMap(sensorID).get<MSize>(
                                    VAR_HAL1_P1_OUT_RRZ_SIZE, MSize(0, 0));
  info->mIMGOin = (tuning.isIMGOin() && !tuning.isRRZOin());
  if (tuning.isRRZOin()) {
    info->mSrcCrop = MRect(MPoint(0, 0), info->mRRZOSize);
  }
  info->mIsSrcCrop = MFALSE;

  if (info->mIMGOin) {
    info->mSrcCrop = (mP2Pack.isValid())
                         ? mP2Pack.getSensorData(sensorID).mP1Crop
                         : getSensorVarMap(sensorID).get<MRect>(
                               VAR_IMGO_2IMGI_P1CROP, MRect(0, 0));
    info->mIsSrcCrop = MTRUE;
    // HW limitation
    info->mSrcCrop.p.x &= (~1);
  }
  // NOTE : currently not consider CRZ mode
  MY_LOGD(
      "sID(%d), imgoIn(%d), srcCrop(%d,%d,%dx%d), isSrcCrop(%d), mP2Pack "
      "Valid(%d), imgo(%dx%d),rrz(%dx%d)",
      sensorID, info->mIMGOin, info->mSrcCrop.p.x, info->mSrcCrop.p.y,
      info->mSrcCrop.s.w, info->mSrcCrop.s.h, info->mIsSrcCrop,
      mP2Pack.isValid(), info->mIMGOSize.w, info->mIMGOSize.h,
      info->mRRZOSize.w, info->mRRZOSize.h);
}

IImageBuffer* StreamingFeatureRequest::getMasterInputBuffer() {
  IImageBuffer* buffer = nullptr;
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  const SFPSensorInput& masterIn = mSFPIOManager.getInput(mMasterID);
  SFPSensorTuning tuning;

  if (generalIO.isValid() && generalIO.hasTuning(mMasterID)) {
    tuning = generalIO.getTuning(mMasterID);
  } else if (mSFPIOManager.hasPhysicalIO(mMasterID)) {
    tuning = mSFPIOManager.getPhysicalIO(mMasterID).getTuning(mMasterID);
  }
  buffer = tuning.isRRZOin() ? masterIn.mRRZO
                             : tuning.isIMGOin() ? masterIn.mIMGO : NULL;
  return buffer;
}

MBOOL StreamingFeatureRequest::getDisplayOutput(SFPOutput* output) {
  TRACE_FUNC_ENTER();
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  MBOOL ret = getOutBuffer(generalIO, IO_TYPE_DISPLAY, output);
  if (!ret) {
    TRACE_FUNC("frame %d: No display buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureRequest::getRecordOutput(SFPOutput* output) {
  TRACE_FUNC_ENTER();
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  MBOOL ret = getOutBuffer(generalIO, IO_TYPE_RECORD, output);
  if (!ret) {
    TRACE_FUNC("frame %d: No record buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureRequest::getExtraOutput(SFPOutput* output) {
  TRACE_FUNC_ENTER();
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  MBOOL ret = getOutBuffer(generalIO, IO_TYPE_EXTRA, output);
  if (!ret) {
    TRACE_FUNC("frame %d: No extra buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureRequest::getExtraOutputs(
    std::vector<SFPOutput>* outList) {
  TRACE_FUNC_ENTER();
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  MBOOL ret = getOutBuffer(generalIO, IO_TYPE_EXTRA, outList);
  if (!ret) {
    TRACE_FUNC("frame %d: No extra buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureRequest::getPhysicalOutput(SFPOutput* output,
                                                 MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  const SFPIOMap& phyIO = mSFPIOManager.getPhysicalIO(sensorID);
  MBOOL ret = getOutBuffer(phyIO, IO_TYPE_PHYSICAL, output);
  if (!ret) {
    TRACE_FUNC("frame %d: No physical buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureRequest::getLargeOutputs(std::vector<SFPOutput>* outList,
                                               MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  const SFPIOMap& largeIO = mSFPIOManager.getLargeIO(sensorID);
  if (largeIO.isValid()) {
    largeIO.getAllOutput(outList);
    ret = MTRUE;
  }
  if (!ret) {
    TRACE_FUNC("frame %d: No Large buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureRequest::getFDOutput(SFPOutput* output) {
  TRACE_FUNC_ENTER();
  const SFPIOMap& generalIO = mSFPIOManager.getFirstGeneralIO();
  MBOOL ret = getOutBuffer(generalIO, IO_TYPE_FD, output);
  if (!ret) {
    TRACE_FUNC("frame %d: No FD buffer", mRequestNo);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

IImageBuffer* StreamingFeatureRequest::getRecordOutputBuffer() {
  SFPOutput output;
  IImageBuffer* outputBuffer = nullptr;
  if (getRecordOutput(&output)) {
    outputBuffer = output.mBuffer;
  }
  return outputBuffer;
}

std::shared_ptr<IIBuffer> StreamingFeatureRequest::requestNextFullImg(
    std::shared_ptr<StreamingFeatureNode> node, MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  if (mIORequestMap.count(sensorID) > 0 &&
      mIORequestMap.at(sensorID).needNextFull(node)) {
    return mIORequestMap.at(sensorID).getNextFullImg(node);
  }

  TRACE_FUNC_EXIT();
  return nullptr;
}

MBOOL StreamingFeatureRequest::needDisplayOutput(
    std::shared_ptr<StreamingFeatureNode> node) {
  return mMasterIOReq.needPreview(node);
}

MBOOL StreamingFeatureRequest::needRecordOutput(
    std::shared_ptr<StreamingFeatureNode> node) {
  return mMasterIOReq.needRecord(node);
}

MBOOL StreamingFeatureRequest::needExtraOutput(
    std::shared_ptr<StreamingFeatureNode> node) {
  return mMasterIOReq.needPreviewCallback(node);
}

MBOOL StreamingFeatureRequest::needFullImg(
    std::shared_ptr<StreamingFeatureNode> node, MUINT32 sensorID) {
  return (mIORequestMap.count(sensorID) > 0) &&
         (mIORequestMap.at(sensorID).needFull(node));
}

MBOOL StreamingFeatureRequest::needNextFullImg(
    std::shared_ptr<StreamingFeatureNode> node, MUINT32 sensorID) {
  return (mIORequestMap.count(sensorID) > 0) &&
         (mIORequestMap.at(sensorID).needNextFull(node));
}

MBOOL StreamingFeatureRequest::needPhysicalOutput(
    std::shared_ptr<StreamingFeatureNode> node, MUINT32 sensorID) {
  return (mIORequestMap.count(sensorID) > 0) &&
         (mIORequestMap.at(sensorID).needPhysicalOut(node));
}

MBOOL StreamingFeatureRequest::hasGeneralOutput() const {
  return mHasGeneralOutput;
}

MBOOL StreamingFeatureRequest::hasDisplayOutput() const {
  return existOutBuffer(mSFPIOManager.getFirstGeneralIO(), IO_TYPE_DISPLAY);
}

MBOOL StreamingFeatureRequest::hasRecordOutput() const {
  return existOutBuffer(mSFPIOManager.getFirstGeneralIO(), IO_TYPE_RECORD);
}

MBOOL StreamingFeatureRequest::hasExtraOutput() const {
  return existOutBuffer(mSFPIOManager.getFirstGeneralIO(), IO_TYPE_EXTRA);
}

MBOOL StreamingFeatureRequest::hasPhysicalOutput(MUINT32 sensorID) const {
  return mSFPIOManager.getPhysicalIO(sensorID).isValid();
}

MBOOL StreamingFeatureRequest::hasLargeOutput(MUINT32 sensorID) const {
  return mSFPIOManager.getLargeIO(sensorID).isValid();
}
SrcCropInfo StreamingFeatureRequest::getSrcCropInfo(MUINT32 sensorID) {
  if (mNonLargeSrcCrops.count(sensorID) == 0) {
    MY_LOGW("sID(%d) srcCropInfo not exist!, create dummy.", sensorID);
    mNonLargeSrcCrops[sensorID] = SrcCropInfo();
  }
  return mNonLargeSrcCrops.at(sensorID);
}

MVOID StreamingFeatureRequest::setDumpProp(MINT32 start,
                                           MINT32 count,
                                           MBOOL byRecordNo) {
  MINT32 debugDumpNo = byRecordNo ? mRecordNo : mRequestNo;
  mNeedDump = (start < 0) || (((MINT32)debugDumpNo >= start) &&
                              (((MINT32)debugDumpNo - start) < count));
}

MVOID StreamingFeatureRequest::setForceIMG3O(MBOOL forceIMG3O) {
  mForceIMG3O = forceIMG3O;
}

MVOID StreamingFeatureRequest::setForceWarpPass(MBOOL forceWarpPass) {
  mForceWarpPass = forceWarpPass;
}

MVOID StreamingFeatureRequest::setForceGpuOut(MUINT32 forceGpuOut) {
  mForceGpuOut = forceGpuOut;
}

MVOID StreamingFeatureRequest::setForceGpuRGBA(MBOOL forceGpuRGBA) {
  mForceGpuRGBA = forceGpuRGBA;
}

MVOID StreamingFeatureRequest::setForcePrintIO(MBOOL forcePrintIO) {
  mForcePrintIO = forcePrintIO;
}

MBOOL StreamingFeatureRequest::isForceIMG3O() const {
  return mForceIMG3O;
}

MBOOL StreamingFeatureRequest::hasSlave(MUINT32 sensorID) const {
  return (mSlaveParamMap.count(sensorID) != 0);
}

FeaturePipeParam& StreamingFeatureRequest::getSlave(MUINT32 sensorID) {
  if (hasSlave(sensorID)) {
    return mSlaveParamMap.at(sensorID);
  } else {
    MY_LOGE("sensor(%d) has no slave FeaturePipeParam !! create Dummy",
            sensorID);
    mSlaveParamMap[sensorID] = FeaturePipeParam();
    return mSlaveParamMap.at(sensorID);
  }
}

const SFPSensorInput& StreamingFeatureRequest::getSensorInput(
    MUINT32 sensorID) const {
  return mSFPIOManager.getInput(sensorID);
}

VarMap& StreamingFeatureRequest::getSensorVarMap(MUINT32 sensorID) {
  if (sensorID == mMasterID) {
    return mVarMap;
  } else {
    return mSlaveParamMap[sensorID].mVarMap;
  }
}

MBOOL StreamingFeatureRequest::getMasterFrameBasic(
    NSCam::NSIoPipe::FrameParams* output) {
  output->UniqueKey = mRequestNo;
  output->mSensorIdx = getMasterID();
  output->mStreamTag = NSCam::v4l2::ENormalStreamTag_Normal;
  if (mPipeUsage.isQParamIOValid()) {
    if (mQParams.mvFrameParams.size()) {
      output->FrameNo = mQParams.mvFrameParams[0].FrameNo;
      output->RequestNo = mQParams.mvFrameParams[0].RequestNo;
      output->Timestamp = mQParams.mvFrameParams[0].Timestamp;
      return MTRUE;
    }
    MY_LOGE("QParamValid = true but w/o any frame param exist!");
    return MFALSE;
  } else {
    if (mExtParam.mP2Pack.isValid()) {
      output->FrameNo = mExtParam.mP2Pack.getFrameData().mMWFrameNo;
      output->RequestNo = mExtParam.mP2Pack.getFrameData().mMWFrameRequestNo;
      output->Timestamp = mExtParam.mP2Pack.getSensorData().mP1TS;
      return MTRUE;
    }
    MY_LOGE("QParamValid = false but w/o valid P2Pack!");
    return MFALSE;
  }
}

// Legacy code for Hal1, w/o dynamic tuning & P2Pack
MBOOL StreamingFeatureRequest::getMasterFrameTuning(
    NSCam::NSIoPipe::FrameParams* output) {
  if (mQParams.mvFrameParams.size()) {
    output->mTuningData = mQParams.mvFrameParams.at(0).mTuningData;
    output->mvModuleData = mQParams.mvFrameParams.at(0).mvModuleData;
    return MTRUE;
  }
  return MFALSE;
}

MBOOL StreamingFeatureRequest::getMasterFrameInput(
    NSCam::NSIoPipe::FrameParams* output) {
  if (mQParams.mvFrameParams.size()) {
    output->mvIn = mQParams.mvFrameParams.at(0).mvIn;
    return MTRUE;
  }
  return MFALSE;
}

const char* StreamingFeatureRequest::getFeatureMaskName() const {
  std::unordered_map<MUINT32, std::string>::const_iterator iter =
      mFeatureMaskNameMap.find(mFeatureMask);

  if (iter == mFeatureMaskNameMap.end()) {
    std::string str;
    append3DNRTag(&str, mFeatureMask);
    appendNoneTag(&str, mFeatureMask);
    appendDefaultTag(&str, mFeatureMask);

    iter = mFeatureMaskNameMap.insert(std::make_pair(mFeatureMask, str)).first;
  }

  return iter->second.c_str();
}
MBOOL StreamingFeatureRequest::need3DNR() const {
  return HAS_3DNR(mFeatureMask);
}
MBOOL StreamingFeatureRequest::needFullImg() const {
  return MTRUE;
}
MBOOL StreamingFeatureRequest::needDump() const {
  return mNeedDump;
}
MBOOL StreamingFeatureRequest::needNddDump() const {
  return (mP2DumpType == Feature::P2Util::P2_DUMP_NDD) && mP2Pack.isValid();
}

MBOOL StreamingFeatureRequest::isLastNodeP2A() const {
  return MTRUE;  //! HAS_EIS(mFeatureMask) ;
}

MBOOL StreamingFeatureRequest::is4K2K() const {
  return mIs4K2K;
}

MUINT32 StreamingFeatureRequest::getMasterID() const {
  return mMasterID;
}

MUINT32 StreamingFeatureRequest::needTPILog() const {
  return mTPILog;
}

MUINT32 StreamingFeatureRequest::needTPIDump() const {
  return mTPIDump;
}

MUINT32 StreamingFeatureRequest::needTPIScan() const {
  return mTPIScan;
}

MUINT32 StreamingFeatureRequest::needTPIBypass() const {
  return mTPIBypass;
}

MBOOL StreamingFeatureRequest::isSlaveParamValid() const {
  return mSlaveID != INVALID_SENSOR && hasSlave(mSlaveID);
}
MBOOL StreamingFeatureRequest::isP2ACRZMode() const {
  return mIsP2ACRZMode;
}
MBOOL StreamingFeatureRequest::useWarpPassThrough() const {
  return mForceWarpPass;
}

MBOOL StreamingFeatureRequest::useDirectGpuOut() const {
  MBOOL val = MFALSE;
  if (mForceGpuRGBA == MFALSE) {
    if (mForceGpuOut) {
      val = (mForceGpuOut == FORCE_ON);
    } else {
      val = this->is4K2K();
    }
  }
  return val;
}

MBOOL StreamingFeatureRequest::needPrintIO() const {
  return mForcePrintIO;
}

MBOOL StreamingFeatureRequest::getCropInfo(
    NSCam::NSIoPipe::EPortCapbility cap,
    MUINT32 defCropGroup,
    NSCam::NSIoPipe::MCrpRsInfo* crop /*, MRectF *cropF*/) {
  TRACE_FUNC_ENTER();
  unsigned count = 0;
  MUINT32 cropGroup = defCropGroup;

  if (mQParams.mvFrameParams.size()) {
    if (cropGroup != IMG2O_CROP_GROUP) {
      for (unsigned i = 0, size = mQParams.mvFrameParams[0].mvOut.size();
           i < size; ++i) {
        if (mQParams.mvFrameParams[0].mvOut[i].mPortID.capbility == cap) {
          switch (mQParams.mvFrameParams[0].mvOut[i].mPortID.index) {
            case NSImageio::NSIspio::EPortIndex_WDMAO:
              cropGroup = WDMAO_CROP_GROUP;
              break;
            case NSImageio::NSIspio::EPortIndex_WROTO:
              cropGroup = WROTO_CROP_GROUP;
              break;
          }
        }
      }
    }

    TRACE_FUNC("wanted crop group = %d, found group = %d", defCropGroup,
               cropGroup);

    for (unsigned i = 0, size = mQParams.mvFrameParams[0].mvCropRsInfo.size();
         i < size; ++i) {
      if (mQParams.mvFrameParams[0].mvCropRsInfo[i].mGroupID ==
          (MINT32)cropGroup) {
        if (++count == 1) {
          *crop = mQParams.mvFrameParams[0].mvCropRsInfo[i];
          TRACE_FUNC("Found crop(%d): %dx%d", crop->mGroupID,
                     crop->mCropRect.s.w, crop->mCropRect.s.h);
        }
      }
    }
  }

  if (count > 1) {
    TRACE_FUNC("frame %d: suspicious crop(ask/found: %d/%d) number = %d",
               mRequestNo, defCropGroup, cropGroup, count);
  }
  TRACE_FUNC_EXIT();
  return count >= 1;
}
void StreamingFeatureRequest::append3DNRTag(std::string* str,
                                            MUINT32 mFeatureMask) const {
  if (HAS_3DNR(mFeatureMask)) {
    if (!str->empty()) {
      *str += "+";
    }
    if (HAS_3DNR_RSC(mFeatureMask)) {
      *str += TAG_3DNR_RSC();
    } else {
      *str += TAG_3DNR();
    }
  }
}
void StreamingFeatureRequest::appendNoneTag(std::string* str,
                                            MUINT32 mFeatureMask) const {
  if (mFeatureMask == 0) {
    *str += "NONE";
  }
}

void StreamingFeatureRequest::appendDefaultTag(std::string* str,
                                               MUINT32 mFeatureMask) const {
  (void)(mFeatureMask);
  if (str->empty()) {
    *str += "UNKNOWN";
  }
}

MVOID FEFMGroup::clear() {
  this->High = NULL;
  this->Medium = NULL;
  this->Low = NULL;
}

MBOOL FEFMGroup::isValid() const {
  return this->High != NULL;
}

RSCResult::RSCResult() : mMV(NULL), mBV(NULL), mIsValid(MFALSE) {}

RSCResult::RSCResult(const ImgBuffer& mv,
                     const ImgBuffer& bv,
                     const MSize& rssoSize,
                     const RSC_STA_0& rscSta,
                     MBOOL valid)
    : mMV(mv), mBV(bv), mRssoSize(rssoSize), mRscSta(rscSta), mIsValid(valid) {}

BasicImg::BasicImg()
    : mBuffer(NULL),
      mDomainTransformScale(MSizeF(1.0f, 1.0f)),
      mIsReady(MTRUE) {}

BasicImg::BasicImg(const ImgBuffer& img)
    : mBuffer(img),
      mDomainTransformScale(MSizeF(1.0f, 1.0f)),
      mIsReady(MTRUE) {}

BasicImg::BasicImg(const ImgBuffer& img, const MPointF& offset)
    : mBuffer(img),
      mDomainOffset(offset),
      mDomainTransformScale(MSizeF(1.0f, 1.0f)),
      mIsReady(MTRUE) {}

BasicImg::BasicImg(const ImgBuffer& img,
                   const MPointF& offset,
                   const MBOOL& isReady)
    : mBuffer(img),
      mDomainOffset(offset),
      mDomainTransformScale(MSizeF(1.0f, 1.0f)),
      mIsReady(isReady) {}

MVOID BasicImg::setDomainInfo(const BasicImg& img) {
  mDomainOffset = img.mDomainOffset;
  mDomainTransformScale = img.mDomainTransformScale;
}

MBOOL BasicImg::syncCache(NSCam::eCacheCtrl ctrl) {
  return (mBuffer != NULL) && mBuffer->syncCache(ctrl);
}

DualBasicImg::DualBasicImg() {}

DualBasicImg::DualBasicImg(const BasicImg& master) : mMaster(master) {}

DualBasicImg::DualBasicImg(const BasicImg& master, const BasicImg& slave)
    : mMaster(master), mSlave(slave) {}

HelpReq::HelpReq() {}

HelpReq::HelpReq(FeaturePipeParam::MSG_TYPE msg) : mCBMsg(msg) {}

HelpReq::HelpReq(FeaturePipeParam::MSG_TYPE msg, INTERNAL_MSG_TYPE intMsg)
    : mCBMsg(msg), mInternalMsg(intMsg) {}
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
