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

#define PIPE_CLASS_TAG "Node"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_NODE
#include <capture/CaptureFeatureNode.h>
#include <common/include/PipeLog.h>
#include <fcntl.h>
#include <memory>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

const char* CaptureFeatureDataHandler::ID2Name(DataID id) {
  return PathID2Name(id);
}

NodeSignal::NodeSignal() : mSignal(0), mStatus(0) {}

NodeSignal::~NodeSignal() {}

MVOID NodeSignal::setSignal(Signal signal) {
  std::lock_guard<std::mutex> lock(mMutex);
  mSignal |= signal;
  mCondition.notify_all();
}

MVOID NodeSignal::clearSignal(Signal signal) {
  std::lock_guard<std::mutex> lock(mMutex);
  mSignal &= ~signal;
}

MBOOL NodeSignal::getSignal(Signal signal) {
  std::lock_guard<std::mutex> lock(mMutex);
  return (mSignal & signal);
}

MVOID NodeSignal::waitSignal(Signal signal) {
  std::unique_lock<std::mutex> lck(mMutex);
  while (!(mSignal & signal)) {
    mCondition.wait(lck);
  }
}

MVOID NodeSignal::setStatus(Status status) {
  std::lock_guard<std::mutex> lock(mMutex);
  mStatus |= status;
}

MVOID NodeSignal::clearStatus(Status status) {
  std::lock_guard<std::mutex> lock(mMutex);
  mStatus &= ~status;
}

MBOOL NodeSignal::getStatus(Status status) {
  std::lock_guard<std::mutex> lock(mMutex);
  return (mStatus & status);
}

CaptureFeatureDataHandler::~CaptureFeatureDataHandler() {}

CaptureFeatureNode::CaptureFeatureNode(NodeID_T nid,
                                       const char* name,
                                       MUINT32 uLogLevel)
    : CamThreadNode(name),
      mSensorIndex(-1),
      mNodeId(nid),
      mLogLevel(uLogLevel) {}

CaptureFeatureNode::~CaptureFeatureNode() {}

MBOOL CaptureFeatureNode::onInit() {
  return MTRUE;
}

MVOID CaptureFeatureNode::setSensorIndex(MINT32 sensorIndex) {
  mSensorIndex = sensorIndex;
}

MVOID CaptureFeatureNode::setLogLevel(MUINT32 uLogLevel) {
  mLogLevel = uLogLevel;
}

MVOID CaptureFeatureNode::setNodeSignal(
    const std::shared_ptr<NodeSignal>& nodeSignal) {
  mNodeSignal = nodeSignal;
}

MVOID CaptureFeatureNode::setCropCalculator(
    const std::shared_ptr<CropCalculator>& rCropCalculator) {
  mpCropCalculator = rCropCalculator;
}

MVOID CaptureFeatureNode::dispatch(const RequestPtr& pRequest) {
  std::vector<NodeID_T> nextNodes = pRequest->getNextNodes(getNodeID());

  // the requiest is finished
  for (NodeID_T nodeId : nextNodes) {
    PathID_T pathId = FindPath(getNodeID(), nodeId);
    if (pathId != NULL_PATH) {
      pRequest->traverse(pathId);
      handleData(pathId, &pRequest);
      MY_LOGD_IF(mLogLevel, "traverse to %s", PathID2Name(pathId));
    }
  }

  if (nextNodes.size() == 0 && pRequest->isTraversed()) {
    handleData(PID_DEQUE, &pRequest);
    return;
  }
}

MBOOL CaptureFeatureNode::dumpData(const RequestPtr& request,
                                   std::shared_ptr<IImageBuffer> buffer,
                                   const char* fmt,
                                   ...) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (buffer && fmt) {
    char name[256];
    va_list ap;
    va_start(ap, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    if (0 >= vsnprintf(name, sizeof(name), fmt, ap)) {
      strncpy(name, "NA", sizeof(name));
      name[sizeof(name) - 1] = 0;
    }
#pragma clang diagnostic pop
    va_end(ap);
    ret = dumpNamedData(request, buffer, name);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL CaptureFeatureNode::dumpNamedData(const RequestPtr& request,
                                        std::shared_ptr<IImageBuffer> buffer,
                                        const char* name) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (buffer && name) {
    MUINT32 stride, pbpp, ibpp, width, height, size;
    stride = buffer->getBufStridesInBytes(0);
    pbpp = buffer->getPlaneBitsPerPixel(0);
    ibpp = buffer->getImgBitsPerPixel();
    size = buffer->getBufSizeInBytes(0);
    pbpp = pbpp ? pbpp : 8;
    width = stride * 8 / pbpp;
    width = width ? width : 1;
    ibpp = ibpp ? ibpp : 8;
    height = size / width;
    if (buffer->getPlaneCount() == 1) {
      height = height * 8 / ibpp;
    }

    char path[256];
    snprintf(path, sizeof(path), DUMP_PATH "/%04d_%s_%dx%d.bin",
             request->getRequestNo(), name, width, height);

    TRACE_FUNC("dump to %s", path);
    buffer->saveToFile(path);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MUINT32 CaptureFeatureNode::dumpData(const char* buffer,
                                     MUINT32 size,
                                     const char* filename) {
  uint32_t writeCount = 0;
  int fd = ::open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
  if (fd < 0) {
    MY_LOGE("Cannot create file [%s]", filename);
  } else {
    for (int cnt = 0, nw = 0; writeCount < size; ++cnt) {
      nw = ::write(fd, buffer + writeCount, size - writeCount);
      if (nw < 0) {
        MY_LOGE("Cannot write to file [%s]", filename);
        break;
      }
      writeCount += nw;
    }
    ::close(fd);
  }
  return writeCount;
}

MBOOL CaptureFeatureNode::loadData(std::shared_ptr<IImageBuffer> buffer,
                                   const char* filename) {
  MBOOL ret = MFALSE;
  if (buffer) {
    loadData(reinterpret_cast<char*>(buffer->getBufVA(0)), 0, filename);
    ret = MTRUE;
  }
  return MFALSE;
}

MUINT32 CaptureFeatureNode::loadData(char* buffer,
                                     size_t size,
                                     const char* filename) {
  uint32_t readCount = 0;
  int fd = ::open(filename, O_RDONLY);
  if (fd < 0) {
    MY_LOGE("Cannot open file [%s]", filename);
  } else {
    if (size == 0) {
      off_t readSize = ::lseek(fd, 0, SEEK_END);
      size = (readSize < 0) ? 0 : readSize;
      ::lseek(fd, 0, SEEK_SET);
    }
    for (int cnt = 0, nr = 0; readCount < size; ++cnt) {
      nr = ::read(fd, buffer + readCount, size - readCount);
      if (nr < 0) {
        MY_LOGE("Cannot read from file [%s]", filename);
        break;
      }
      readCount += nr;
    }
    ::close(fd);
  }
  return readCount;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
