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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMBUFFERSET_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMBUFFERSET_H_
//
#include "IStreamBuffer.h"
#include <memory>

#define ENABLE_PRERELEASE 0

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class IStreamBufferSet {
 public:  ////                            Definitions.
  typedef IUsersManager::UserId_T UserId_T;

 public:  ////                            Operations.
#if ENABLE_PRERELEASE
  /**
   * Create a subject's acquire fence associated with a user.
   * This user must wait on this fence before attempting to use the subject.
   *
   * @param[in] streamId: A stream Id.
   *
   * @param[in] userId: A specified user Id.
   *
   * @return
   *      A bad fence indicates this subject has not been initialized or need
   *      not to wait before using it.
   */
  virtual MINT createAcquireFence(StreamId_T const streamId,
                                  UserId_T userId) const = 0;

  /**
   * Set a specified user's release fence.
   * The specified user must be enqueued before this call.
   *
   * @param[in] streamId: A stream Id.
   *
   * @param[in] userId: A specified unique user Id.
   *
   * @param[in] releaseFence: A release fence to register.
   *      The callee takes the ownership of the fence file descriptor and is
   *      charge of closing it.
   *      If a release fence associated with this user is specified during
   *      enqueUserList(), the old release fence will be replaced with the
   *      specified release fence after this call.
   *
   * @return
   *      0 indicates success; otherwise failure.
   *      NAME_NOT_FOUND indicates a bad user Id.
   */
  virtual MERROR setUserReleaseFence(StreamId_T const streamId,
                                     UserId_T userId,
                                     MINT releaseFence) = 0;

  /**
   * Query a specific user's group usage.
   *
   * @param[in] streamId: A stream Id.
   *
   * @param[in] userId: A specified unique user Id.
   *
   * @return
   *      A group usage associated with this user.
   */
  virtual MUINT queryGroupUsage(StreamId_T const streamId,
                                UserId_T userId) const = 0;
#endif

 public:  ////                            Operations.
  /**
   * This call marks a specified user's status.
   *
   * @param[in] streamId: A stream Id.
   *
   * @param[in] userId: A specified unique user Id.
   *
   * @param[in] eStatus: user status.
   *      ACQUIRE     : This user has waited on the subject's acquire fence.
   *      PRE_RELEASE : This user is ready to pre-release the subject and will
   *                    still use it after its pre-release until a release
   *                    fence is signalled.
   *      USED        : This user has used the subject.
   *      RELEASE     : This user is ready to release the subject and will not
   *                    use it after its release.
   *
   * @return
   *      the current status mask.
   */
  virtual MUINT32 markUserStatus(StreamId_T const streamId,
                                 UserId_T userId,
                                 MUINT32 eStatus) = 0;

 public:  ////                            Operations.
#if ENABLE_PRERELEASE
  /**
   * Apply to pre-release.
   * After this call, all of PRE_RELEASE-marked buffers are pre-released by
   * this user.
   *
   * @param[in] userId: A specified unique user Id.
   *
   */
  virtual MVOID applyPreRelease(UserId_T userId) = 0;
#endif
  /**
   * Apply to release.
   * After this call, all of RELEASE-marked buffers are released by this user.
   *
   * @param[in] userId: A specified unique user Id.
   *
   */
  virtual MVOID applyRelease(UserId_T userId) = 0;

 public:  ////                            Operations.
  /**
   * For a specific stream buffer (associated with a stream Id), a user (with
   * a unique user Id) could successfully acquire the buffer from this buffer
   * set only if all users ahead of this user have pre-released or released
   * the buffer.
   *
   * @param[in] streamId: A specified unique stream Id.
   *
   * @param[in] userId: A specified unique user Id.
   *
   * @return
   *      A pointer to the buffer associated with the given stream Id.
   */
  virtual std::shared_ptr<IMetaStreamBuffer> getMetaBuffer(
      StreamId_T streamId, UserId_T userId) const = 0;

  virtual std::shared_ptr<IImageStreamBuffer> getImageBuffer(
      StreamId_T streamId, UserId_T userId) const = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_ISTREAMBUFFERSET_H_
