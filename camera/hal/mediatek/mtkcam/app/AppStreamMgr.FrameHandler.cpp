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

#undef LOG_TAG
#define LOG_TAG "MtkCam/AppStreamMgr"
//
#include "MyUtils.h"
#include "AppStreamMgr.h"
//
#include <sys/prctl.h>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Imp::AppStreamMgr::FrameHandler::FrameHandler(
    std::shared_ptr<IMetadataProvider> pMetadataProvider, MBOOL isExternal)
    : mpMetadataProvider(pMetadataProvider), mIsExternal(isExternal) {
  IMetadata::IEntry const& entry =
      mpMetadataProvider->getMtkStaticCharacteristics().entryFor(
          MTK_REQUEST_PARTIAL_RESULT_COUNT);
  if (entry.isEmpty()) {
    MY_LOGE("no static REQUEST_PARTIAL_RESULT_COUNT");
    mAtMostMetaStreamCount = 1;
  } else {
    mAtMostMetaStreamCount = entry.itemAt(0, Type2Type<MINT32>());
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Imp::AppStreamMgr::FrameHandler::isEmptyFrameQueue() const {
  return mFrameQueue.empty();
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Imp::AppStreamMgr::FrameHandler::getFrameQueueSize() const {
  return mFrameQueue.size();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Imp::AppStreamMgr::FrameHandler::queryOldestRequestNumber(
    MUINT32* ReqNo) const {
  if (mFrameQueue.empty()) {
    return -ENODATA;
  }
  FrameQueue::const_iterator itFrame = mFrameQueue.begin();
  *ReqNo = (*itFrame)->frameNo;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::addConfigStream(
    std::shared_ptr<AppImageStreamInfo> pStreamInfo) {
  ImageConfigItem item;
  item.pStreamInfo = pStreamInfo;
  mImageConfigMap.emplace(pStreamInfo->getStreamId(), item);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::addConfigStream(
    std::shared_ptr<AppMetaStreamInfo> pStreamInfo) {
  MetaConfigItem item;
  item.pStreamInfo = pStreamInfo;
  mMetaConfigMap.emplace(pStreamInfo->getStreamId(), item);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Imp::AppStreamMgr::FrameHandler::getConfigStreams(
    ConfigAppStreams* rStreams) const {
  if (mMetaConfigMap.empty() || mImageConfigMap.empty()) {
    MY_LOGE("Bad mMetaConfigMap.size:%zu, mImageConfigMap.size:%zu",
            mMetaConfigMap.size(), mImageConfigMap.size());
    return -ENODEV;
  }
  //
  rStreams->vMetaStreams.clear();
  for (auto const& it : mMetaConfigMap) {
    rStreams->vMetaStreams.emplace(it.first, it.second.pStreamInfo);
  }
  //
  rStreams->vImageStreams.clear();
  for (auto const& it : mImageConfigMap) {
    rStreams->vImageStreams.emplace(it.first, it.second.pStreamInfo);
  }
  //
  IMetadata::IEntry const& entryMinDuration =
      mpMetadataProvider->getMtkStaticCharacteristics().entryFor(
          MTK_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
  if (entryMinDuration.isEmpty()) {
    MY_LOGE("no static MTK_SCALER_AVAILABLE_MIN_FRAME_DURATIONS");
    return OK;
  }
  IMetadata::IEntry const& entryStallDuration =
      mpMetadataProvider->getMtkStaticCharacteristics().entryFor(
          MTK_SCALER_AVAILABLE_STALL_DURATIONS);
  if (entryStallDuration.isEmpty()) {
    MY_LOGE("no static MTK_SCALER_AVAILABLE_STALL_DURATIONS");
    return OK;
  }
  //
  rStreams->vMinFrameDuration.clear();
  rStreams->vStallFrameDuration.clear();
  for (auto const& it : mImageConfigMap) {
    for (size_t j = 0; j < entryMinDuration.count(); j += 4) {
      if (entryMinDuration.itemAt(j, Type2Type<MINT64>()) ==
              it.second.pStreamInfo->getImgFormatToAlloc() &&
          entryMinDuration.itemAt(j + 1, Type2Type<MINT64>()) ==
              it.second.pStreamInfo->getImgSize().w &&
          entryMinDuration.itemAt(j + 2, Type2Type<MINT64>()) ==
              it.second.pStreamInfo->getImgSize().h) {
        rStreams->vMinFrameDuration.emplace(
            it.first, entryMinDuration.itemAt(j + 3, Type2Type<MINT64>()));
        rStreams->vStallFrameDuration.emplace(
            it.first, entryStallDuration.itemAt(j + 3, Type2Type<MINT64>()));
        MY_LOGI("format:%" PRId64 " size:(%" PRId64 ", %" PRId64
                ") min_duration:%" PRId64 ", stall_duration:%" PRId64,
                entryMinDuration.itemAt(j, Type2Type<MINT64>()),
                entryMinDuration.itemAt(j + 1, Type2Type<MINT64>()),
                entryMinDuration.itemAt(j + 2, Type2Type<MINT64>()),
                entryMinDuration.itemAt(j + 3, Type2Type<MINT64>()),
                entryStallDuration.itemAt(j + 3, Type2Type<MINT64>()));
        break;
      }
    }
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Imp::AppStreamMgr::AppMetaStreamInfo>
NSCam::v3::Imp::AppStreamMgr::FrameHandler::getConfigMetaStream(
    size_t index) const {
  auto iter = mMetaConfigMap.begin();
  for (; iter != mMetaConfigMap.end(); iter++) {
    if (std::distance(mMetaConfigMap.begin(), iter) == index) {
      break;
    }
  }
  return iter->second.pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Imp::AppStreamMgr::FrameHandler::registerFrame(
    Request const& rRequest) {
  std::shared_ptr<FrameParcel> pFrame = std::make_shared<FrameParcel>();
  mFrameQueue.push_back(pFrame);
  pFrame->frameNo = rRequest.frameNo;
  //
  //  Request::vInputImageBuffers
  //  Request::vOutputImageBuffers
  {
    registerStreamBuffers(rRequest.vOutputImageBuffers, pFrame,
                          &pFrame->vOutputImageItem);
    registerStreamBuffers(rRequest.vInputImageBuffers, pFrame,
                          &pFrame->vInputImageItem);
  }
  //
  //  Request::vInputMetaBuffers
  //  Request::vOutputMetaBuffers
  {
    registerStreamBuffers(rRequest.vInputMetaBuffers, pFrame,
                          &pFrame->vInputMetaItem);
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Imp::AppStreamMgr::FrameHandler::registerStreamBuffers(
    std::map<StreamId_T, std::shared_ptr<AppImageStreamBuffer> > const& buffers,
    std::shared_ptr<FrameParcel> const pFrame,
    ImageItemSet* const pItemSet) {
  //  Request::v[Input|Output]ImageBuffers
  //  -> FrameParcel
  //  -> mImageConfigMap::vItemFrameQueue
  //
  for (auto& it : buffers) {
    std::shared_ptr<AppImageStreamBuffer> const pBuffer = it.second;
    //
    StreamId_T const streamId = pBuffer->getStreamInfo()->getStreamId();
    //
    auto iter = mImageConfigMap.find(streamId);
    if (iter == mImageConfigMap.end()) {
      MY_LOGE("[frameNo:%u] bad streamId:%#" PRIx64, pFrame->frameNo, streamId);
      return NAME_NOT_FOUND;
    }
    ImageItemFrameQueue& rItemFrameQueue = iter->second.vItemFrameQueue;
    //
    std::shared_ptr<ImageItem> pItem = std::make_shared<ImageItem>();
    //
    rItemFrameQueue.push_back(pItem);
    //
    pItem->pFrame = pFrame;
    pItem->pItemSet = pItemSet;
    pItem->buffer = pBuffer;
    pItem->iter = --rItemFrameQueue.end();
    //
    pItemSet->push_back(std::make_pair(streamId, pItem));
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Imp::AppStreamMgr::FrameHandler::registerStreamBuffers(
    std::map<StreamId_T, std::shared_ptr<IMetaStreamBuffer> > const& buffers,
    std::shared_ptr<FrameParcel> const pFrame,
    MetaItemSet* const pItemSet) {
  //  Request::v[Input|Output]MetaBuffers
  //  -> FrameParcel
  //
  for (auto& it : buffers) {
    std::shared_ptr<IMetaStreamBuffer> const pBuffer = it.second;
    //
    StreamId_T const streamId = pBuffer->getStreamInfo()->getStreamId();
    //
    std::shared_ptr<MetaItem> pItem = std::make_shared<MetaItem>();
    pItem->pFrame = pFrame;
    pItem->pItemSet = pItemSet;
    pItem->buffer = pBuffer;
    //
    pItemSet->emplace(streamId, pItem);
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Imp::AppStreamMgr::FrameHandler::checkRequestError(
    FrameParcel const& frame) {
  /**
   * @return
   *      ==0: uncertain
   *      > 0: it is indeed a request error
   *      < 0: it is indeed NOT a request error
   */
  //  It's impossible to be a request error if:
  //  1) any valid output image streams exist, or
  //  2) all valid output meta streams exist
  //
  // [NOT a request error]
  //
  if (frame.vOutputImageItem.numValidStreams > 0 ||
      (frame.vOutputMetaItem.numValidStreams == frame.vOutputMetaItem.size() &&
       frame.vOutputMetaItem.hasLastPartial)) {
    return -1;
  }
  //
  // [A request error]
  //
  if (frame.vOutputImageItem.numErrorStreams == frame.vOutputImageItem.size() &&
      frame.vOutputMetaItem.numErrorStreams > 0) {
    return 1;
  }
  //
  // [uncertain]
  return 0;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareErrorFrame(
    CallbackParcel* rCbParcel, std::shared_ptr<FrameParcel> const& pFrame) {
  rCbParcel->valid = MTRUE;
  //
  {
    CallbackParcel::Error error;
    error.errorCode = CAMERA3_MSG_ERROR_REQUEST;
    //
    rCbParcel->vError.push_back(error);
    //
  }
  //
  // Note:
  // FrameParcel::vInputImageItem
  // We're not sure whether input image streams are returned or not.
  //
  // FrameParcel::vOutputImageItem
  // for (size_t i = 0; i < pFrame->vOutputImageItem.size(); i++) {
  //    prepareReturnImage(rCbParcel, pFrame->vOutputImageItem[i]);
  //}
  for (auto& it : pFrame->vOutputImageItem) {
    prepareReturnImage(rCbParcel, it.second);
  }
  //
  pFrame->errors.set(HistoryBit::ERROR_SENT_FRAME);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareErrorMetaIfPossible(
    CallbackParcel* rCbParcel, std::shared_ptr<MetaItem> const& pItem) {
  auto pFrame = pItem->pFrame.lock();
  if (pFrame && !pFrame->errors.test(HistoryBit::ERROR_SENT_META)) {
    pFrame->errors.set(HistoryBit::ERROR_SENT_META);
    //
    CallbackParcel::Error error;
    error.errorCode = CAMERA3_MSG_ERROR_RESULT;
    //
    rCbParcel->vError.push_back(error);
    rCbParcel->valid = MTRUE;
  }
  //
  //  Actually, this item will be set to NULL, and it is not needed for
  //  the following statements.
  //
  pItem->history.set(HistoryBit::ERROR_SENT_META);
  //
  if (0 == pFrame->timestampShutter) {
    MY_LOGW("[frameNo:%u] CAMERA3_MSG_ERROR_RESULT with shutter timestamp = 0",
            pFrame->frameNo);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareErrorImage(
    CallbackParcel* rCbParcel, std::shared_ptr<ImageItem> const& pItem) {
  rCbParcel->valid = MTRUE;
  //
  {
    StreamId_T const streamId = pItem->buffer->getStreamInfo()->getStreamId();
    ImageConfigItem const& rConfigItem = mImageConfigMap.at(streamId);
    //
    CallbackParcel::Error error;
    error.errorCode = CAMERA3_MSG_ERROR_BUFFER;
    error.stream = rConfigItem.pStreamInfo;
    //
    rCbParcel->vError.push_back(error);
    MY_LOGW_IF(0, "(Error Status) streamId:%#" PRIx64 "(%s)", streamId,
               rConfigItem.pStreamInfo->getStreamName());
  }
  //
  //  Actually, this item will be set to NULL, and it is not needed for
  //  the following statements.
  //
  pItem->history.set(HistoryBit::ERROR_SENT_IMAGE);
}

/******************************************************************************
 *
 ******************************************************************************/
bool NSCam::v3::Imp::AppStreamMgr::FrameHandler::
    prepareShutterNotificationIfPossible(
        CallbackParcel* rCbParcel, std::shared_ptr<MetaItem> const& pItem) {
  auto pFrame = pItem->pFrame.lock();
  if (!pFrame) {
    MY_LOGE("Frame Expired");
    return MFALSE;
  }
  if (false == pFrame->bShutterCallbacked && 0 != pFrame->timestampShutter) {
    pFrame->bShutterCallbacked = true;
    //
    rCbParcel->shutter = std::make_shared<CallbackParcel::Shutter>();
    rCbParcel->shutter->timestamp = pFrame->timestampShutter;
    rCbParcel->valid = MTRUE;
    return MTRUE;
  }
  return MFALSE;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareReturnMeta(
    CallbackParcel* rCbParcel, std::shared_ptr<MetaItem> const& pItem) {
  rCbParcel->valid = MTRUE;
  //
  {
    pItem->history.set(HistoryBit::RETURNED);
    pItem->pItemSet->numReturnedStreams++;
    //
    std::vector<CallbackParcel::MetaItem>* pvCbItem =
        &(rCbParcel->vOutputMetaItem);
    pvCbItem->emplace_back();
    CallbackParcel::MetaItem& rCbItem = pvCbItem->at(pvCbItem->size() - 1);
    rCbItem.buffer = pItem->buffer;

    if (pItem->bufferNo == mAtMostMetaStreamCount) {
      rCbItem.bufferNo = mAtMostMetaStreamCount;
      //
      // #warning "[FIXME] hardcode: REQUEST_PIPELINE_DEPTH=4"
      IMetadata::IEntry entry(MTK_REQUEST_PIPELINE_DEPTH);
      entry.push_back(4, Type2Type<MUINT8>());
      //
      IMetadata* pMetadata = rCbItem.buffer->tryWriteLock(LOG_TAG);
      pMetadata->update(MTK_REQUEST_PIPELINE_DEPTH, entry);
      rCbItem.buffer->unlock(LOG_TAG, pMetadata);
    } else {
      rCbItem.bufferNo = pItem->pItemSet->numReturnedStreams;
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Imp::AppStreamMgr::FrameHandler::isReturnable(
    std::shared_ptr<MetaItem> const& pItem) const {
  if (pItem->bufferNo == mAtMostMetaStreamCount) {
    // the final meta result to return must keep the order submitted.

    FrameQueue::const_iterator itFrame = mFrameQueue.begin();
    while (1) {
      std::shared_ptr<FrameParcel> const& pFrame = *itFrame;
      auto pFrame2 = pItem->pFrame.lock();
      if (pFrame == pFrame2) {
        break;
      }
      //
      if (pFrame->vOutputMetaItem.empty()) {
        if (!mIsExternal) {
          MY_LOGW("[%d/%d] vOutputMetaItem:%zu", pFrame->frameNo,
                  pFrame2->frameNo, pFrame->vOutputMetaItem.size());
          dump();
        }
        return MFALSE;
      }
      MBOOL isAllMetaReturned = (pFrame->vOutputMetaItem.size() ==
                                 pFrame->vOutputMetaItem.numReturnedStreams);

      if ((pFrame->vOutputMetaItem.hasLastPartial && isAllMetaReturned) ||
          (!pFrame->vOutputMetaItem.hasLastPartial)) {
        MY_LOGD(
            "Block to return the final meta of frameNo:%u since frameNo:%u "
            "(%zu|%zu) partial:%d",
            pFrame2->frameNo, pFrame->frameNo,
            pFrame->vOutputMetaItem.numReturnedStreams,
            pFrame->vOutputMetaItem.size(),
            pFrame->vOutputMetaItem.hasLastPartial);
        return MFALSE;
      }
      //
      ++itFrame;
    }
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareReturnImage(
    CallbackParcel* rCbParcel, std::shared_ptr<ImageItem> const& pItem) {
  rCbParcel->valid = MTRUE;
  //
  if (!pItem->history.test(HistoryBit::RETURNED)) {
    pItem->history.set(HistoryBit::RETURNED);
    pItem->pItemSet->numReturnedStreams++;
    //
    StreamId_T const streamId = pItem->buffer->getStreamInfo()->getStreamId();
    ImageConfigItem& rConfigItem = mImageConfigMap.at(streamId);
    rConfigItem.vItemFrameQueue.erase(pItem->iter);
    //
    std::vector<CallbackParcel::ImageItem>* pvCbItem =
        (pItem->pItemSet->asInput) ? &(rCbParcel->vInputImageItem)
                                   : &(rCbParcel->vOutputImageItem);
    pvCbItem->emplace_back();
    CallbackParcel::ImageItem& rCbItem = pvCbItem->at(pvCbItem->size() - 1);
    rCbItem.buffer = pItem->buffer;
    rCbItem.stream = rConfigItem.pStreamInfo;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Imp::AppStreamMgr::FrameHandler::isReturnable(
    std::shared_ptr<ImageItem> const& pItem) const {
  StreamId_T const streamId = pItem->buffer->getStreamInfo()->getStreamId();
  ImageItemFrameQueue const& rItemFrameQueue =
      mImageConfigMap.at(streamId).vItemFrameQueue;
  //
  ImageItemFrameQueue::const_iterator it = rItemFrameQueue.begin();
  for (; it != pItem->iter; it++) {
    if (State::IN_FLIGHT == (*it)->state) {
      return MFALSE;
    }
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Imp::AppStreamMgr::FrameHandler::isFrameRemovable(
    std::shared_ptr<FrameParcel> const& pFrame) const {
  // Not all output image streams have been returned.
  if (pFrame->vOutputImageItem.size() !=
      pFrame->vOutputImageItem.numReturnedStreams) {
    return MFALSE;
  }
  //
  // Not all input image streams have been returned.
  if (pFrame->vInputImageItem.size() !=
      pFrame->vInputImageItem.numReturnedStreams) {
    return MFALSE;
  }
  //
  //
  if (pFrame->errors.test(HistoryBit::ERROR_SENT_FRAME)) {
    // frame error was sent.
    return MTRUE;
  } else if (pFrame->errors.test(HistoryBit::ERROR_SENT_META)) {
    // meta error was sent.
    if (0 == pFrame->timestampShutter) {
      MY_LOGW("[frameNo:%u] shutter not sent with meta error", pFrame->frameNo);
    }
  } else {
    // Not all meta streams have been returned.
    MBOOL isAllMetaReturned = (pFrame->vOutputMetaItem.size() ==
                               pFrame->vOutputMetaItem.numReturnedStreams);
    if (!pFrame->vOutputMetaItem.hasLastPartial || !isAllMetaReturned) {
      return MFALSE;
    }
    //
    // No shutter timestamp has been sent.
    if (0 == pFrame->timestampShutter) {
      MY_LOGW("[frameNo:%u] shutter not sent @ no meta error", pFrame->frameNo);
      return MFALSE;
    }
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareCallbackIfPossible(
    CallbackParcel* rCbParcel, MetaItemSet* rItemSet) {
  MBOOL anyUpdate = MFALSE;
  //
  for (auto& it : *rItemSet) {
    std::shared_ptr<MetaItem> pItem = it.second;
    if (!pItem) {
      continue;
    }
    //
    auto pFrame = pItem->pFrame.lock();
    if (!pFrame) {
      MY_LOGE("Frame Expired");
      return MFALSE;
    }
    //
    switch (pItem->state) {
      case State::VALID: {
        // Valid Buffer but Not Returned
        if (!pItem->history.test(HistoryBit::RETURNED)) {
          // separate shutter and metadata
          updateShutterTimeIfPossible(pItem);
          if (isShutterReturnable(pItem) &&
              (prepareShutterNotificationIfPossible(rCbParcel, pItem))) {
            anyUpdate = MTRUE;
            if (isReturnable(pItem)) {
              prepareReturnMeta(rCbParcel, pItem);
            }
          } else if (isReturnable(pItem)) {
            prepareReturnMeta(rCbParcel, pItem);
            anyUpdate = MTRUE;
          }
        }
      } break;
      //
      case State::ERROR: {
        // Error Buffer but Not Error Sent Yet
        if (!pItem->history.test(HistoryBit::ERROR_SENT_META)) {
          if (checkRequestError(*pFrame) < 0) {
            // Not a request error
            prepareErrorMetaIfPossible(rCbParcel, pItem);
            anyUpdate = MTRUE;
          } else {
            MY_LOGD("frameNo:%u Result Error Pending", pFrame->frameNo);
          }
        }
      } break;
      //
      default:
        break;
    }
    //
    MBOOL const needRelease =
        (pItem->buffer->haveAllUsersReleased() == OK) &&
        (pItem->history.test(HistoryBit::RETURNED) ||
         pItem->history.test(HistoryBit::ERROR_SENT_FRAME) ||
         pItem->history.test(HistoryBit::ERROR_SENT_META) ||
         pItem->history.test(HistoryBit::ERROR_SENT_IMAGE));
    if (needRelease) {
      it.second = NULL;
    }
  }
  //
  return anyUpdate;
}

void NSCam::v3::Imp::AppStreamMgr::FrameHandler::updateShutterTimeIfPossible(
    std::shared_ptr<MetaItem> const& pItem) {
  auto pFrame = pItem->pFrame.lock();
  if (pFrame && 0 == pFrame->timestampShutter) {
    MINT64 timestamp = 0;
    if (getShutterTime(pItem, &timestamp)) {
      pFrame->timestampShutter = (MUINT64)timestamp;
    }
  }
  return;
}

bool NSCam::v3::Imp::AppStreamMgr::FrameHandler::getShutterTime(
    std::shared_ptr<MetaItem> const& pItem,
    MINT64* timestamp) {  // true: get; false: not found
  bool bGet_shutter = false;
  IMetadata* pMetadata = pItem->buffer->tryReadLock(LOG_TAG);
  if (pMetadata) {
    MUINT8 needOverrideTime = 0;
    MUINT8 timeOverrided = 0;
    MBOOL timestampValid =
        (needOverrideTime == 0) || (needOverrideTime > 0 && timeOverrided > 0);

    IMetadata::IEntry const entry = pMetadata->entryFor(MTK_SENSOR_TIMESTAMP);
    pItem->buffer->unlock(LOG_TAG, pMetadata);
    //
    if (timestampValid && !entry.isEmpty() &&
        entry.tag() == MTK_SENSOR_TIMESTAMP) {
      *timestamp = (MUINT64)entry.itemAt(0, Type2Type<MINT64>());
      bGet_shutter = true;
    }
  }
  return bGet_shutter;
}

bool NSCam::v3::Imp::AppStreamMgr::FrameHandler::isShutterReturnable(
    std::shared_ptr<MetaItem> const& pItem) {
  // check this shutter can return or not
  auto itFrame = mFrameQueue.begin();
  while (itFrame != mFrameQueue.end()) {
    auto pFrame = pItem->pFrame.lock();
    if (!pFrame) {
      MY_LOGE("Frame Expired");
      return MFALSE;
    }
    if ((*itFrame)->frameNo != pFrame->frameNo &&
        (*itFrame)->bShutterCallbacked == false) {
      MY_LOGI("previous shutter (%u:%p) is not ready for frame(%u)",
              (*itFrame)->frameNo, (*itFrame).get(), pFrame->frameNo);
      return MFALSE;
    } else if ((*itFrame)->frameNo == pFrame->frameNo) {
      break;
    }
    ++itFrame;
  }
  //
  return MTRUE;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Imp::AppStreamMgr::FrameHandler::prepareCallbackIfPossible(
    CallbackParcel* rCbParcel, ImageItemSet* rItemSet) {
  MBOOL anyUpdate = MFALSE;
  //
  for (auto& it : *rItemSet) {
    std::shared_ptr<ImageItem> pItem = it.second;
    if (!pItem) {
      continue;
    }
    //
    auto pFrame = pItem->pFrame.lock();
    if (!pFrame) {
      MY_LOGE("Frame Expired");
      return MFALSE;
    }
    //
    switch (pItem->state) {
      case State::PRE_RELEASE: {
        // Pre-Release but Not Returned
        if (!pItem->history.test(HistoryBit::RETURNED)) {
          if (isReturnable(pItem)) {
            prepareReturnImage(rCbParcel, pItem);
            anyUpdate = MTRUE;
          }
        }
      } break;
      //
      case State::VALID: {
        // Valid Buffer but Not Returned
        if (!pItem->history.test(HistoryBit::RETURNED)) {
          if (isReturnable(pItem)) {
            prepareReturnImage(rCbParcel, pItem);
            anyUpdate = MTRUE;
          }
        }
      } break;
      //
      case State::ERROR: {
        // Error Buffer but Not Error Sent Yet
        if (!pItem->history.test(HistoryBit::ERROR_SENT_IMAGE)) {
          if (checkRequestError(*pFrame) < 0) {
            // Not a request error
            prepareErrorImage(rCbParcel, pItem);
            if (!pItem->history.test(HistoryBit::RETURNED)) {
              prepareReturnImage(rCbParcel, pItem);
            }
            anyUpdate = MTRUE;
          } else {
            MY_LOGV("frameNo:%u Buffer Error Pending, streamId:%#" PRIx64,
                    pFrame->frameNo,
                    pItem->buffer->getStreamInfo()->getStreamId());
          }
        }
      } break;
      //
      default:
        break;
    }
    //
    MBOOL const needRelease =
        (pItem->buffer->haveAllUsersReleased() == OK) &&
        (pItem->history.test(HistoryBit::RETURNED) ||
         pItem->history.test(HistoryBit::ERROR_SENT_FRAME) ||
         pItem->history.test(HistoryBit::ERROR_SENT_META) ||
         pItem->history.test(HistoryBit::ERROR_SENT_IMAGE));
    if (needRelease) {
      it.second = nullptr;
    }
  }
  //
  return anyUpdate;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::updateItemSet(
    MetaItemSet const& rItemSet) {
  for (auto& it : rItemSet) {
    StreamId_T const streamId = it.first;
    std::shared_ptr<MetaItem> pItem = it.second;
    if (!pItem) {
      MY_LOGV("Meta streamId:%#" PRIx64 " NULL MetaItem", streamId);
      continue;
    }
    //
    if (State::VALID != pItem->state && State::ERROR != pItem->state) {
      std::shared_ptr<IMetaStreamBuffer> pStreamBuffer = pItem->buffer;
      //
      if (pStreamBuffer->getStreamInfo()->getStreamType() !=
              eSTREAMTYPE_META_IN &&
          pStreamBuffer->haveAllProducerUsersReleased() == OK) {
        MBOOL const isError =
            pStreamBuffer->hasStatus(STREAM_BUFFER_STATUS::ERROR);
        if (isError) {
          // RELEASE & ERROR BUFFER
          pItem->state = State::ERROR;
          pItem->pItemSet->numErrorStreams++;
#if 0  // todo
                    CAM_LOGW(
                        "[Meta Stream Buffer] Error happens..."
                        " - frameNo:%u streamId:%#" PRIx64 " %s",
                        pItem->pFrame->frameNo, streamId,
                        pStreamBuffer->getName());
#endif

        } else {
          // RELEASE & VALID BUFFER
          pItem->state = State::VALID;
          pItem->pItemSet->numValidStreams++;
        }
      }
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::updateItemSet(
    ImageItemSet const& rItemSet) {
  struct ReleaseHandler {
    MVOID
    operator()(StreamId_T const streamId,
               ImageItem* const pItem,
               AppImageStreamBuffer* pStreamBuffer) {
      MBOOL const isError =
          pStreamBuffer->hasStatus(STREAM_BUFFER_STATUS::ERROR);
      if (isError) {
#if 0  // todo
        CAM_LOGW(
            "[Image Stream Buffer] Error happens and all users release"
            " - frameNo:%u streamId:%#" PRIx64 " %s state:%s->ERROR",
            pItem->pFrame->frameNo,
            streamId,
            pStreamBuffer->getName(),
            ((State::IN_FLIGHT == pItem->state) ? "IN-FLIGHT" : "PRE-RELEASE"));
#endif
        // We should:
        //  RF = ( ACQUIRE ) ? -1 : AF
        // For simplity, however, no matter if acquire_fence was
        // waited on or not, we just:
        //  RF = AF
        MINT AF = pStreamBuffer->createAcquireFence();
        pStreamBuffer->setReleaseFence(AF);
        pStreamBuffer->setAcquireFence(-1);
        //
        pItem->state = State::ERROR;
        pItem->pItemSet->numErrorStreams++;
      } else {
        pStreamBuffer->setReleaseFence(-1);
        pStreamBuffer->setAcquireFence(-1);
        //
        pItem->state = State::VALID;
        pItem->pItemSet->numValidStreams++;
      }
    }
  };
  //
  struct PreReleaseHandler {
    MVOID
    operator()(StreamId_T const streamId,
               ImageItem* const pItem,
               AppImageStreamBuffer* pStreamBuffer) {
      // Here the buffer status is uncertain, but its default should be OK.
      MINT RF = pStreamBuffer->createReleaseFence();
      auto pFrame = pItem->pFrame.lock();
      MY_LOGW_IF(-1 == RF && pFrame,
                 "[Image Stream Buffer] pre-release but release_fence=-1 !! "
                 " - frameNo:%u streamId:%#" PRIx64
                 " %s state:IN-FLIGHT->PRE-RELEASE",
                 pFrame->frameNo, streamId, pStreamBuffer->getName());
      pStreamBuffer->setReleaseFence(RF);
      pStreamBuffer->setAcquireFence(-1);
      //
      pItem->state = State::PRE_RELEASE;
    }
  };
  //
  for (auto& it : rItemSet) {
    StreamId_T const streamId = it.first;
    std::shared_ptr<ImageItem> pItem = it.second;
    if (!pItem) {
      MY_LOGV("Image streamId:%#" PRIx64 " NULL ImageItem", streamId);
      continue;
    }
    //
    switch (pItem->state) {
      case State::IN_FLIGHT: {
        MUINT32 allUsersStatus = pItem->buffer->getAllUsersStatus();
        //
        //  IN_FLIGHT && all users release ==> VALID/ERROR
        if (allUsersStatus == IUsersManager::UserStatus::RELEASE) {
          ReleaseHandler()(streamId, pItem.get(), pItem->buffer.get());
        } else if (allUsersStatus == IUsersManager::UserStatus::PRE_RELEASE) {
          //  IN-IN_FLIGHT && all users release or pre-release ==> PRE_RELEASE
          PreReleaseHandler()(streamId, pItem.get(), pItem->buffer.get());
        }
      } break;
      //
      case State::PRE_RELEASE: {
        //  PRE_RELEASE && all users release ==> VALID/ERROR
        if (OK == pItem->buffer->haveAllUsersReleased()) {
          ReleaseHandler()(streamId, pItem.get(), pItem->buffer.get());
        }
      } break;
      //
      default:
        break;
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::update(
    ResultQueueT const& rvResult) {
  if (mFrameQueue.empty()) {
    MY_LOGD("Empty FrameQueue:%p %p", &mFrameQueue, this);
    return;
  }
  //
  FrameQueue::iterator itFrame = mFrameQueue.begin();
  for (auto iter = rvResult.begin(); iter != rvResult.end(); ++iter) {
    MUINT32 const frameNo = iter->second->frameNo;
    for (; itFrame != mFrameQueue.end(); itFrame++) {
      //
      std::shared_ptr<FrameParcel>& pFrame = *itFrame;
      if (frameNo != pFrame->frameNo) {
        continue;
      }
      // put output meta into vOutputMetaItem
      std::shared_ptr<MetaItem> pMetaItem = NULL;  // last partial metadata
      MetaItemSet* pItemSet = &pFrame->vOutputMetaItem;
      auto it = iter->second->buffer.begin();
      for (; it != iter->second->buffer.end(); it++) {
        std::shared_ptr<IMetaStreamBuffer> const pBuffer = *it;
        //
        StreamId_T const streamId = pBuffer->getStreamInfo()->getStreamId();
        //
        std::shared_ptr<MetaItem> pItem = std::make_shared<MetaItem>();
        pItem->pFrame = pFrame;
        pItem->pItemSet = pItemSet;
        pItem->buffer = pBuffer;
        pItem->bufferNo = pItemSet->size() + 1;
        pMetaItem = pItem;
        //
        pItemSet->emplace(streamId, pItem);
      }

      if (!pMetaItem) {
        MY_LOGV("frameNo:%u NULL MetaItem", frameNo);
      } else if (iter->second->lastPartial) {
        pMetaItem->bufferNo = mAtMostMetaStreamCount;
        pItemSet->hasLastPartial = true;
      }
      //
      updateItemSet(pFrame->vOutputMetaItem);
      updateItemSet(pFrame->vOutputImageItem);
      updateItemSet(pFrame->vInputImageItem);
      break;
    }
    //
    if (itFrame == mFrameQueue.end()) {
      MY_LOGW("frameNo:%u is not in FrameQueue; its first frameNo:%u", frameNo,
              (*mFrameQueue.begin())->frameNo);
      itFrame = mFrameQueue.begin();
    }
  }
  //
  auto it = --rvResult.end();
  MUINT32 const latestResultFrameNo = it->second->frameNo;
  if (0 < (MINT32)(latestResultFrameNo - mFrameQueue.latestResultFrameNo)) {
    mFrameQueue.latestResultFrameNo = latestResultFrameNo;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::update(
    std::list<CallbackParcel>* rCbList) {
  FrameQueue::iterator itFrame = mFrameQueue.begin();
  while (itFrame != mFrameQueue.end()) {
    MUINT32 const frameNo = (*itFrame)->frameNo;
    if (0 < (MINT32)(frameNo - mFrameQueue.latestResultFrameNo)) {
      MY_LOGV("stop updating frame => frameNo: this(%u) > latest(%u) ", frameNo,
              mFrameQueue.latestResultFrameNo);
      break;
    }
    //
    CallbackParcel cbParcel;
    cbParcel.valid = MFALSE;
    cbParcel.frameNo = frameNo;
    cbParcel.timestampShutter = (*itFrame)->timestampShutter;
    //
    if (checkRequestError(**itFrame) > 0) {
      // It's a request error
      // Here we're still not sure that the input image stream is returned or
      // not.
      MY_LOGD("frameNo:%u Request Error", (*itFrame)->frameNo);
      prepareErrorFrame(&cbParcel, *itFrame);
    } else {
      prepareCallbackIfPossible(&cbParcel, &((*itFrame)->vOutputMetaItem));
      prepareCallbackIfPossible(&cbParcel, &((*itFrame)->vOutputImageItem));
    }
    prepareCallbackIfPossible(&cbParcel, &((*itFrame)->vInputImageItem));
    //
    if (cbParcel.valid) {
      rCbList->push_back(cbParcel);
    }
    //
    if (isFrameRemovable(*itFrame)) {
      // remove this frame from the frame queue.
      itFrame = mFrameQueue.erase(itFrame);
    } else {
      // next iteration
      itFrame++;
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::update(
    ResultQueueT const& rvResult, std::list<CallbackParcel>* rCbList) {
  update(rvResult);
  update(rCbList);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Imp::AppStreamMgr::FrameHandler::dump() const {
  FrameQueue::const_iterator itFrame = mFrameQueue.begin();
  for (; itFrame != mFrameQueue.end(); itFrame++) {
    MY_LOGD("frameNo:%u shutter:%" PRId64 " errors:%lx", (*itFrame)->frameNo,
            (*itFrame)->timestampShutter, (*itFrame)->errors.to_ulong());
    //
    //  Input Image
    {
      ImageItemSet const& rItems = (*itFrame)->vInputImageItem;
      MY_LOGD("\t Input Image");
      MY_LOGD("\t\t return#:%zu valid#:%zu error#:%zu",
              rItems.numReturnedStreams, rItems.numValidStreams,
              rItems.numErrorStreams);
      for (auto& it : rItems) {
        StreamId_T const streamId = it.first;
        ImageItem* pItem = (it.second).get();
        if (pItem) {
          MY_LOGD("\t\t streamId:%#" PRIx64
                  " "
                  "state:%#x history:%lx buffer:%p %s",
                  streamId, pItem->state, pItem->history.to_ulong(),
                  pItem->buffer.get(),
                  (pItem->buffer ? pItem->buffer->getName() : ""));
        } else {
          MY_LOGD("\t\t streamId:%#" PRIx64 " ", streamId);
        }
      }
    }
    //
    //  Output Image
    {
      ImageItemSet const& rItems = (*itFrame)->vOutputImageItem;
      MY_LOGD("\t Output Image");
      MY_LOGD("\t\t return#:%zu valid#:%zu error#:%zu",
              rItems.numReturnedStreams, rItems.numValidStreams,
              rItems.numErrorStreams);
      for (auto& it : rItems) {
        StreamId_T const streamId = it.first;
        ImageItem* pItem = (it.second).get();
        if (pItem) {
          MY_LOGD("\t\t streamId:%#" PRIx64
                  " "
                  "state:%#x history:%lx buffer:%p %s",
                  streamId, pItem->state, pItem->history.to_ulong(),
                  pItem->buffer.get(),
                  (pItem->buffer ? pItem->buffer->getName() : ""));
        } else {
          MY_LOGD("\t\t streamId:%#" PRIx64 " ", streamId);
        }
      }
    }
    //
    //  Output Meta
    {
      MetaItemSet const& rItems = (*itFrame)->vOutputMetaItem;
      MY_LOGD("\t Output Meta");
      MY_LOGD("\t\t return#:%zu valid#:%zu error#:%zu",
              rItems.numReturnedStreams, rItems.numValidStreams,
              rItems.numErrorStreams);
      for (auto& it : rItems) {
        StreamId_T const streamId = it.first;
        MetaItem* pItem = it.second.get();
        if (pItem) {
          MY_LOGD("\t\t streamId:%#" PRIx64
                  " "
                  "state:%#x history:%lx buffer:%p %s",
                  streamId, pItem->state, pItem->history.to_ulong(),
                  pItem->buffer.get(),
                  (pItem->buffer ? pItem->buffer->getName() : ""));
        } else {
          MY_LOGD("\t\t streamId:%#" PRIx64 " ", streamId);
        }
      }
    }
  }
}
