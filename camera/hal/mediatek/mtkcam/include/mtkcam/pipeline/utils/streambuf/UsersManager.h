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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_USERSMANAGER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_USERSMANAGER_H_
//
#include <bitset>
#include <iterator>
#include <map>
#include <memory>
#include <mtkcam/pipeline/stream/IUsersManager.h>
#include <mtkcam/utils/std/Sync.h>
#include <string>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * An implementation of subject users manager.
 */
class VISIBILITY_PUBLIC UsersManager : public virtual IUsersManager {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Definitions.
  /**
   * The type of a fence in order to sync. accessing a given subject.
   */
  typedef std::shared_ptr<NSCam::Utils::Sync::IFence> IFencePtr_T;

  struct MyUser {
    UserId_T mUserId;
    IFencePtr_T mAcquireFence;
    IFencePtr_T mReleaseFence;
    MUINT64 mUsage;
    MUINT mCategory;
    ssize_t mGroupIndex;
    MUINT32 mUserStatus;
    //
    MyUser(User const& user, ssize_t groupIndex);
  };

  class MyUserGraph : public IUsersManager::IUserGraph {
    friend class UsersManager;

   public:  ////                    Definitions.
    struct UserNode {
      std::shared_ptr<MyUser> mMyUser;

      /**
       * In-coming edges of this node
       */
      std::vector<UserId_T> mIn;

      /**
       * Out-going edges of this node
       */
      std::vector<UserId_T> mOut;

      explicit UserNode(std::shared_ptr<MyUser> pUser = 0) : mMyUser(pUser) {}
    };

   protected:  ////                    Data Members.
    mutable pthread_rwlock_t mRWLock;
    MUINT64 mConsumerUsage;
    size_t mGroupIndex;
    std::map<UserId_T, UserNode> mUserVector;

   public:  ////                    Operations.
    explicit MyUserGraph(size_t groupIndex);

    ~MyUserGraph() { pthread_rwlock_destroy(&mRWLock); }

    virtual MERROR addUser(User const& usr);

    virtual MERROR removeUser(UserId_T id);

    virtual MERROR addEdge(UserId_T id_src, UserId_T id_dst);

    virtual MERROR removeEdge(UserId_T id_src, UserId_T id_dst);

    virtual MERROR setCapacity(size_t size);

    virtual size_t getGroupIndex() const;

    virtual size_t size() const;

    virtual size_t indegree(size_t index) const;

    virtual size_t outdegree(size_t index) const;

    std::shared_ptr<MyUser> userAt(size_t index) const;

    MERROR setAcquireFence(size_t index, MINT fence);

    MUINT getCategory(size_t index);

    MERROR getInUsers(size_t userId,
                      std::vector<std::shared_ptr<MyUser> >* result) const;

    MERROR DFS(size_t userId,
               std::vector<std::shared_ptr<MyUser> >* result) const;

    MERROR getPriorUsers(size_t userId,
                         std::vector<std::shared_ptr<MyUser> >* result) const;
  };

  struct MyUserGroupGraph : public std::vector<std::shared_ptr<MyUserGraph> > {
    // Override clear() if additional members are added.
  };

  struct MyUserMap : public std::map<UserId_T, std::shared_ptr<MyUser> > {
    typedef std::map<UserId_T, std::shared_ptr<MyUser> > ParentT;
    std::bitset<32> mBitSetUsed;
    std::bitset<32> mBitSetReleased;
    std::bitset<32> mBitSetPreReleased;
    std::bitset<32> mBitSetReleasedStillUse;

    MVOID clear() {
      ParentT::clear();
      mBitSetUsed.reset();
      mBitSetReleased.reset();
      mBitSetPreReleased.reset();
      mBitSetReleasedStillUse.reset();
    }
  };

 private:  ////                        Data Members.
  Subject_T const mSubject;
  std::string mSubjectName;
  mutable pthread_rwlock_t mRWLock;
  MBOOL mUserInit;
  MyUserGroupGraph mUserGroupGraph;
  MyUserMap mProducerMap;
  MyUserMap mConsumerMap;

 public:  ////                        Operations.
  explicit UsersManager(Subject_T subject, char const* name = "");
  ~UsersManager() { pthread_rwlock_destroy(&mRWLock); }

 protected:  ////                        Operations.
  std::shared_ptr<MyUser> queryUser_(UserId_T const userId) const;

  ssize_t enqueUserGraph_(size_t groupIndex);

  std::shared_ptr<MyUserGraph> queryUserGraph(
      IUserGraph* const pUserGraph) const;

  MERROR haveAllUsersReleasedLocked() const;
  MERROR haveAllUsersReleasedOrPreReleasedLocked() const;

  MVOID dumpStateLocked() const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IUsersManager Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Operations.
  virtual Subject_T getSubject() const;
  virtual char const* getSubjectName() const;
  virtual MVOID dumpState() const;

 public:  ////                        Operations.
  virtual std::shared_ptr<IUsersManager::IUserGraph> createGraph();

  virtual ssize_t enqueUserGraph(std::shared_ptr<IUserGraph> pUserGraph);

  virtual MERROR finishUserSetup();

  virtual MVOID reset();

 public:  ////                        Operations.
  virtual MUINT32 markUserStatus(UserId_T userId, MUINT32 const statusMask);

  virtual MUINT32 getUserStatus(UserId_T userId) const;

  virtual MUINT getUserCategory(UserId_T userId) const;

  virtual MERROR setUserReleaseFence(UserId_T userId, MINT releaseFence);

  virtual MUINT64 queryGroupUsage(UserId_T userId) const;

  virtual size_t getNumberOfProducers() const;
  virtual size_t getNumberOfConsumers() const;

 public:  ////                        Operations.
  virtual MINT createAcquireFence(UserId_T userId) const;

  virtual MINT createReleaseFence(UserId_T userId) const;

  virtual MINT createAcquireFence() const;

  virtual MINT createReleaseFence() const;

 public:  ////                        Operations.
  /**
   * return OK if all users ahead of this user are released or pre-released.
   */
  virtual MERROR haveAllUsersReleasedOrPreReleased(UserId_T userId) const;
  // All User
  virtual MERROR haveAllUsersReleased() const;

  /**
   * return UserStatus::RELEASE
           if all users are released

     return UserStatus::PRE_RELEASE
           if all users are released or pre-released.

     return 0
           if NOT all users are released or pre-released.
   */
  virtual MUINT32 getAllUsersStatus() const;

  // All Producer
  virtual MERROR haveAllProducerUsersReleased() const;

  virtual MERROR haveAllProducerUsersUsed() const;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_USERSMANAGER_H_
