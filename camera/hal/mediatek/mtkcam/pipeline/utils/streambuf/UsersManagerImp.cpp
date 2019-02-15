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

#define LOG_TAG "MtkCam/streambuf"
//
#include "MyUtils.h"
#include <memory>
#include <string>
#include <vector>
#include <mtkcam/pipeline/utils/streambuf/UsersManager.h>
//
// using namespace NSCam::Utils::Sync;

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::UsersManager::MyUser::MyUser(User const& user,
                                               ssize_t groupIndex)
    : mUserId(user.mUserId),
      mAcquireFence(NSCam::Utils::Sync::IFence::create(user.mAcquireFence)),
      mReleaseFence(NSCam::Utils::Sync::IFence::create(user.mReleaseFence)),
      mUsage(user.mUsage),
      mCategory(user.mCategory)
      //
      ,
      mGroupIndex(groupIndex),
      mUserStatus(0) {}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::UsersManager::MyUserGraph::MyUserGraph(size_t groupIndex)
    : mConsumerUsage(0), mGroupIndex(groupIndex) {
  pthread_rwlock_init(&mRWLock, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::addUser(User const& usr) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto it = mUserVector.find(usr.mUserId);
  if (CC_UNLIKELY(it != mUserVector.end())) {
    MY_LOGE("UserId %zd already exists", usr.mUserId);
    return ALREADY_EXISTS;
  }
  //
  mUserVector.emplace(usr.mUserId,
                      UserNode(std::make_shared<MyUser>(usr, mGroupIndex)));
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::removeUser(UserId_T id) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto rmIter = mUserVector.find(id);
  if (mUserVector.end() == rmIter) {
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }
  std::vector<UserId_T>* rmSet;
  // remove in adjacent nodes of deleted node
  rmSet = &(rmIter->second.mIn);
  size_t upper = rmSet->size();
  for (size_t i = 0; i < upper; i++) {
    auto index = std::find(mUserVector.at(rmSet->at(i)).mOut.begin(),
                           mUserVector.at(rmSet->at(i)).mOut.end(), id);
    mUserVector.at(rmSet->at(i)).mOut.erase(index);
  }
  // remove out adjacent nodes of deleted node
  rmSet = &(rmIter->second.mOut);
  upper = rmSet->size();
  for (size_t i = 0; i < upper; i++) {
    auto index = std::find(mUserVector.at(rmSet->at(i)).mIn.begin(),
                           mUserVector.at(rmSet->at(i)).mIn.end(), id);
    mUserVector.at(rmSet->at(i)).mIn.erase(index);
  }
  mUserVector.erase(rmIter);

  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::addEdge(UserId_T id_src,
                                                     UserId_T id_dst) {
  pthread_rwlock_wrlock(&mRWLock);
  // Ensure both nodes exist
  auto srcIter = mUserVector.find(id_src);
  auto dstIter = mUserVector.find(id_dst);
  if (mUserVector.end() == srcIter || mUserVector.end() == dstIter) {
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }

  // Ensure that each edge only be added once
  MERROR ret = OK;
  auto srcEdgeIdx = std::find(mUserVector.at(id_src).mOut.begin(),
                              mUserVector.at(id_src).mOut.end(), id_dst);
  auto dstEdgeIdx = std::find(mUserVector.at(id_dst).mIn.begin(),
                              mUserVector.at(id_dst).mIn.end(), id_src);
  if (mUserVector.at(id_src).mOut.end() == srcEdgeIdx ||
      mUserVector.at(id_dst).mIn.end() == dstEdgeIdx) {
    mUserVector.at(id_src).mOut.push_back(id_dst);
    mUserVector.at(id_dst).mIn.push_back(id_src);
  } else {
    MY_LOGE("Illegal edge exists");
    ret = ALREADY_EXISTS;
  }

  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::removeEdge(UserId_T id_src,
                                                        UserId_T id_dst) {
  pthread_rwlock_wrlock(&mRWLock);
  auto srcIter = mUserVector.find(id_src);
  auto dstIter = mUserVector.find(id_dst);
  if (mUserVector.end() == srcIter || mUserVector.end() == dstIter) {
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }

  auto srcEdgeIdx = std::find(mUserVector.at(id_src).mOut.begin(),
                              mUserVector.at(id_src).mOut.end(), id_dst);
  auto dstEdgeIdx = std::find(mUserVector.at(id_dst).mIn.begin(),
                              mUserVector.at(id_dst).mIn.end(), id_src);

  if (mUserVector.at(id_src).mOut.end() == srcEdgeIdx ||
      mUserVector.at(id_dst).mIn.end() == dstEdgeIdx) {
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }

  mUserVector.at(id_src).mOut.erase(srcEdgeIdx);
  mUserVector.at(id_dst).mIn.erase(dstEdgeIdx);
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::setCapacity(size_t size) {
  pthread_rwlock_wrlock(&mRWLock);
  if (mUserVector.max_size() < size) {
    MY_LOGE("Not enough memory for size %zu", size);
    pthread_rwlock_unlock(&mRWLock);
    return NO_MEMORY;
  }
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::UsersManager::MyUserGraph::getGroupIndex() const {
  pthread_rwlock_rdlock(&mRWLock);
  size_t ret = mGroupIndex;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::UsersManager::MyUserGraph::size() const {
  pthread_rwlock_rdlock(&mRWLock);
  size_t ret = mUserVector.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::UsersManager::MyUser>
NSCam::v3::Utils::UsersManager::MyUserGraph::userAt(size_t index) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto it = mUserVector.begin();
  std::advance(it, index);
  auto ret = (it->second).mMyUser;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::UsersManager::MyUserGraph::indegree(
    size_t index) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto it = mUserVector.begin();
  std::advance(it, index);
  size_t ret = (it->second).mIn.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::UsersManager::MyUserGraph::outdegree(
    size_t index) const {
  pthread_rwlock_rdlock(&mRWLock);
  auto it = mUserVector.begin();
  std::advance(it, index);
  size_t ret = (it->second).mOut.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::setAcquireFence(size_t index,
                                                             MINT fence) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto it = mUserVector.begin();
  std::advance(it, index);
  std::shared_ptr<MyUser> const& pUser = (it->second).mMyUser;

  if (CC_UNLIKELY(0 <= pUser->mAcquireFence->getFd())) {
    MY_LOGE("%zu: fail to set fence:%d since acquire fence:%d already exists",
            index, fence, pUser->mAcquireFence->getFd());
    pthread_rwlock_unlock(&mRWLock);
    return ALREADY_EXISTS;
  }
  //
  pUser->mAcquireFence = NSCam::Utils::Sync::IFence::create(fence);
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT
NSCam::v3::Utils::UsersManager::MyUserGraph::getCategory(size_t index) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto it = mUserVector.begin();
  std::advance(it, index);
  std::shared_ptr<MyUser> const& pUser = (it->second).mMyUser;
  MUINT ret = pUser->mCategory;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::getInUsers(
    size_t userId, std::vector<std::shared_ptr<MyUser> >* result) const {
  result->clear();
  UserNode const& usr = mUserVector.at(userId);
  for (size_t i = 0; i < usr.mIn.size(); i++) {
    result->push_back(mUserVector.at(usr.mIn.at(i)).mMyUser);
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::DFS(
    size_t userId, std::vector<std::shared_ptr<MyUser> >* result) const {
  for (size_t i = 0; i < mUserVector.at(userId).mIn.size(); i++) {
    if (CC_UNLIKELY(0 > DFS(mUserVector.at(userId).mIn.at(i), result))) {
      return UNKNOWN_ERROR;
    }
  }
  result->push_back(mUserVector.at(userId).mMyUser);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::MyUserGraph::getPriorUsers(
    size_t userId, std::vector<std::shared_ptr<MyUser> >* result) const {
  result->clear();
  //
  // It also contains the current user.
  if (CC_UNLIKELY(0 > DFS(userId, result))) {
    return UNKNOWN_ERROR;
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::UsersManager::UsersManager(Subject_T subject,
                                             char const* name)
    : mSubject(subject), mSubjectName(name), mUserInit(MFALSE) {
  pthread_rwlock_init(&mRWLock, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::UsersManager::Subject_T
NSCam::v3::Utils::UsersManager::getSubject() const {
  return mSubject;
}

/******************************************************************************
 *
 ******************************************************************************/
char const* NSCam::v3::Utils::UsersManager::getSubjectName() const {
  return mSubjectName.data();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::UsersManager::reset() {
  pthread_rwlock_wrlock(&mRWLock);
  //
  mUserInit = MFALSE;
  mUserGroupGraph.clear();
  mProducerMap.clear();
  mConsumerMap.clear();
  pthread_rwlock_unlock(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::UsersManager::MyUser>
NSCam::v3::Utils::UsersManager::queryUser_(UserId_T const userId) const {
  auto iter = mProducerMap.begin();
  std::shared_ptr<MyUser> pUser;
  //
  if (CC_UNLIKELY((iter = mProducerMap.find(userId)) == mProducerMap.end() &&
                  (iter = mConsumerMap.find(userId)) == mConsumerMap.end())) {
    MY_LOGE("Subject:%s cannot find userId:%#" PRIxPTR
            " #Producers:%zu #Consumers:%zu",
            getSubjectName(), userId, mProducerMap.size(), mConsumerMap.size());
    NSCam::Utils::dumpCallStack(LOG_TAG);
    return nullptr;
  }
  //
  pUser = iter->second;
  if (CC_UNLIKELY((ssize_t)mUserGroupGraph.size() <= pUser->mGroupIndex)) {
    MY_LOGE("userId:%" PRIdPTR " has groupIndex:%zu > mUserGroupGraph.size:%zu",
            userId, pUser->mGroupIndex, mUserGroupGraph.size());
    return nullptr;
  }
  return pUser;
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t NSCam::v3::Utils::UsersManager::enqueUserGraph_(size_t groupIndex) {
  std::shared_ptr<MyUserGraph> myUserGraph = mUserGroupGraph.at(groupIndex);
  //
  for (size_t i = 0; i < myUserGraph->size(); i++) {
    //
    std::shared_ptr<MyUser> pMyUser = myUserGraph->userAt(i);
    switch (pMyUser->mCategory) {
      case Category::PRODUCER: {
        mProducerMap.emplace(pMyUser->mUserId, pMyUser);
      } break;
      //
      case Category::CONSUMER: {
        mConsumerMap.emplace(pMyUser->mUserId, pMyUser);
        myUserGraph->mConsumerUsage |=
            pMyUser->mUsage;  // update usage of consumer group.
      } break;
      //
      default:
        break;
    }
  }
  //
  return groupIndex;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::UsersManager::MyUserGraph>
NSCam::v3::Utils::UsersManager::queryUserGraph(
    IUserGraph* const pUserGraph) const {
  if (CC_UNLIKELY(!pUserGraph)) {
    return nullptr;
  }

  size_t const groupIndex = pUserGraph->getGroupIndex();

  pthread_rwlock_rdlock(&mRWLock);
  if (CC_UNLIKELY(mUserGroupGraph.size() <= groupIndex)) {
    MY_LOGW("groupIndex:%zu > %zu", groupIndex, mUserGroupGraph.size());
    pthread_rwlock_unlock(&mRWLock);
    return nullptr;
  }
  auto ret = mUserGroupGraph[groupIndex];
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::UsersManager::IUserGraph>
NSCam::v3::Utils::UsersManager::createGraph() {
  pthread_rwlock_wrlock(&mRWLock);
  //
  if (CC_UNLIKELY(mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] ALREADY_EXISTS", mSubject,
            getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return nullptr;
  }
  //
  size_t groupIndex = mUserGroupGraph.size();
  mUserGroupGraph.resize(groupIndex + 1);
  auto myUserGraph = std::make_shared<MyUserGraph>(groupIndex);
  mUserGroupGraph.at(groupIndex) = myUserGraph;
  pthread_rwlock_unlock(&mRWLock);
  return myUserGraph;
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t NSCam::v3::Utils::UsersManager::enqueUserGraph(
    std::shared_ptr<IUserGraph> pUserGraph) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  if (CC_UNLIKELY(mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] ALREADY_EXISTS", mSubject,
            getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return ALREADY_EXISTS;
  }
  //
  ssize_t ret = enqueUserGraph_(pUserGraph->getGroupIndex());
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::finishUserSetup() {
  pthread_rwlock_wrlock(&mRWLock);
  mUserInit = MTRUE;
  MY_LOGD_IF(0, "[subject:%#" PRIxPTR " %s]", mSubject, getSubjectName());
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::UsersManager::markUserStatus(UserId_T userId,
                                               MUINT32 const statusMask) {
  std::bitset<32> bitset(statusMask);
  MUINT32 mask = statusMask;
  //
  pthread_rwlock_wrlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //

  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (CC_UNLIKELY(pMyUser == nullptr)) {
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  for (MUINT32 bitmask = 1; 0 != mask; bitmask <<= 1) {
    // skip if not intending to be marked.
    if (bitmask != (bitmask & mask)) {
      continue;
    }

    // clear this bit.
    mask &= ~bitmask;

    // skip if marked before.
    if (bitmask == (bitmask & pMyUser->mUserStatus)) {
      continue;
    }

    // mark this bit.
    pMyUser->mUserStatus |= bitmask;

    //
    MyUserMap* pUserMap;
    if (pMyUser->mCategory == Category::PRODUCER) {
      pUserMap = &mProducerMap;
    } else if (pMyUser->mCategory == Category::CONSUMER) {
      pUserMap = &mConsumerMap;
    } else {
      MY_LOGE("Wrong Category: %d", pMyUser->mCategory);
      pthread_rwlock_unlock(&mRWLock);
      return 0;
    }

    auto iter = pUserMap->find(userId);
    switch (bitmask) {
      case UserStatus::USED:
        pUserMap->mBitSetUsed.set(std::distance(pUserMap->begin(), iter));
        break;
        //
      case UserStatus::RELEASE:
        pUserMap->mBitSetReleased.set(std::distance(pUserMap->begin(), iter));
        break;
        //
      case UserStatus::PRE_RELEASE:
        pUserMap->mBitSetPreReleased.set(
            std::distance(pUserMap->begin(), iter));
        break;
        //
      case UserStatus::RELEASE_STILLUSE:
        pUserMap->mBitSetReleasedStillUse.set(
            std::distance(pUserMap->begin(), iter));
        break;
        //
      default:
        break;
    }
  }
  //
  MUINT32 ret = pMyUser->mUserStatus;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::UsersManager::getUserStatus(UserId_T userId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (CC_UNLIKELY(pMyUser == nullptr)) {
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  MUINT32 ret = pMyUser->mUserStatus;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT
NSCam::v3::Utils::UsersManager::getUserCategory(UserId_T userId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return Category::NONE;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (pMyUser == nullptr) {
    pthread_rwlock_unlock(&mRWLock);
    return Category::NONE;
  }
  //
  MUINT ret = pMyUser->mCategory;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::setUserReleaseFence(UserId_T userId,
                                                    MINT releaseFence) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (CC_UNLIKELY(pMyUser == nullptr)) {
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }
  //
  if (CC_UNLIKELY(pMyUser->mReleaseFence != 0 &&
                  pMyUser->mReleaseFence->isValid() &&
                  pMyUser->mReleaseFence->getFd() == releaseFence)) {
    MY_LOGW("the same release fence:%s(%d) is set to userId:%" PRIdPTR,
            pMyUser->mReleaseFence->name(), pMyUser->mReleaseFence->getFd(),
            userId);
    pthread_rwlock_unlock(&mRWLock);
    return ALREADY_EXISTS;
  }
  //
  pMyUser->mReleaseFence = NSCam::Utils::Sync::IFence::create(releaseFence);
  //
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Utils::UsersManager::queryGroupUsage(UserId_T userId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (pMyUser == nullptr) {
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  MUINT64 groupUsage = 0;
  switch (pMyUser->mCategory) {
    case Category::PRODUCER:
      groupUsage = pMyUser->mUsage;
      break;
      //
    case Category::CONSUMER: {
      size_t const groupIndex = pMyUser->mGroupIndex;
      if (CC_UNLIKELY(mUserGroupGraph.size() <= groupIndex)) {
        MY_LOGE("[userId:%" PRIdPTR
                "] groupIndex:%zu > mUserGroupGraph.size:%zu",
                userId, groupIndex, mUserGroupGraph.size());
      }
      groupUsage = mUserGroupGraph[groupIndex]->mConsumerUsage;
    } break;
    //
    default:
      break;
  }

  pthread_rwlock_unlock(&mRWLock);
  return groupUsage;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::UsersManager::getNumberOfProducers() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  size_t ret = mProducerMap.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::UsersManager::getNumberOfConsumers() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return 0;
  }
  //
  size_t ret = mConsumerMap.size();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::UsersManager::createAcquireFence(UserId_T userId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (pMyUser == nullptr) {
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  size_t groupIndex = pMyUser->mGroupIndex;
  //
  IFencePtr_T fence;
  if (groupIndex == 0) {
    std::shared_ptr<MyUserGraph> myUserGraph = mUserGroupGraph[0];
    std::vector<std::shared_ptr<MyUser> > result;
    if (CC_UNLIKELY(0 > myUserGraph->getPriorUsers(userId, &result))) {
      pthread_rwlock_unlock(&mRWLock);
      return UNKNOWN_ERROR;
    }
    for (size_t j = 0; j < result.size(); j++) {
      std::shared_ptr<MyUser> myUser = result.at(j);
      if (userId == myUser->mUserId) {
        IFencePtr_T AF = myUser->mAcquireFence;
        if (AF != 0) {
          if (fence == 0) {
            MINT ret = AF->dup();
            pthread_rwlock_unlock(&mRWLock);
            return ret;
          } else {
            std::string name =
                base::StringPrintf("%s-%s", fence->name(), AF->name());
            IFencePtr_T AF_priorRFs =
                NSCam::Utils::Sync::IFence::merge(name.c_str(), fence, AF);
            MINT ret = AF_priorRFs->dup();
            pthread_rwlock_unlock(&mRWLock);
            return ret;
          }
        } else {
          if (fence != 0) {
            pthread_rwlock_unlock(&mRWLock);
            return fence->dup();
          }
          pthread_rwlock_unlock(&mRWLock);
          return -1;
        }
      }
      //
      IFencePtr_T RF = myUser->mReleaseFence;
      if (RF == 0) {
        continue;
      }
      //
      if (fence == 0) {
        fence = NSCam::Utils::Sync::IFence::create(RF->dup());
      } else {
        std::string name =
            base::StringPrintf("%s-%s", fence->name(), RF->name());
        fence = NSCam::Utils::Sync::IFence::merge(name.c_str(), fence, RF);
      }
    }
  }
  //
  MY_LOGE("Never here...something wrong!!!");
  pthread_rwlock_unlock(&mRWLock);
  return -1;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::UsersManager::createReleaseFence(UserId_T userId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (pMyUser == 0) {
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  IFencePtr_T RF = pMyUser->mReleaseFence;
  if (CC_UNLIKELY(RF == nullptr)) {
    MY_LOGW("subject:%#" PRIxPTR " userId:%#" PRIxPTR " has no release fence",
            mSubject, userId);
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  MINT ret = RF->dup();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::UsersManager::createAcquireFence() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  IFencePtr_T fence;
  for (size_t g = 0; g < mUserGroupGraph.size(); g++) {
    std::shared_ptr<MyUserGraph> myUserGraph = mUserGroupGraph[g];
    if (!myUserGraph) {
      continue;
    }
    for (size_t i = 0; i < myUserGraph->size(); i++) {
      std::shared_ptr<MyUser> myUser = myUserGraph->userAt(i);
      //
      IFencePtr_T AF = myUser->mAcquireFence;
      if (AF == nullptr || AF->getFd() == -1) {
        continue;
      } else {
        MINT ret = AF->dup();
        pthread_rwlock_unlock(&mRWLock);
        return ret;
      }
    }
  }
  //
  pthread_rwlock_unlock(&mRWLock);
  return -1;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::UsersManager::createReleaseFence() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return -1;
  }
  //
  IFencePtr_T fence;
  for (size_t g = 0; g < mUserGroupGraph.size(); g++) {
    std::shared_ptr<MyUserGraph> myUserGraph = mUserGroupGraph[g];
    if (!myUserGraph.get()) {
      continue;
    }
    for (size_t i = 0; i < myUserGraph->size(); i++) {
      std::shared_ptr<MyUser> myUser = myUserGraph->userAt(i);
      //
      IFencePtr_T RF = myUser->mReleaseFence;
      if (RF == 0) {
        continue;
      }
      //
      if (fence == 0) {
        fence = NSCam::Utils::Sync::IFence::create(RF->dup());
      } else {
        std::string name =
            base::StringPrintf("%s-%s", fence->name(), RF->name());
        fence = NSCam::Utils::Sync::IFence::merge(name.c_str(), fence, RF);
      }
    }
  }
  //
  MINT ret = (fence != 0) ? fence->dup() : -1;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::haveAllUsersReleasedOrPreReleased(
    UserId_T userId) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  std::shared_ptr<MyUser> pMyUser = queryUser_(userId);
  if (pMyUser == 0) {
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }
  size_t const groupIndex = pMyUser->mGroupIndex;
  //
  if (groupIndex == 0) {
    std::shared_ptr<MyUserGraph> myUserGraph = mUserGroupGraph[0];
    std::vector<std::shared_ptr<MyUser> > result;
    MERROR err = myUserGraph->getPriorUsers(userId, &result);
    if (CC_UNLIKELY(OK != err)) {
      MY_LOGE("[Subject:%#" PRIxPTR "] userId:%#" PRIxPTR
              " getPriorUsers return %d",
              mSubject, userId, err);
      pthread_rwlock_unlock(&mRWLock);
      return err;
    }
    for (size_t j = 0; j < result.size(); j++) {
      std::shared_ptr<MyUser> const& pThisUser = result[j];
      //
      if (userId == pThisUser->mUserId) {
        continue;
      }
      //
      if (Category::NONE == pThisUser->mCategory) {
        continue;
      }
      //
      bool const isPreReleased =
          pThisUser->mUserStatus & (UserStatus::PRE_RELEASE);
      bool const isReleased =
          (pThisUser->mUserStatus & (UserStatus::RELEASE)) ||
          (pThisUser->mUserStatus & (UserStatus::RELEASE_STILLUSE));
      if (CC_UNLIKELY(!isReleased && !isPreReleased)) {
        MY_LOGW("[Subject:%#" PRIxPTR "] UserId:%#" PRIxPTR
                " ahead of UserId:%#" PRIxPTR
                " has not released:%d or pre-released:%d",
                mSubject, pThisUser->mUserId, userId, isReleased,
                isPreReleased);
        pthread_rwlock_unlock(&mRWLock);
        return NO_INIT;
      }
    }
    pthread_rwlock_unlock(&mRWLock);
    return OK;
  }
  //
  MY_LOGE("[Subject:%#" PRIxPTR "] UserId:%#" PRIxPTR " groupIndex=%zu",
          mSubject, userId, groupIndex);
  pthread_rwlock_unlock(&mRWLock);
  return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::haveAllUsersReleased() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  MERROR ret = haveAllUsersReleasedLocked();
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::haveAllUsersReleasedLocked() const {
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    return NO_INIT;
  }
  //
  if (mConsumerMap.size() == mConsumerMap.mBitSetReleased.count() &&
      mProducerMap.size() == mProducerMap.mBitSetReleased.count()) {
    return OK;
  }
  //
  return UNKNOWN_ERROR;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::haveAllUsersReleasedOrPreReleasedLocked()
    const {
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    return NO_INIT;
  }
  //
  std::bitset<32> const consumer =
      mConsumerMap.mBitSetReleased | mConsumerMap.mBitSetPreReleased;
  if (consumer.count() != mConsumerMap.size()) {
    return UNKNOWN_ERROR;
  }
  //
  std::bitset<32> const producer =
      mProducerMap.mBitSetReleased | mProducerMap.mBitSetPreReleased;
  if (producer.count() != mProducerMap.size()) {
    return UNKNOWN_ERROR;
  }
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::UsersManager::getAllUsersStatus() const {
  MUINT32 ret = 0;
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (OK == haveAllUsersReleasedLocked()) {
    ret = UserStatus::RELEASE;
  } else if (OK == haveAllUsersReleasedOrPreReleasedLocked()) {
    ret = UserStatus::PRE_RELEASE;
  }
  //
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::haveAllProducerUsersReleased() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  MERROR ret = mProducerMap.size() == mProducerMap.mBitSetReleased.count()
                   ? OK
                   : UNKNOWN_ERROR;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::UsersManager::haveAllProducerUsersUsed() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(!mUserInit)) {
    MY_LOGW("[subject:%#" PRIxPTR " %s] NO_INIT", mSubject, getSubjectName());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  MERROR ret = mProducerMap.size() == mProducerMap.mBitSetUsed.count()
                   ? OK
                   : UNKNOWN_ERROR;
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::UsersManager::dumpStateLocked() const {
  auto printMyUserMap = [](const NSCam::v3::Utils::UsersManager::MyUserMap& map,
                           const char* prefix, const char* title) {
    if (map.size()) {
      std::string os;
      os += base::StringPrintf(" #%zu", map.size());
#if 1
      auto value = map.mBitSetUsed;
      if (value != 0) {
        os += base::StringPrintf(" Used:%#lx", value.to_ulong());
      }
      value = map.mBitSetReleased;
      if (value != 0) {
        os += base::StringPrintf(" Released:%#lx", value.to_ulong());
      }
      value = map.mBitSetPreReleased;
      if (value != 0) {
        os += base::StringPrintf(" PreReleased:%#lx", value.to_ulong());
      }
      value = map.mBitSetReleasedStillUse;
      if (value != 0) {
        os += base::StringPrintf(" ReleasedStillUse:%#lx", value.to_ulong());
      }
#endif
    }
  };

  if (CC_UNLIKELY(!mUserInit)) {
    return;
  }

  printMyUserMap(mProducerMap, "    ", "Producer");
  printMyUserMap(mConsumerMap, "    ", "Consumer");
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::UsersManager::dumpState() const {
  if (pthread_rwlock_tryrdlock(&mRWLock) == OK) {
    dumpStateLocked();
    pthread_rwlock_unlock(&mRWLock);
  } else {
  }
}
