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

#define LOG_TAG "MtkCam/SyncHelperBase"
//
#include <memory>
#include <mtkcam/utils/std/Log.h>

#include "mtkcam/pipeline/utils/SyncHelper/SyncHelperBase.h"
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
// using namespace NSCam::v3::Utils::Imp;

/******************************************************************************
 *
 ******************************************************************************/

std::shared_ptr<NSCam::v3::Utils::Imp::ISyncHelperBase>
NSCam::v3::Utils::Imp::ISyncHelperBase::createInstance() {
  return std::make_shared<SyncHelperBase>();
}

/******************************************************************************
 *
 ******************************************************************************/

NSCam::v3::Utils::Imp::SyncHelperBase::SyncHelperBase() : mUserCounter(0) {
  mSyncTimeStart = std::chrono::system_clock::now();
  mResultTimeStart = std::chrono::system_clock::now();
}

NSCam::v3::Utils::Imp::SyncHelperBase::~SyncHelperBase() {}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::MERROR NSCam::v3::Utils::Imp::SyncHelperBase::start(int CamId) {
  NSCam::MERROR err = NO_ERROR;
  mUserCounter++;
  mContextMap.emplace(CamId, std::shared_ptr<SyncContext>(new SyncContext()));
  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
NSCam::MERROR NSCam::v3::Utils::Imp::SyncHelperBase::stop(int CamId __unused) {
  NSCam::MERROR err = NO_ERROR;
  mUserCounter--;
  if (mUserCounter == 0) {
    mContextMap.clear();
  }
  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
NSCam::MERROR NSCam::v3::Utils::Imp::SyncHelperBase::init(int CamId __unused) {
  NSCam::MERROR err = NO_ERROR;

  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
NSCam::MERROR NSCam::v3::Utils::Imp::SyncHelperBase::uninit(
    int CamId __unused) {
  NSCam::MERROR err = NO_ERROR;

  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
NSCam::MERROR NSCam::v3::Utils::Imp::SyncHelperBase::syncEnqHW(
    SyncParam const& sParam) {
  NSCam::MERROR err = NO_ERROR;
  std::vector<int> searchCamIdByTargetInSyncQueue;

  mSyncQLock.lock();
  for (auto& syncTargetCamId : sParam.mSyncCams) {
    auto findTargetCamInSyncQueue =
        std::find(mSyncQueue.begin(), mSyncQueue.end(), syncTargetCamId);
    if (findTargetCamInSyncQueue != mSyncQueue.end()) {
      searchCamIdByTargetInSyncQueue.push_back(*findTargetCamInSyncQueue);
    }
  }
  if (searchCamIdByTargetInSyncQueue.size() != sParam.mSyncCams.size()) {
    // this cam needs to other cam ready.
    mSyncQueue.push_back(sParam.mCamId);
    MY_LOGD("CamID = %d wait+ q:t(%zu:%zu)", sParam.mCamId,
            sParam.mSyncCams.size(), searchCamIdByTargetInSyncQueue.size());
    mSyncQLock.unlock();
    auto context = mContextMap.at(sParam.mCamId);
    sem_wait(&(context->mSyncSem));
    MY_LOGD("CamID = %d wait-", sParam.mCamId);
  } else {
    mSyncQLock.unlock();
    // all camera is ready.
    for (auto& camId : searchCamIdByTargetInSyncQueue) {
      auto targetContext = mContextMap.at(camId);
      sem_post(&(targetContext->mSyncSem));
      MY_LOGD("all cam ready CamID = %d, postCamID = %d!", sParam.mCamId,
              camId);
      mSyncQLock.lock();
      // clear camId that in sync queue
      mSyncQueue.erase(std::remove(mSyncQueue.begin(), mSyncQueue.end(), camId),
                       mSyncQueue.end());
      auto currentTime = std::chrono::system_clock::now();
      std::chrono::duration<double> diff = currentTime - mSyncTimeStart;
      MY_LOGD("sync time duration: %lf", diff.count());
      mSyncTimeStart = currentTime;
      mSyncQLock.unlock();
    }
  }

  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
NSCam::MERROR NSCam::v3::Utils::Imp::SyncHelperBase::syncResultCheck(
    SyncParam* sParam) {
  NSCam::MERROR err = NO_ERROR;
  bool syncResult = true;

  std::shared_ptr<SyncContext> context1;
  std::shared_ptr<SyncContext> context2;

  auto getResult = [](int64_t time1, int64_t time2, int64_t tolerance) {
    int timeStamp_ms_main1 = time1 / 1000000;
    int timeStamp_ms_main2 = time2 / 1000000;
    int tolerance_ms = tolerance / 1000;
    int64_t diff = abs(timeStamp_ms_main1 - timeStamp_ms_main2);
    if (diff <= tolerance_ms) {
      return true;
    } else {
      return false;
    }
  };

  std::vector<int> searchCamIdByTargetInResultQueue;
  // assign current data
  context1 = mContextMap.at(sParam->mCamId);
  context1->mResultTimeStamp = sParam->mReslutTimeStamp;
  mResultQLock.lock();
  for (auto& syncTargetCamId : sParam->mSyncCams) {
    auto findTargetCamInResultQueue =
        std::find(mResultQueue.begin(), mResultQueue.end(), syncTargetCamId);
    if (findTargetCamInResultQueue != mResultQueue.end()) {
      searchCamIdByTargetInResultQueue.push_back(*findTargetCamInResultQueue);
    }
  }
  if (searchCamIdByTargetInResultQueue.size() != sParam->mSyncCams.size()) {
    MY_LOGD("CamID = %d wait+ q:t(%zu:%zu)", sParam->mCamId,
            sParam->mSyncCams.size(), searchCamIdByTargetInResultQueue.size());
    mResultQueue.push_back(sParam->mCamId);
    mResultQLock.unlock();
    sem_wait(&(context1->mResultSem));
    MY_LOGD("CamID = %d wait-", sParam->mCamId);

    for (auto& syncTargetCamId : sParam->mSyncCams) {
      context2 = mContextMap.at(syncTargetCamId);
      syncResult &=
          getResult(context1->mResultTimeStamp, context2->mResultTimeStamp,
                    sParam->mSyncTolerance);

      MY_LOGD("CamID = %d, time1=%" PRId64
              "ns, ret = %d, synID = %d, time2=%" PRId64
              "ns, tolerance=%" PRId64 "us",
              sParam->mCamId, context1->mResultTimeStamp, syncResult,
              syncTargetCamId, context2->mResultTimeStamp,
              sParam->mSyncTolerance);
    }
  } else {
    mResultQLock.unlock();
    // all camera is ready.
    for (auto& camId : searchCamIdByTargetInResultQueue) {
      context2 = mContextMap.at(camId);
      syncResult &=
          getResult(context1->mResultTimeStamp, context2->mResultTimeStamp,
                    sParam->mSyncTolerance);
      sem_post(&(context2->mResultSem));

      MY_LOGD("CamID = %d, time1=%" PRId64
              "ns, ret = %d, synID = %d, time2=%" PRId64 "ns tolerance=%" PRId64
              "us",
              sParam->mCamId, context1->mResultTimeStamp, syncResult, camId,
              context2->mResultTimeStamp, sParam->mSyncTolerance);
      mResultQLock.lock();
      // clear camId that in result queue
      mResultQueue.erase(
          std::remove(mResultQueue.begin(), mResultQueue.end(), camId),
          mResultQueue.end());
      auto currentTime = std::chrono::system_clock::now();
      std::chrono::duration<double> diff = currentTime - mResultTimeStart;
      MY_LOGD("result time duration: %lf", diff.count());
      mResultTimeStart = currentTime;
      mResultQLock.unlock();
    }
  }

  sParam->mSyncResult = syncResult;

  return err;
}
