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

#define LOG_TAG "mtkcam-module"
//
#include <atomic>
#include <dlfcn.h>
#include <limits.h>
#include <mtkcam/utils/module/module.h>
#include <mtkcam/utils/std/Log.h>
#include <mutex>
//
/******************************************************************************
 *
 ******************************************************************************/
namespace {
class MyHolder {
 protected:
  typedef void* (*MY_T)(unsigned int moduleId);

  MY_T mTarget_ctor;                 // from constructor
  std::atomic<MY_T> mTarget_atomic;  // from atomic
  std::mutex mMutex;
  void* mLibrary;

  char const* const mTargetLibPath;
  char const* const mTargetSymbolName;

 protected:
  void load(void** rpLib, MY_T* rTarget) {
    void* pfnEntry = nullptr;
    void* lib = ::dlopen(mTargetLibPath, RTLD_NOW);
    if (!lib) {
      char const* err_str = ::dlerror();
      CAM_LOGE("dlopen: %s error:%s", mTargetLibPath,
               (err_str ? err_str : "unknown"));
      goto lbExit;
    }
    //
    pfnEntry = ::dlsym(lib, mTargetSymbolName);
    if (!pfnEntry) {
      char const* err_str = ::dlerror();
      CAM_LOGE("dlsym: %s (@%s) error:%s", mTargetSymbolName, mTargetLibPath,
               (err_str ? err_str : "unknown"));
      goto lbExit;
    }
    //
    CAM_LOGI("%s(%p) @ %s", mTargetSymbolName, pfnEntry, mTargetLibPath);
    *rpLib = lib;
    *rTarget = reinterpret_cast<MY_T>(pfnEntry);
    return;
    //
  lbExit:
    if (lib) {
      ::dlclose(lib);
      lib = nullptr;
    }
  }

 public:
  ~MyHolder() {
    if (mLibrary) {
      ::dlclose(mLibrary);
      mLibrary = nullptr;
    }
  }

  MyHolder(char const* szTargetLibPath, char const* szTargetSymbolName)
      : mTarget_ctor(nullptr),
        mTarget_atomic(),
        mMutex(),
        mLibrary(nullptr)
        //
        ,
        mTargetLibPath(szTargetLibPath),
        mTargetSymbolName(szTargetSymbolName) {
    load(&mLibrary, &mTarget_ctor);
  }

  MY_T get() {
    // Usually the target can be established during constructor.
    // Note: in this case we don't need any lock here.
    if (mTarget_ctor) {
      return mTarget_ctor;
    }

    // Since the target cannot be established during constructor,
    // we're trying to do it again now.
    //
    // Double-checked locking
    auto tmp = mTarget_atomic.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    //
    if (tmp == nullptr) {
      std::lock_guard<std::mutex> _l(mMutex);
      tmp = mTarget_atomic.load(std::memory_order_relaxed);
      if (tmp == nullptr) {
        CAM_LOGW(
            "fail to establish it during constructor, so we're trying to do "
            "now");
        load(&mLibrary, &tmp);
        //
        std::atomic_thread_fence(std::memory_order_release);
        mTarget_atomic.store(tmp, std::memory_order_relaxed);
      }
    }
    return tmp;
  }
};
}  // namespace

/******************************************************************************
 *
 ******************************************************************************/
void* getMtkcamModuleFactory(uint32_t module_id) {
  switch (MTKCAM_GET_MODULE_GROUP_ID(module_id)) {
#define CASE(_group_id_, _group_shared_lib_, _group_factory_) \
  case _group_id_: {                                          \
    static MyHolder* singleton =                              \
        new MyHolder(_group_shared_lib_, _group_factory_);    \
    if (auto factory = singleton->get()) {                    \
      return factory(module_id);                              \
    }                                                         \
  } break

    CASE(MTKCAM_MODULE_GROUP_ID_DRV, "libmtkcam_modulefactory_drv.so",
         "MtkCam_getModuleFactory_drv");

    CASE(MTKCAM_MODULE_GROUP_ID_AAA, "libmtkcam_modulefactory_aaa.so",
         "MtkCam_getModuleFactory_aaa");

    CASE(MTKCAM_MODULE_GROUP_ID_FEATURE, "libmtkcam_modulefactory_feature.so",
         "MtkCam_getModuleFactory_feature");

    CASE(MTKCAM_MODULE_GROUP_ID_CUSTOM, "libmtkcam_modulefactory_custom.so",
         "MtkCam_getModuleFactory_custom");

    CASE(MTKCAM_MODULE_GROUP_ID_UTILS, "libmtkcam_modulefactory_utils.so",
         "MtkCam_getModuleFactory_utils");

#undef CASE

    default:
      CAM_LOGE("Unsupported module id:0x%#x, group id:%u", module_id,
               MTKCAM_GET_MODULE_GROUP_ID(module_id));
      break;
  }
  //
  return NULL;
}

/******************************************************************************
 *
 ******************************************************************************/
int getMtkcamModule(uint32_t module_id, mtkcam_module** module) {
  void* factory = getMtkcamModuleFactory(module_id);
  if (!factory) {
    CAM_LOGE("[module id:%#x] Not found", module_id);
    return -ENOENT;
  }

  mtkcam_module* m = ((mtkcam_module * (*)()) factory)();
  if (!m) {
    CAM_LOGE("[module id:%#x] No such module returned from factory:%p",
             module_id, factory);
    return -ENODEV;
  }

  if (!m->get_module_id) {
    CAM_LOGE("[module id:%#x] Not implemented: get_module_id", module_id);
    return -ENOSYS;
  }

  if (m->get_module_id() != module_id) {
    CAM_LOGE("[module id:%#x] Not match with get_module_id() -> %#x", module_id,
             m->get_module_id());
    return -EFAULT;
  }

  if (!m->get_module_extension) {
    CAM_LOGE("[module id:%#x] Not implemented: get_module_extension",
             module_id);
    return -ENOSYS;
  }

  if (!m->get_module_extension()) {
    CAM_LOGE("[module id:%#x] get_module_extension() -> NULL", module_id);
    return -EFAULT;
  }

  if (!module) {
    CAM_LOGE("[module id:%#x] Invalid argument", module_id);
    return -EINVAL;
  }

  *module = m;
  return 0;
}
