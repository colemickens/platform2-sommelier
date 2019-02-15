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

#define LOG_TAG "MtkCam/HwUtils/ResourceConcurrency"
//
#include <chrono>
#include <condition_variable>
#include <inttypes.h>
#include <memory>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/hw/IResourceConcurrency.h>
#include <mutex>
#include <property_service/property.h>
#include <property_service/property_lib.h>
#include <string>
/******************************************************************************
 *
 ******************************************************************************/
using NSCam::IResourceConcurrency;

namespace NSCam {

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interface of Resource Concurrency
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class ResourceConcurrency : public virtual IResourceConcurrency {
#ifdef RES_CON_MS_TO_NS
#undef RES_CON_MS_TO_NS
#endif
#define RES_CON_MS_TO_NS(ms) ((MINT64)((ms) * (1000000LL)))

 private:
  enum STATE {
    STATE_NONE = 0,
    STATE_HELD_IDLE,
    STATE_HELD_RES_ACQUIRING,
    STATE_HELD_RES_OCCUPIED,
    STATE_HELD_RES_RELEASING
  };

  class Control {
   public:
    Control(const char* name, MINT64 timeout_ns)
        : mStrName(name),
          mTimeoutNs(timeout_ns),
          mIsWaiting(MFALSE),
          mIsExiting(MFALSE),
          mUsingId(CLIENT_HANDLER_NULL) {}
    virtual ~Control() {}
    //
    MERROR acquire(CLIENT_HANDLER id);
    MERROR release(CLIENT_HANDLER id);
    //
    MVOID settle();

   public:
    std::string mStrName;
    MINT64 mTimeoutNs;

   private:
    CLIENT_HANDLER getUser() const { return mUsingId; }
    MVOID setUser(CLIENT_HANDLER id) { mUsingId = id; }

   private:
    mutable std::mutex mLock;
    std::condition_variable mCond;
    MBOOL mIsWaiting;
    MBOOL mIsExiting;
    CLIENT_HANDLER mUsingId;
  };

  class Client {
   public:
    Client(CLIENT_HANDLER id, Control* ctrl)
        : mId(id), mState(STATE_NONE), mpCtrl(ctrl) {
      pthread_rwlock_init(&mStateLock, NULL);
    }
    virtual ~Client() { pthread_rwlock_destroy(&mStateLock); }
    //
    CLIENT_HANDLER id() const { return mId; }
    //
    CLIENT_HANDLER obtain();  // obtain this client
    MERROR cancel();          // cancel this client
    //
    MERROR acquire();  // acquire the resource
    MERROR release();  // release the resource
    //
    MBOOL isApplied() const;

   private:
    STATE getState() const {
      pthread_rwlock_rdlock(&mStateLock);
      STATE stat = mState;
      pthread_rwlock_unlock(&mStateLock);
      return stat;
    }
    MVOID setState(STATE state) {
      pthread_rwlock_wrlock(&mStateLock);
      mState = state;
      pthread_rwlock_unlock(&mStateLock);
    }

   private:
    CLIENT_HANDLER mId;
    mutable std::mutex mOpLock;
    mutable pthread_rwlock_t mStateLock;
    STATE mState;
    Control* mpCtrl;
  };

 public:
  ResourceConcurrency(const char* name, MINT64 timeout_ms)
      : mControl(name, RES_CON_MS_TO_NS(timeout_ms)),
        mClient0(CLIENT_HANDLER_0, &mControl),
        mClient1(CLIENT_HANDLER_1, &mControl) {
    MY_LOGI("name[%s] timeout(%" PRId64 "ns)", mControl.mStrName.c_str(),
            mControl.mTimeoutNs);
  }
  virtual ~ResourceConcurrency() {
    mControl.settle();
    if (mClient0.isApplied()) {
      MY_LOGE("name[%s] Client0 is in use", mControl.mStrName.c_str());
    }
    if (mClient1.isApplied()) {
      MY_LOGE("name[%s] Client1 is in use", mControl.mStrName.c_str());
    }
    MY_LOGI("name[%s] timeout(%" PRId64 "ns)", mControl.mStrName.c_str(),
            mControl.mTimeoutNs);
  }
  //
  virtual CLIENT_HANDLER requestClient();
  virtual MERROR returnClient(CLIENT_HANDLER id);
  //
  virtual MERROR acquireResource(CLIENT_HANDLER id);
  virtual MERROR releaseResource(CLIENT_HANDLER id);

 private:
  ResourceConcurrency::Client* getClient(CLIENT_HANDLER id);

 private:
  mutable std::mutex mLock;
  Control mControl;
  Client mClient0;
  Client mClient1;
};

}  // namespace NSCam

using NSCam::ResourceConcurrency;

/******************************************************************************
 *
 ******************************************************************************/
ResourceConcurrency::Client* ResourceConcurrency::getClient(CLIENT_HANDLER id) {
  if (id == CLIENT_HANDLER_0) {
    return &mClient0;
  }
  if (id == CLIENT_HANDLER_1) {
    return &mClient1;
  }
  return (ResourceConcurrency::Client*)(NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
IResourceConcurrency::CLIENT_HANDLER ResourceConcurrency::requestClient() {
  std::lock_guard<std::mutex> _l(mLock);
  IResourceConcurrency::CLIENT_HANDLER client = CLIENT_HANDLER_NULL;
  client = mClient0.obtain();
  if (client != CLIENT_HANDLER_NULL) {
    return client;
  }
  client = mClient1.obtain();
  if (client != CLIENT_HANDLER_NULL) {
    return client;
  }
  return CLIENT_HANDLER_NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::returnClient(CLIENT_HANDLER id) {
  std::lock_guard<std::mutex> _l(mLock);
  Client* pClient = getClient(id);
  if (pClient == NULL) {
    return BAD_VALUE;
  }
  return pClient->cancel();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::acquireResource(CLIENT_HANDLER id) {
  Client* pClient = getClient(id);
  if (pClient == NULL) {
    return BAD_VALUE;
  }
  return pClient->acquire();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::releaseResource(CLIENT_HANDLER id) {
  Client* pClient = getClient(id);
  if (pClient == NULL) {
    return BAD_VALUE;
  }
  return pClient->release();
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
ResourceConcurrency::Client::isApplied() const {
  return (getState() != STATE_NONE);
}

/******************************************************************************
 *
 ******************************************************************************/
IResourceConcurrency::CLIENT_HANDLER ResourceConcurrency::Client::obtain() {
  if (mpCtrl == NULL) {
    return CLIENT_HANDLER_NULL;
  }
  //
  if (getState() == STATE_NONE) {
    setState(STATE_HELD_IDLE);
    MY_LOGI("[%s][Client-%d] client-requested , state(%d)",
            mpCtrl->mStrName.c_str(), id(), getState());
    return id();
  }
  return CLIENT_HANDLER_NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::Client::cancel() {
  if (mpCtrl == NULL) {
    return BAD_VALUE;
  }
  //
  if (getState() == STATE_HELD_IDLE) {
    setState(STATE_NONE);
    MY_LOGI("[%s][Client-%d] client-returned , state(%d)",
            mpCtrl->mStrName.c_str(), id(), getState());
    return NO_ERROR;
  }
  MY_LOGW("[%s][Client-%d] incorrect , state(%d)", mpCtrl->mStrName.c_str(),
          id(), getState());
  return INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::Client::acquire() {
  if (mpCtrl == NULL) {
    return BAD_VALUE;
  }
  //
  std::lock_guard<std::mutex> _l(mOpLock);
  if (getState() != STATE_HELD_IDLE) {
    MY_LOGW("[%s][Client-%d] incorrect , state(%d)", mpCtrl->mStrName.c_str(),
            id(), getState());
    return INVALID_OPERATION;
  }
  //
  MERROR res = NO_ERROR;
  setState(STATE_HELD_RES_ACQUIRING);
  //
  res = mpCtrl->acquire(id());
  //
  if (res == NO_ERROR) {
    setState(STATE_HELD_RES_OCCUPIED);
  } else {
    setState(STATE_HELD_IDLE);
  }
  return res;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::Client::release() {
  if (mpCtrl == NULL) {
    return BAD_VALUE;
  }
  //
  std::lock_guard<std::mutex> _l(mOpLock);
  if (getState() != STATE_HELD_RES_OCCUPIED) {
    MY_LOGW("[%s][Client-%d] incorrect , state(%d)", mpCtrl->mStrName.c_str(),
            id(), getState());
    return INVALID_OPERATION;
  }
  //
  MERROR res = NO_ERROR;
  setState(STATE_HELD_RES_RELEASING);
  //
  res = mpCtrl->release(id());
  //
  setState(STATE_HELD_IDLE);
  return res;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::Control::acquire(CLIENT_HANDLER id) {
  MERROR res = NO_ERROR;
  if (id >= CLIENT_HANDLER_NULL) {
    return INVALID_OPERATION;
  }
  std::unique_lock<std::mutex> _l(mLock);
  if (mIsExiting) {
    MY_LOGE("[%s][Client-%d] acquire but exiting (%" PRId64 "ns)",
            mStrName.c_str(), id, mTimeoutNs);
    return INVALID_OPERATION;
  }
  if (getUser() == CLIENT_HANDLER_NULL) {
    MY_LOGI("[%s][Client-%d] acquire resource directly", mStrName.c_str(), id);
    setUser(id);
    res = NO_ERROR;
  } else if (getUser() != id) {
    MY_LOGI("[%s][Client-%d] wait resource (%" PRId64 "ns)", mStrName.c_str(),
            id, mTimeoutNs);
    mIsWaiting = MTRUE;
    if (mCond.wait_for(_l, std::chrono::nanoseconds(mTimeoutNs)) !=
        std::cv_status::timeout) {
      setUser(id);
      MY_LOGI("[%s][Client-%d] got resource (%" PRId64 "ns)", mStrName.c_str(),
              id, mTimeoutNs);
    } else {
      // timeout
      MY_LOGW("[%s][Client-%d] wait timeout (%" PRId64 "ns) res(%d)",
              mStrName.c_str(), id, mTimeoutNs, res);
    }
    mIsWaiting = MFALSE;
  } else {
    MY_LOGW("[%s][Client-%d] has this resource already", mStrName.c_str(), id);
    res = INVALID_OPERATION;
  }
  return res;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
ResourceConcurrency::Control::release(CLIENT_HANDLER id) {
  MERROR res = NO_ERROR;
  if (id >= CLIENT_HANDLER_NULL) {
    return INVALID_OPERATION;
  }
  std::lock_guard<std::mutex> _l(mLock);
  if (mIsExiting) {
    MY_LOGE("[%s][Client-%d] release but exiting (%" PRId64 "ns)",
            mStrName.c_str(), id, mTimeoutNs);
    return INVALID_OPERATION;
  }
  if (getUser() == id) {
    MY_LOGI("[%s][Client-%d] free resource (%" PRId64 "ns)", mStrName.c_str(),
            id, mTimeoutNs);
    setUser(CLIENT_HANDLER_NULL);
    mCond.notify_all();
  } else {
    MY_LOGW("[%s][Client-%d] NOT has this resource", mStrName.c_str(), id);
    res = INVALID_OPERATION;
  }
  return res;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
ResourceConcurrency::Control::settle() {
  {
    std::lock_guard<std::mutex> _l(mLock);
    mIsExiting = MTRUE;
    if (mIsWaiting) {
      MY_LOGE("[%s][Client-%d] waiting as exiting", mStrName.c_str(),
              getUser());
      mCond.notify_all();
    }
  }
  MY_LOGD("[%s][Client-%d] settle done", mStrName.c_str(), getUser());
  return;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IResourceConcurrency> IResourceConcurrency::createInstance(
    const char* name, MINT64 timeout_ms) {
  return (std::make_shared<ResourceConcurrency>(name, timeout_ms));
}
