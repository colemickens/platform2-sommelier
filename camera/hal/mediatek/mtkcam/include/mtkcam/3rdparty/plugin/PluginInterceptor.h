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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PLUGININTERCEPTOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PLUGININTERCEPTOR_H_

#include <mtkcam/3rdparty/plugin/Reflection.h>
#include <mtkcam/def/common.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

/******************************************************************************
 *
 ******************************************************************************/
template <class T>
class PipelinePlugin;

/******************************************************************************
 *
 ******************************************************************************/
template <class T, class P>
class Interceptor : public PipelinePlugin<T>::IProvider {
 public:
  typedef typename PipelinePlugin<T>::Property Property;
  typedef typename PipelinePlugin<T>::Selection Selection;
  typedef typename PipelinePlugin<T>::Request::Ptr RequestPtr;
  typedef typename PipelinePlugin<T>::RequestCallback::Ptr RequestCallbackPtr;
  class Callback : public PipelinePlugin<T>::RequestCallback {
   public:
    Callback(Interceptor* pInterceptor, RequestCallbackPtr pCallback)
        : mpInterceptor(pInterceptor),
          mName(pInterceptor->mName),
          mMutex(pInterceptor->mMutex),
          mRequests(pInterceptor->mRequests),
          mpCallback(pCallback) {}

    virtual void onAborted(RequestPtr pRequest) {
      std::cout << "request: " << pRequest.get() << " aborted" << std::endl;
      mMutex.lock();
      auto it = mRequests.begin();
      for (; it != mRequests.end(); it++) {
        if (*it == pRequest) {
          mRequests.erase(it);
          break;
        }
      }
      mMutex.unlock();

      mpCallback->onAborted(pRequest);
    }

    virtual void onCompleted(RequestPtr pRequest, MERROR result) {
      std::cout << "request: " << pRequest.get() << " result: " << result
                << std::endl;
      mMutex.lock();
      auto it = mRequests.begin();
      for (; it != mRequests.end(); it++) {
        if (*it == pRequest) {
          mRequests.erase(it);
          break;
        }
      }
      mMutex.unlock();

      mpCallback->onCompleted(pRequest, result);
    }
    virtual ~Callback() {}

   private:
    Interceptor* mpInterceptor;
    const char* mName;
    std::mutex& mMutex;
    std::vector<RequestPtr>& mRequests;
    RequestCallbackPtr mpCallback;
  };

  explicit Interceptor(const char* name) : mName(name), mInitCounter(0) {}

  virtual void set(MINT32 iOpenId) { mImpl.set(iOpenId); }

  virtual const Property& property() { return mImpl.property(); }

  virtual MERROR negotiate(Selection* sel) { return mImpl.negotiate(sel); }

  virtual void init() {
    if (++mInitCounter == 1) {
      mImpl.init();
    }
  }

  virtual MERROR process(RequestPtr pRequest, RequestCallbackPtr pCallback) {
    if (pRequest == nullptr) {
      return BAD_VALUE;
    }

    // Asynchronous
    if (pCallback != nullptr) {
      mMutex.lock();
      mRequests.push_back(pRequest);
      if (mCallbackMap.find(pCallback) == mCallbackMap.end()) {
        mCallbackMap[pCallback] = std::make_shared<Callback>(this, pCallback);
      }
      mMutex.unlock();

      // Redirect to interceptor's callback
      MERROR ret = mImpl.process(pRequest, mCallbackMap[pCallback]);
      if (ret != OK) {
        mMutex.lock();
        mRequests.erase(
            std::remove(mRequests.begin(), mRequests.end(), pRequest));
        mMutex.unlock();
      }
      return ret;
    }

    return mImpl.process(pRequest, nullptr);
  }

  virtual void abort(const std::vector<RequestPtr>& pRequests) {
    mImpl.abort(pRequests);
  }

  virtual void uninit() {
    if (--mInitCounter == 0) {
      mImpl.uninit();
    }
  }

  virtual ~Interceptor() {
    mCallbackMap.clear();

    mMutex.lock();
    if (mRequests.size() != 0) {
      std::cout << "--- the requests not returned  ---" << std::endl;
    }
    mRequests.clear();
    mMutex.unlock();
  }

 private:
  P mImpl;
  const char* mName;
  std::atomic<int> mInitCounter;
  std::mutex mMutex;
  std::vector<RequestPtr> mRequests;
  std::map<RequestCallbackPtr, RequestCallbackPtr> mCallbackMap;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSPipelinePlugin
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_PLUGIN_PLUGININTERCEPTOR_H_
