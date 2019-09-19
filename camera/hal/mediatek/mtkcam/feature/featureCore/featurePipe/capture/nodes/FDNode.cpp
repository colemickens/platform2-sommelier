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

#define PIPE_CLASS_TAG "FDNode"
#define PIPE_TRACE TRACE_FD_NODE
#include <capture/nodes/FDNode.h>
#include <common/include/PipeLog.h>
#include <memory>

#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

#define FD_FACE_NUM (15)
#define FD_WORKING_BUF_SIZE (1024 * 1024 * 4)
#define FD_PURE_Y_BUF_SIZE (640 * 480 * 2)

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

FDNode::FDNode(NodeID_T nid, const char* name)
    : CaptureFeatureNode(nid, name),
      mpFDHalObj(NULL),
      mpFDWorkingBuffer(NULL),
      mpPureYBuffer(NULL),
      mpDetectedFaces(NULL) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mRequests);
  TRACE_FUNC_EXIT();
}

FDNode::~FDNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL FDNode::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  MY_LOGD_IF(mLogLevel, "Frame %d: %s arrived", pRequest->getRequestNo(),
             PathID2Name(id));

  MBOOL ret = MTRUE;
  mRequests.enque(pRequest);

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL FDNode::onInit() {
  TRACE_FUNC_ENTER();
  CaptureFeatureNode::onInit();

  mpDetectedFaces = new MtkCameraFaceMetadata();
  if (NULL != mpDetectedFaces) {
    MtkCameraFace* faces = new MtkCameraFace[FD_FACE_NUM];
    MtkFaceInfo* posInfo = new MtkFaceInfo[FD_FACE_NUM];

    if (NULL != faces && NULL != posInfo) {
      mpDetectedFaces->faces = faces;
      mpDetectedFaces->posInfo = posInfo;
      mpDetectedFaces->number_of_faces = 0;
    } else {
      MY_LOGE("fail to allocate face info buffer!");
      return UNKNOWN_ERROR;
    }
  } else {
    MY_LOGE("fail to allocate face metadata!");
    return UNKNOWN_ERROR;
  }

  mpFDWorkingBuffer = new unsigned char[FD_WORKING_BUF_SIZE];
  mpPureYBuffer = new unsigned char[FD_PURE_Y_BUF_SIZE];
  mpFDHalObj = halFDBase::createInstance(HAL_FD_OBJ_FDFT_SW);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL FDNode::onUninit() {
  TRACE_FUNC_ENTER();

  if (mpFDHalObj != NULL) {
    mpFDHalObj = NULL;
  }

  if (mpFDWorkingBuffer != NULL) {
    delete mpFDWorkingBuffer;
    mpFDWorkingBuffer = NULL;
  }

  if (mpPureYBuffer != NULL) {
    delete mpPureYBuffer;
    mpPureYBuffer = NULL;
  }

  if (mpDetectedFaces != NULL) {
    delete[] mpDetectedFaces->faces;
    delete[] mpDetectedFaces->posInfo;
    delete mpDetectedFaces;
    mpDetectedFaces = NULL;
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL FDNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL FDNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL FDNode::onThreadLoop() {
  TRACE_FUNC_ENTER();
  if (!waitAllQueue()) {
    TRACE_FUNC("Wait all queue exit");
    return MFALSE;
  }

  RequestPtr pRequest;
  if (!mRequests.deque(&pRequest)) {
    MY_LOGE("Request deque out of sync");
    return MFALSE;
  } else if (pRequest == NULL) {
    MY_LOGE("Request out of sync");
    return MFALSE;
  }

  pRequest->mTimer.startFD();
  { onRequestProcess(pRequest); }
  pRequest->mTimer.startFD();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL FDNode::onRequestProcess(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  CAM_TRACE_FMT_BEGIN("fd:process|r%df%d", requestNo, frameNo);
  MY_LOGD("+, R/F Num: %d/%d", requestNo, frameNo);

  std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq =
      pRequest->getNodeRequest(NID_FD);
  if (pNodeReq == NULL) {
    MY_LOGE("should not be here if no node request");
    return MFALSE;
  }

  // [1] acquire the FD buffer whose width must be not more than 640
  IImageBuffer* pInBuffer = nullptr;
  BufferID_T uFDBuffer = pNodeReq->mapBufferID(TID_MAIN_FD_YUV, INPUT);
  if (uFDBuffer != NULL_BUFFER) {
    pInBuffer = pNodeReq->acquireBuffer(uFDBuffer);
  }
  if (pInBuffer == nullptr) {
    MY_LOGE("cannot acquire FD buffer!");
    return MFALSE;
  }

  IMetadata* pAppMeta = pNodeReq->acquireMetadata(MID_MAIN_IN_APP);

  MINT32 jpegOrientation;
  if (!tryGetMetadata<MINT32>(pAppMeta, MTK_JPEG_ORIENTATION,
                              &jpegOrientation)) {
    MY_LOGE("cannot find MTK_JPEG_ORIENTATION in APP metadata!");
    return MFALSE;
  }

  MSize fdSize = pInBuffer->getImgSize();
  if (fdSize.w > 640) {
    MY_LOGE("can not support the buffer size(%dx%d)", fdSize.w, fdSize.h);
    return MFALSE;
  }

  // [2] FD init
  int ret = mpFDHalObj->halFDInit(fdSize.w, fdSize.h, 1, HAL_FD_MODE_MANUAL);

  if (ret) {
    MY_LOGE("fail to init FD object!");
    return MFALSE;
  }

  // [3] extract pure Y buffer
  mpFDHalObj->halFDYUYV2ExtractY(
      mpPureYBuffer, reinterpret_cast<MUINT8*>(pInBuffer->getBufVA(0)),
      fdSize.w, fdSize.h);

  struct FD_Frame_Parameters param;
  param.pScaleImages = NULL;
  // on FD HW Version above 4.2
  param.pRGB565Image = reinterpret_cast<MUINT8*>(pInBuffer->getBufVA(0));
  param.pPureYImage = mpPureYBuffer;
  param.pImageBufferVirtual = reinterpret_cast<MUINT8*>(pInBuffer->getBufVA(0));
  param.pImageBufferPhyP0 = reinterpret_cast<MUINT8*>(pInBuffer->getBufPA(0));
  param.pImageBufferPhyP1 = NULL;
  param.pImageBufferPhyP2 = NULL;
  param.Rotation_Info = jpegOrientation;
  param.SDEnable = 0;
  param.AEStable = 0;
  param.padding_w = 0;
  param.padding_h = 0;

  // [4] do face detection
  ret = mpFDHalObj->halFDDo(param);
  if (ret) {
    MY_LOGW("fail to do face dection. ret=%d", ret);
    return UNKNOWN_ERROR;
  }

  MINT32 numFace = mpFDHalObj->halFDGetFaceResult(mpDetectedFaces);

  // [5] FD uninit
  ret = mpFDHalObj->halFDUninit();
  if (ret) {
    MY_LOGE("fail to uninit FD object!");
    return UNKNOWN_ERROR;
  }

  MY_LOGD("fd result: orientation=%d,face num=%d", jpegOrientation, numFace);

  // [6] update into app metadata
  // TODO(MTK): only support full size coordinate for now
  MRect cropRegion = mpCropCalculator->getActiveArray();

  MY_LOGD("fd crop region(%d,%d)(%dx%d) from metadata", cropRegion.p.x,
          cropRegion.p.y, cropRegion.s.w, cropRegion.s.h);

  // Map to FD's coordinate
  int aspect = cropRegion.s.w * fdSize.h - cropRegion.s.h * fdSize.w;
  // Pillar
  if (aspect > 0) {
    int newCropW = div_round(cropRegion.s.h * fdSize.w, fdSize.h);
    cropRegion.p.x += (cropRegion.s.w - newCropW) >> 1;
    cropRegion.s.w = newCropW;
    // Letter
  } else if (aspect < 0) {
    int newCropH = div_round(cropRegion.s.w * fdSize.h, fdSize.w);
    cropRegion.p.y += (cropRegion.s.h - newCropH) >> 1;
    cropRegion.s.h = newCropH;
  }

  MRect faceRect;

  IMetadata::IEntry entryFaceRects(MTK_STATISTICS_FACE_RECTANGLES);
  IMetadata::IEntry entryFaceLandmarks(MTK_STATISTICS_FACE_LANDMARKS);
  IMetadata::IEntry entryFaceId(MTK_STATISTICS_FACE_IDS);
  IMetadata::IEntry entryFaceScores(MTK_STATISTICS_FACE_SCORES);
  IMetadata::IEntry entryPoseOriens(MTK_FACE_FEATURE_POSE_ORIENTATIONS);

  for (int i = 0; i < mpDetectedFaces->number_of_faces; i++) {
    MtkCameraFace& face = mpDetectedFaces->faces[i];

    // Face Rectangle
    faceRect.p.x = ((face.rect[0] + 1000) * cropRegion.s.w / 2000) +
                   cropRegion.p.x;  // left
    faceRect.p.y = ((face.rect[1] + 1000) * cropRegion.s.h / 2000) +
                   cropRegion.p.y;  // top
    faceRect.s.w = ((face.rect[2] + 1000) * cropRegion.s.w / 2000) +
                   cropRegion.p.x;  // right
    faceRect.s.h = ((face.rect[3] + 1000) * cropRegion.s.h / 2000) +
                   cropRegion.p.y;  // bottom

    entryFaceRects.push_back(faceRect, Type2Type<MRect>());

    MY_LOGD(
        "Detected Face Rect[%zd]: (xmin, ymin, xmax, ymax) => (%d, %d, %d, %d)",
        i, faceRect.p.x, faceRect.p.y, faceRect.s.w, faceRect.s.h);

    // Face Landmark
    entryFaceLandmarks.push_back(face.left_eye[0],
                                 Type2Type<MINT32>());  // left eye X
    entryFaceLandmarks.push_back(face.left_eye[1],
                                 Type2Type<MINT32>());  // left eye Y
    entryFaceLandmarks.push_back(face.right_eye[0],
                                 Type2Type<MINT32>());  // right eye X
    entryFaceLandmarks.push_back(face.right_eye[1],
                                 Type2Type<MINT32>());  // right eye Y
    entryFaceLandmarks.push_back(face.mouth[0],
                                 Type2Type<MINT32>());  // mouth X
    entryFaceLandmarks.push_back(face.mouth[1],
                                 Type2Type<MINT32>());  // mouth Y

    // Face ID
    entryFaceId.push_back(face.id, Type2Type<MINT32>());

    // Face Score
    if (face.score > 100) {
      face.score = 100;
    }
    entryFaceScores.push_back(face.score, Type2Type<MUINT8>());

    // Face Pose
    entryPoseOriens.push_back(0, Type2Type<MINT32>());  // pose X asix
    entryPoseOriens.push_back(mpDetectedFaces->fld_rop[i],
                              Type2Type<MINT32>());  // pose Y asix
    entryPoseOriens.push_back(mpDetectedFaces->fld_rip[i],
                              Type2Type<MINT32>());  // pose Z asix
  }

  pAppMeta->update(MTK_STATISTICS_FACE_RECTANGLES, entryFaceRects);
  pAppMeta->update(MTK_STATISTICS_FACE_LANDMARKS, entryFaceLandmarks);
  pAppMeta->update(MTK_STATISTICS_FACE_IDS, entryFaceId);
  pAppMeta->update(MTK_STATISTICS_FACE_SCORES, entryFaceScores);
  pAppMeta->update(MTK_FACE_FEATURE_POSE_ORIENTATIONS, entryPoseOriens);

  // Release
  pNodeReq->releaseMetadata(MID_MAIN_IN_APP);

  dispatch(pRequest);

  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
  CAM_TRACE_FMT_END();
  return ret;
}

MERROR FDNode::evaluate(CaptureFeatureInferenceData* rInfer) {
  auto& srcData = rInfer->getSharedSrcData();
  auto& dstData = rInfer->getSharedDstData();
  auto& features = rInfer->getSharedFeatures();
  auto& metadatas = rInfer->getSharedMetadatas();

  if (rInfer->hasType(TID_MAIN_FD_YUV)) {
    MSize srcSize;
    if (rInfer->hasType(TID_MAIN_FULL_RAW)) {
      srcSize = rInfer->getSize(TID_MAIN_FULL_RAW);
    } else if (rInfer->hasType(TID_MAIN_RSZ_RAW)) {
      srcSize = rInfer->getSize(TID_MAIN_RSZ_RAW);
    } else {
      return OK;
    }

    // FD requires a buffer whose size is less than 640P
    MSize fdSize;
    if (srcSize.w > 640) {
      fdSize.w = 640;
      fdSize.h = ALIGNX(div_round(srcSize.h * 640, srcSize.w), 1);
    } else {
      fdSize = srcSize;
    }
    srcData.emplace_back();
    auto& src_0 = srcData.back();
    src_0.mTypeId = TID_MAIN_FD_YUV;
    src_0.mSizeId = SID_SPECIFIC;
    src_0.mSize = fdSize;
    src_0.mFormat = eImgFmt_YUY2;

    dstData.emplace_back();
    auto& dst_0 = dstData.back();
    dst_0.mTypeId = TID_MAIN_FD;
    dst_0.mSizeId = NULL_SIZE;

    metadatas.push_back(MID_MAIN_IN_APP);

    rInfer->addNodeIO(NID_FD, srcData, dstData, metadatas, features);
  }

  return OK;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
