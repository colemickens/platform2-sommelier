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

#define LOG_TAG "MtkCam/SyncHelper"
//
#include <memory>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/std/Log.h>
#include "mtkcam/pipeline/utils/SyncHelper/SyncHelper.h"

#include "SyncHelper.h"

/******************************************************************************
 *
 ******************************************************************************/
// using namespace NSCam::v3::Utils::Imp;
/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::Imp::ISyncHelper>
NSCam::v3::Utils::Imp::ISyncHelper::createInstance() {
  return std::make_shared<SyncHelper>();
}
/******************************************************************************
 *
 ******************************************************************************/
status_t NSCam::v3::Utils::Imp::SyncHelper::syncEnqHW(int CamId,
                                                      IMetadata* HalControl) {
  status_t err = NO_ERROR;
  SyncParam syncParam;
  //
  if (HalControl == nullptr) {
    return false;
  }
  //
  IMetadata::IEntry entry = HalControl->entryFor(MTK_FRAMESYNC_ID);
  // if tag not exist, it means no need to do sync check
  if (entry.isEmpty()) {
    return true;
  }
  // copy sync target to mSyncCams
  for (MUINT i = 0; i < entry.count(); i++) {
    syncParam.mSyncCams.push_back(entry.itemAt(i, Type2Type<MINT32>()));
  }
  syncParam.mCamId = CamId;
  // enque to sync routine
  SyncHelperBase::syncEnqHW(syncParam);
  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
bool NSCam::v3::Utils::Imp::SyncHelper::syncResultCheck(int CamId,
                                                        IMetadata* HalControl,
                                                        IMetadata* HalDynamic) {
  bool syncResult = true;
  SyncParam syncParam;

  if (HalControl == nullptr || HalDynamic == nullptr) {
    MY_LOGE("cannot get metadata. c(%p) d(%p)", HalControl, HalDynamic);
    return false;
  }

  IMetadata::IEntry entry = HalControl->entryFor(MTK_FRAMESYNC_ID);
  // if tag not exist, it means no need to do sync check
  if (entry.isEmpty()) {
    return true;
  }
  // copy sync target to mSyncCams
  for (MUINT i = 0; i < entry.count(); i++) {
    syncParam.mSyncCams.push_back(entry.itemAt(i, Type2Type<MINT32>()));
  }
  // assign current camera id
  syncParam.mCamId = CamId;
  // get sync params from metadata, and do sync check.
  if (!IMetadata::getEntry<MINT64>(HalControl, MTK_FRAMESYNC_TOLERANCE,
                                   &syncParam.mSyncTolerance)) {
    MY_LOGW("cannot get sync MTK_FRAMESYNC_TOLERANCE");
    syncResult = false;
    goto lbExit;
  }
  if (!IMetadata::getEntry<MINT32>(HalControl, MTK_FRAMESYNC_FAILHANDLE,
                                   &syncParam.mSyncFailHandle)) {
    MY_LOGW("cannot get MTK_FRAMESYNC_FAILHANDLE");
    syncResult = false;
    goto lbExit;
  }
  if (!IMetadata::getEntry<MINT64>(HalDynamic, MTK_P1NODE_FRAME_START_TIMESTAMP,
                                   &syncParam.mReslutTimeStamp)) {
    MY_LOGW("cannot get MTK_P1NODE_FRAME_START_TIMESTAMP");
    syncResult = false;
    goto lbExit;
  }
  // do sync check
  SyncHelperBase::syncResultCheck(&syncParam);  // update metadata
  if (syncParam.mSyncResult) {
    if (IMetadata::setEntry<MINT64>(HalDynamic, MTK_FRAMESYNC_RESULT,
                                    MTK_FRAMESYNC_RESULT_PASS) != OK) {
      MY_LOGE("update dynamic metadata fail");
      syncResult = false;
      goto lbExit;
    }
    syncResult = true;
  } else {
    if (syncParam.mSyncFailHandle == MTK_FRAMESYNC_FAILHANDLE_CONTINUE) {
      if (IMetadata::setEntry<MINT64>(HalDynamic, MTK_FRAMESYNC_RESULT,
                                      MTK_FRAMESYNC_RESULT_FAIL_CONTINUE) !=
          OK) {
        MY_LOGE("update dynamic metadata fail");
        syncResult = false;
        goto lbExit;
      }
      syncResult = true;
    } else if (syncParam.mSyncFailHandle == MTK_FRAMESYNC_FAILHANDLE_DROP) {
      if (IMetadata::setEntry<MINT64>(HalDynamic, MTK_FRAMESYNC_RESULT,
                                      MTK_FRAMESYNC_RESULT_FAIL_DROP) != OK) {
        MY_LOGE("update dynamic metadata fail");
        syncResult = false;
        goto lbExit;
      }
      syncResult = false;
    } else {
      MY_LOGE("should not happened");
      syncResult = false;
      goto lbExit;
    }
  }
lbExit:
  return syncResult;
}
