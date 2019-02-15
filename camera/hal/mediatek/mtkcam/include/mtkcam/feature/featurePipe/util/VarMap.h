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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_UTIL_VARMAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_UTIL_VARMAP_H_

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#define DECLARE_VAR_MAP_INTERFACE(mapName, setF, getF, tryGetF, clearF) \
  template <typename T>                                                 \
  bool setF(const char* name, const T& var) {                           \
    return mapName.set<T>(name, var);                                   \
  }                                                                     \
  template <typename T>                                                 \
  T getF(const char* name, const T& var) const {                        \
    return mapName.get<T>(name, var);                                   \
  }                                                                     \
  template <typename T>                                                 \
  bool tryGetF(const char* name, T& var) const {                        \
    return mapName.tryGet<T>(name, var);                                \
  }                                                                     \
  template <typename T>                                                 \
  void clearF(const char* name) {                                       \
    return mapName.clear<T>(name);                                      \
  }

namespace NSCam {
namespace NSCamFeature {

template <typename T>
const char* getTypeNameID() {
  const char* name = __PRETTY_FUNCTION__;
  return name;
}

class VarHolderBase {
 public:
  VarHolderBase() {}

  virtual ~VarHolderBase() {}

  virtual void* getPtr() const = 0;
};

class VarMap {
 private:
  template <typename T>
  class VarHolder : public VarHolderBase {
   public:
    explicit VarHolder(const T& var) : mVar(var) {}

    virtual ~VarHolder() {}

    virtual void* getPtr() const {
      return const_cast<void*>(reinterpret_cast<const void*>(&mVar));
    }

   private:
    T mVar;
  };

 public:
  VarMap() {}

  VarMap(const VarMap& src) {
    std::lock_guard<std::mutex> lock(src.mMutex);
    mMap = src.mMap;
  }

  VarMap& operator=(const VarMap& src) {
    // lock in strict order to avoid deadlock
    // check to avoid self assignment
    if (this < &src) {
      std::lock_guard<std::mutex> lock1(mMutex);
      std::lock_guard<std::mutex> lock2(src.mMutex);
      mMap = src.mMap;
    } else if (this > &src) {
      std::lock_guard<std::mutex> lock1(src.mMutex);
      std::lock_guard<std::mutex> lock2(mMutex);
      mMap = src.mMap;
    }
    return *this;
  }

  template <typename T>
  bool set(const char* name, const T& var) {
    std::lock_guard<std::mutex> lock(mMutex);
    bool ret = false;

    if (name) {
      std::string id;
      std::shared_ptr<VarHolderBase> holder;
      id = std::string(getTypeNameID<T>()) + std::string(name);
      holder = std::make_shared<VarHolder<T> >(var);

      if (holder) {
        mMap[id] = holder;
        ret = true;
      }
    }
    return ret;
  }

  template <typename T>
  T get(const char* name, T var) const {
    tryGet(name, &var);
    return var;
  }

  template <typename T>
  bool tryGet(const char* name, T* var) const {
    std::lock_guard<std::mutex> lock(mMutex);
    bool ret = false;
    T* holder = NULL;

    if (name) {
      std::string id;
      id = std::string(getTypeNameID<T>()) + std::string(name);
      CONTAINER::const_iterator it;
      it = mMap.find(id);

      if ((it != mMap.end()) && (it->second != NULL)) {
        if ((holder = static_cast<T*>(it->second->getPtr())) != NULL) {
          *var = *holder;
          ret = true;
        }
      }
    }
    return ret;
  }

  template <typename T>
  void clear(const char* name) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (name) {
      std::string id;
      std::shared_ptr<VarHolderBase> holder;
      id = std::string(getTypeNameID<T>()) + std::string(name);
      CONTAINER::iterator it;
      it = mMap.find(id);
      if (it != mMap.end()) {
        it->second = NULL;
        mMap.erase(it);
      }
    }
  }

 private:
  typedef std::map<std::string, std::shared_ptr<VarHolderBase> > CONTAINER;
  CONTAINER mMap;
  mutable std::mutex mMutex;
};

}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_UTIL_VARMAP_H_
