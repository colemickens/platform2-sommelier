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
#define PIPE_CLASS_TAG "Pipe_1"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_PIPE
#include <featurePipe/common/include/PipeLog.h>
#include <featurePipe/streaming/StreamingFeaturePipe.h>
#include <map>
#include <memory>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
#include <set>
#include <string>
#include <src/pass2/NormalStream.h>

#define NORMAL_STREAM_NAME "StreamingFeature"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

StreamingFeaturePipe::StreamingFeaturePipe(MUINT32 sensorIndex,
                                           const UsageHint& usageHint)
    : CamPipe<StreamingFeatureNode>("StreamingFeaturePipe"),
      mForceOnMask(0),
      mForceOffMask(~0),
      mSensorIndex(sensorIndex),
      mPipeUsage(usageHint, sensorIndex),
      mCounter(0),
      mRecordCounter(0),
      mDebugDump(0),
      mDebugDumpCount(1),
      mDebugDumpByRecordNo(MFALSE),
      mForceIMG3O(MFALSE),
      mForceWarpPass(MFALSE),
      mForceGpuOut(NO_FORCE),
      mForceGpuRGBA(MFALSE),
      mUsePerFrameSetting(MFALSE),
      mForcePrintIO(MFALSE),
      mEarlyInited(MFALSE) {
  TRACE_FUNC_ENTER();
  mRootNode = std::make_shared<RootNode>("fpipe.root");
  mP2A = std::make_shared<P2ANode>("fpipe.p2a");
#if MTK_DP_ENABLE
  mP2AMDP = std::make_shared<mP2AMDP>("fpipe.p2amdp");
#endif
  mHelper = std::make_shared<HelperNode>("fpipe.helper");

  mAllSensorIDs = mPipeUsage.getAllSensorIDs();
  mNodeSignal = std::make_shared<NodeSignal>();
  if (mNodeSignal == nullptr) {
    MY_LOGE("OOM: cannot create NodeSignal");
  }

  for (int i = 0; i < P2CamContext::SENSOR_INDEX_MAX; i++) {
    mContextCreated[i] = MFALSE;
  }

  mEarlyInited = earlyInit();
  TRACE_FUNC_EXIT();
}

StreamingFeaturePipe::~StreamingFeaturePipe() {
  TRACE_FUNC_ENTER();
  MY_LOGD("destroy pipe(%p): SensorIndex=%d", this, mSensorIndex);
  lateUninit();
  // must call dispose to free CamGraph
  this->dispose();
  TRACE_FUNC_EXIT();
}

void StreamingFeaturePipe::setSensorIndex(MUINT32 sensorIndex) {
  TRACE_FUNC_ENTER();
  this->mSensorIndex = sensorIndex;
  TRACE_FUNC_EXIT();
}

MBOOL StreamingFeaturePipe::init(const char* name) {
  TRACE_FUNC_ENTER();
  (void)name;
  MBOOL ret = MFALSE;

  initNodes();

  ret = PARENT_PIPE::init();
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::config(const StreamConfigure config) {
  mP2A->configNormalStream(config);
  return MTRUE;
}

MBOOL StreamingFeaturePipe::uninit(const char* name) {
  TRACE_FUNC_ENTER();
  (void)name;
  MBOOL ret;
  ret = PARENT_PIPE::uninit();

  uninitNodes();

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::enque(const FeaturePipeParam& param) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (mPipeUsage.isDynamicTuning() && !param.mP2Pack.isValid()) {
    MY_LOGE("Dynamic Tuning w/o valid P2Pack!! Directly assert!");
    return MFALSE;
  }
  this->prepareFeatureRequest(param);
  RequestPtr request = std::make_shared<StreamingFeatureRequest>(
      mPipeUsage, param, mCounter, mRecordCounter);
  if (request == nullptr) {
    MY_LOGE("OOM: Cannot allocate StreamingFeatureRequest");
  } else {
    request->updateSFPIO();
    request->calSizeInfo();
    request->setDisplayFPSCounter(mDisplayFPSCounter);
    request->setFrameFPSCounter(mFrameFPSCounter);

    if (mUsePerFrameSetting) {
      this->prepareDebugSetting();
    }
    this->applyMaskOverride(request);
    this->applyVarMapOverride(request);
    mNodeSignal->clearStatus(NodeSignal::STATUS_IN_FLUSH);
    prepareIORequest(request);
    ret = CamPipe::enque(ID_ROOT_ENQUE, &request);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::flush() {
  TRACE_FUNC_ENTER();
  MY_LOGD("Trigger flush");
  mNodeSignal->setStatus(NodeSignal::STATUS_IN_FLUSH);
  CamPipe::sync();
  mNodeSignal->clearStatus(NodeSignal::STATUS_IN_FLUSH);
  TRACE_FUNC_EXIT();
  return MTRUE;
}
MBOOL StreamingFeaturePipe::sendCommand(NSCam::v4l2::ESDCmd cmd,
                                        MINTPTR arg1,
                                        MINTPTR arg2,
                                        MINTPTR arg3) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (mNormalStream != NULL) {
    ret = mNormalStream->sendCommand(cmd, arg1, arg2, arg3);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::addMultiSensorID(MUINT32 sensorID) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;

  if (sensorID < P2CamContext::SENSOR_INDEX_MAX) {
    std::lock_guard<std::mutex> lock(mContextMutex);
    if (!mContextCreated[sensorID]) {
      P2CamContext::createInstance(sensorID, mPipeUsage);
      mContextCreated[sensorID] = MTRUE;
      ret = MTRUE;
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MVOID StreamingFeaturePipe::sync() {
  TRACE_FUNC_ENTER();
  MY_LOGD("Sync start");
  CamPipe::sync();
  MY_LOGD("Sync finish");
  TRACE_FUNC_EXIT();
}

IImageBuffer* StreamingFeaturePipe::requestBuffer() {
  TRACE_FUNC_ENTER();
  IImageBuffer* buffer = nullptr;
  buffer = mInputBufferStore.requestBuffer();
  TRACE_FUNC_EXIT();
  return buffer;
}

MBOOL StreamingFeaturePipe::returnBuffer(IImageBuffer* buffer) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  ret = mInputBufferStore.returnBuffer(buffer);
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::onInit() {
  TRACE_FUNC_ENTER();
  MY_LOGI("+");
  MBOOL ret;
  ret = mEarlyInited && this->prepareDebugSetting() &&
        this->prepareNodeSetting() && this->prepareNodeConnection() &&
        this->prepareIOControl() && this->prepareBuffer() &&
        this->prepareCamContext();

  MY_LOGI("-");
  TRACE_FUNC_EXIT();
  return ret;
}

MVOID StreamingFeaturePipe::onUninit() {
  TRACE_FUNC_ENTER();
  MY_LOGI("+");
  this->releaseCamContext();
  this->releaseBuffer();
  this->releaseNodeSetting();
  MY_LOGI("-");
  TRACE_FUNC_EXIT();
}

MBOOL StreamingFeaturePipe::onData(DataID, const RequestPtr&) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::earlyInit() {
  return this->prepareGeneralPipe();
}

MVOID StreamingFeaturePipe::lateUninit() {
  this->releaseGeneralPipe();
}

MBOOL StreamingFeaturePipe::initNodes() {
  TRACE_FUNC_ENTER();

  mNodes.push_back(mRootNode);
  mNodes.push_back(mP2A);
#if MTK_DP_ENABLE
  mNodes.push_back(mP2AMDP);
#endif
  mNodes.push_back(mHelper);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL StreamingFeaturePipe::uninitNodes() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL StreamingFeaturePipe::prepareDebugSetting() {
  TRACE_FUNC_ENTER();

  mForceOnMask = 0;
  mForceOffMask = ~0;

#define CHECK_DEBUG_MASK(name)                                          \
  {                                                                     \
    MINT32 prop = getPropertyValue(KEY_FORCE_##name, VAL_FORCE_##name); \
    if (prop == FORCE_ON)                                               \
      ENABLE_##name(mForceOnMask);                                      \
    if (prop == FORCE_OFF)                                              \
      DISABLE_##name(mForceOffMask);                                    \
  }
  CHECK_DEBUG_MASK(3DNR);
  CHECK_DEBUG_MASK(DUMMY);
#undef CHECK_DEBUG_SETTING

  mDebugDump = getPropertyValue(KEY_DEBUG_DUMP, VAL_DEBUG_DUMP);
  mDebugDumpCount =
      getPropertyValue(KEY_DEBUG_DUMP_COUNT, VAL_DEBUG_DUMP_COUNT);
  mDebugDumpByRecordNo =
      getPropertyValue(KEY_DEBUG_DUMP_BY_RECORDNO, VAL_DEBUG_DUMP_BY_RECORDNO);
  mForceIMG3O = getPropertyValue(KEY_FORCE_IMG3O, VAL_FORCE_IMG3O);
  mForceWarpPass = getPropertyValue(KEY_FORCE_WARP_PASS, VAL_FORCE_WARP_PASS);
  mForceGpuOut = getPropertyValue(KEY_FORCE_GPU_OUT, VAL_FORCE_GPU_OUT);
  mForceGpuRGBA = getPropertyValue(KEY_FORCE_GPU_RGBA, VAL_FORCE_GPU_RGBA);
  mUsePerFrameSetting =
      getPropertyValue(KEY_USE_PER_FRAME_SETTING, VAL_USE_PER_FRAME_SETTING);
  mForcePrintIO = getPropertyValue(KEY_FORCE_PRINT_IO, VAL_FORCE_PRINT_IO);

  if (!mPipeUsage.support3DNR()) {
    DISABLE_3DNR(mForceOffMask);
  }
  MY_LOGD("forceOnMask=0x%04x, forceOffMask=0x%04x", mForceOnMask,
          ~mForceOffMask);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL StreamingFeaturePipe::prepareGeneralPipe() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  P2_CAM_TRACE_CALL(TRACE_DEFAULT);
  std::map<NSIoPipe::EDIPInfoEnum, MUINT32> dipInfo;
  mDipVersion = dipInfo[NSIoPipe::EDIPINFO_DIPVERSION];
  if (!mPipeUsage.supportBypassP2A()) {
    mNormalStream = std::make_shared<NSCam::v4l2::NormalStream>(mSensorIndex);
    mP2A->setNormalStream(mNormalStream, mDipVersion);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeaturePipe::prepareNodeSetting() {
  TRACE_FUNC_ENTER();
  NODE_LIST::iterator it, end;
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    (*it)->setSensorIndex(mSensorIndex);
    (*it)->setPipeUsage(mPipeUsage);
    (*it)->setNodeSignal(mNodeSignal);
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL StreamingFeaturePipe::prepareNodeConnection() {
  TRACE_FUNC_ENTER();

  this->connectData(ID_ROOT_TO_P2A, mRootNode, mP2A);
  this->connectData(ID_P2A_TO_HELPER, mP2A, mHelper);
#if MTK_DP_ENABLE
  this->connectData(ID_P2A_TO_PMDP, mP2A, mP2AMDP);
  this->connectData(ID_PMDP_TO_HELPER, mP2AMDP, mHelper);
#endif
  this->setRootNode(mRootNode);
  mRootNode->registerInputDataID(ID_ROOT_ENQUE);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL StreamingFeaturePipe::prepareIOControl() {
  TRACE_FUNC_ENTER();

  std::shared_ptr<StreamingFeatureNode> rootN = mP2A;

  mRecordPath.push_back(rootN);
  mDisplayPath.push_back(rootN);
  mPhysicalPath.push_back(rootN);

  mIOControl.setRoot(rootN);
  mIOControl.addStream(STREAMTYPE_PREVIEW, mDisplayPath);
  mIOControl.addStream(STREAMTYPE_RECORD, mRecordPath);
  mIOControl.addStream(STREAMTYPE_PREVIEW_CALLBACK, mDisplayPath);
  mIOControl.addStream(STREAMTYPE_PHYSICAL, mPhysicalPath);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL StreamingFeaturePipe::prepareBuffer() {
  TRACE_FUNC_ENTER();

  MSize fullSize(MAX_FULL_WIDTH, MAX_FULL_HEIGHT);
  MSize streamingSize = mPipeUsage.getStreamingSize();

  if (streamingSize.w > 0 && streamingSize.h > 0) {
    // align 64
    fullSize.w = align(streamingSize.w, 6);
    // IMG3O cann't rotate, the h don't need ALIGN64
    fullSize.h = streamingSize.h;
  }

  MY_LOGD("sensor(%d) StreamingSize=(%dx%d) align64=(%dx%d)", mSensorIndex,
          streamingSize.w, streamingSize.h, fullSize.w, fullSize.h);

  if (mPipeUsage.supportP2AFeature()) {
    mFullImgPool = createFullImgPool("fpipe.fullImg", fullSize);
    mP2A->setFullImgPool(mFullImgPool, mPipeUsage.getNumP2ABuffer());
    mInputBufferStore.init(mFullImgPool);
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

std::shared_ptr<IBufferPool> StreamingFeaturePipe::createFullImgPool(
    const char* name, MSize size) {
  TRACE_FUNC_ENTER();

  std::shared_ptr<IBufferPool> fullImgPool;
  EImageFormat format = mPipeUsage.getFullImgFormat();
  fullImgPool = ImageBufferPool::create(name, size.w, size.h, format,
                                        ImageBufferPool::USAGE_HW);
  TRACE_FUNC_EXIT();

  return fullImgPool;
}

MVOID StreamingFeaturePipe::createPureImgPools(const char* name, MSize size) {
  TRACE_FUNC_ENTER();
  for (MUINT32 sId : mAllSensorIDs) {
    // TODO(MTK): format need sync
    mPureImgPoolMap[sId] = createFullImgPool(name, size);
  }
  TRACE_FUNC_EXIT();
}

std::shared_ptr<IBufferPool> StreamingFeaturePipe::createImgPool(
    const char* name, MSize size, EImageFormat fmt) {
  TRACE_FUNC_ENTER();

  std::shared_ptr<IBufferPool> pool;
  pool = ImageBufferPool::create(name, size.w, size.h, fmt,
                                 ImageBufferPool::USAGE_HW);
  TRACE_FUNC_EXIT();
  return pool;
}

MVOID StreamingFeaturePipe::releaseNodeSetting() {
  TRACE_FUNC_ENTER();
  this->disconnect();
  mDisplayPath.clear();
  mRecordPath.clear();
  mPhysicalPath.clear();
  TRACE_FUNC_EXIT();
}

MVOID StreamingFeaturePipe::releaseGeneralPipe() {
  P2_CAM_TRACE_CALL(TRACE_DEFAULT);
  TRACE_FUNC_ENTER();
  mP2A->setNormalStream(NULL, mDipVersion);
  if (mNormalStream) {
    mNormalStream->uninit(NORMAL_STREAM_NAME);
  }
  TRACE_FUNC_EXIT();
}

MVOID StreamingFeaturePipe::releaseBuffer() {
  TRACE_FUNC_ENTER();

  mP2A->setFullImgPool(NULL);
  mInputBufferStore.uninit();

  IBufferPool::destroy(&mFullImgPool);

  TRACE_FUNC_EXIT();
}

MVOID StreamingFeaturePipe::applyMaskOverride(const RequestPtr& request) {
  TRACE_FUNC_ENTER();
  request->mFeatureMask |= mForceOnMask;
  request->mFeatureMask &= mForceOffMask;
  request->setDumpProp(mDebugDump, mDebugDumpCount, mDebugDumpByRecordNo);
  request->setForceIMG3O(mForceIMG3O);
  request->setForceWarpPass(mForceWarpPass);
  request->setForceGpuOut(mForceGpuOut);
  request->setForceGpuRGBA(mForceGpuRGBA);
  request->setForcePrintIO(mForcePrintIO);
  TRACE_FUNC_EXIT();
}

MVOID StreamingFeaturePipe::applyVarMapOverride(const RequestPtr& request) {
  TRACE_FUNC_ENTER();
  (void)(request);
  TRACE_FUNC_EXIT();
}

MBOOL StreamingFeaturePipe::prepareCamContext() {
  TRACE_FUNC_ENTER();
  for (auto&& id : mAllSensorIDs) {
    addMultiSensorID(id);
  }

  TRACE_FUNC_EXIT();

  return MTRUE;
}

MVOID StreamingFeaturePipe::prepareFeatureRequest(
    const FeaturePipeParam& param) {
  ++mCounter;
  eAppMode appMode = param.getVar<eAppMode>(VAR_APP_MODE, APP_PHOTO_PREVIEW);
  if (appMode == APP_VIDEO_RECORD || appMode == APP_VIDEO_STOP) {
    ++mRecordCounter;
  } else if (mRecordCounter) {
    MY_LOGI("Set Record Counter %d=>0. AppMode=%d", mRecordCounter, appMode);
    mRecordCounter = 0;
  }
  TRACE_FUNC("Request=%d, Record=%d, AppMode=%d", mCounter, mRecordCounter,
             appMode);
}

MVOID StreamingFeaturePipe::prepareIORequest(const RequestPtr& request) {
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);
  TRACE_FUNC_ENTER();
  {
    std::string dumpStr;
    request->mSFPIOManager.appendDumpInfo(&dumpStr);

    MY_LOGD(
        "master/slave(%d/%d) MWFrame:#%d, ReqNo(%d), feature=0x%04x(%s), "
        "SFPIOMgr:%s",
        request->getMasterID(), request->mSlaveID, request->mMWFrameNo,
        request->mRequestNo, request->mFeatureMask,
        request->getFeatureMaskName(), dumpStr.c_str());
  }

  std::set<StreamType> generalStreams;
  if (request->hasDisplayOutput()) {
    generalStreams.insert(STREAMTYPE_PREVIEW);
  }
  if (request->hasRecordOutput()) {
    generalStreams.insert(STREAMTYPE_RECORD);
  }
  if (request->hasExtraOutput()) {
    generalStreams.insert(STREAMTYPE_PREVIEW_CALLBACK);
  }

  {
    // Master
    prepareIORequest(request, generalStreams, request->mMasterID);
  }
  if (request->hasSlave(request->mSlaveID)) {
    prepareIORequest(request, generalStreams, request->mSlaveID);
  }

  TRACE_FUNC_EXIT();
}

MVOID StreamingFeaturePipe::prepareIORequest(
    const RequestPtr& request,
    const std::set<StreamType>& generalStreams,
    MUINT32 sensorID) {
  std::set<StreamType> streams = generalStreams;
  if (request->hasPhysicalOutput(sensorID)) {
    streams.insert(STREAMTYPE_PHYSICAL);
  }
  StreamingReqInfo reqInfo(request->mRequestNo, request->mFeatureMask,
                           request->mMasterID, sensorID);
  IORequest<StreamingFeatureNode, StreamingReqInfo>& ioReq =
      request->mIORequestMap[sensorID];
  mIOControl.prepareMap(streams, reqInfo, &(ioReq.mOutMap), &(ioReq.mBufMap));

  if (1) {
    MY_LOGD("IOUtil ReqInfo : %s", reqInfo.dump());
    mIOControl.printMap(&(ioReq.mOutMap));
    mIOControl.dumpInfo(&(ioReq.mOutMap));
    mIOControl.dumpInfo(&(ioReq.mBufMap));
  }
}

MVOID StreamingFeaturePipe::releaseCamContext() {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> lock(mContextMutex);
  for (int i = 0; i < P2CamContext::SENSOR_INDEX_MAX; i++) {
    if (mContextCreated[i]) {
      P2CamContext::destroyInstance(i);
      mContextCreated[i] = MFALSE;
    }
  }

  TRACE_FUNC_EXIT();
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
