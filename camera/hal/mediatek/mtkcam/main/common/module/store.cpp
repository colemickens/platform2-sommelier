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

#include <mtkcam/main/common/module/store.h>

#include <android/log.h>
#include <mtkcam/def/common.h>
//
#include <mtkcam/main/common/module/local.h>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace {
/**************************************************************************
 *
 **************************************************************************/
enum {
  MODULE_GROUP_ID = MTKCAM_MODULE_GROUP_ID,
  MODULE_GROUP_ID_START = MTKCAM_MODULE_GROUP_ID_START,
  MODULE_GROUP_ID_END = MTKCAM_MODULE_GROUP_ID_END,
  MODULE_GROUP_COUNT =
      MTKCAM_MODULE_GROUP_ID_END - MTKCAM_MODULE_GROUP_ID_START,
};

/**************************************************************************
 *
 **************************************************************************/
struct Store {
  /**********************************************************************
   *
   **********************************************************************/
  static mtkcam_module_info* get_module_info(unsigned int module_id) {
    struct ModuleStore {
      mtkcam_module_info table[MODULE_GROUP_COUNT];
      ModuleStore() {
        for (auto i = 0; i < MODULE_GROUP_COUNT; i++) {
          table[i].module_id = 0;
          table[i].module_factory = nullptr;
          table[i].register_name = nullptr;
        }
        MY_LOGI("ctor");
      }
    };
    static ModuleStore* store = new ModuleStore();

    if (!check_module_id(module_id)) {
      return NULL;
    }

    return &(store->table[MTKCAM_GET_MODULE_INDEX(module_id)]);
  }

  /**********************************************************************
   *
   **********************************************************************/
  static void* get_module_factory(unsigned int module_id) {
    mtkcam_module_info const* info = get_module_info(module_id);
    if (!info) {
      return NULL;
    }

    if (!info->module_factory) {
      MY_LOGW("[module_id:0x%#x] Bad module_factory==NULL", module_id);
      dump_module(info);
      return NULL;
    }

    return info->module_factory;
  }

  /**********************************************************************
   *
   **********************************************************************/
  static void register_module(mtkcam_module_info const* info) {
    // check module id
    if (!check_module_id(info->module_id)) {
      dump_module(info);
      return;
    }

    if (!info->module_factory) {
      MY_LOGW("Bad module_factory==NULL");
      dump_module(info);
      return;
    }

    if (get_module_info(info->module_id)->module_factory) {
      MY_LOGE("Has registered before");
      dump_module(get_module_info(info->module_id), "old");
      dump_module(info, "new");
      return;
    }

    dump_module(info, "registered");
    *get_module_info(info->module_id) = *info;
  }

  /**********************************************************************
   *
   **********************************************************************/
  static bool check_module_id(unsigned int module_id) {
    unsigned int const group_id = MTKCAM_GET_MODULE_GROUP_ID(module_id);
    unsigned int const module_index = MTKCAM_GET_MODULE_INDEX(module_id);

    // check module group id
    if (MODULE_GROUP_ID != group_id) {
      MY_LOGE("Bad module_id(0x%#x) whose group id(%u) != %u ", module_id,
              group_id, MODULE_GROUP_ID);
      return false;
    }

    // check module index
    if (MODULE_GROUP_COUNT <= module_index) {
      MY_LOGE(
          "Bad module_id(0x%#x) whose module index(%u) >= module group "
          "count(%u) ",
          module_id, module_index, MODULE_GROUP_COUNT);
      return false;
    }

    return true;
  }

  /**********************************************************************
   *
   **********************************************************************/
  static void dump_module(mtkcam_module_info const* info,
                          char const* prefix_msg = "") {
    MY_LOGI("[%s] module_id:0x%#x module_factory:%p register_name:%s",
            prefix_msg, info->module_id, info->module_factory,
            (info->register_name ? info->register_name : "unknown"));
  }
};  // struct

/**************************************************************************
 *
 **************************************************************************/
struct ShowLoading {
  ShowLoading() {
    MY_LOGI("loading (MODULE_GROUP_ID:%u MODULE_GROUP_COUNT:%u ...",
            MODULE_GROUP_ID, MODULE_GROUP_COUNT);
  }
};
static const ShowLoading gShowLoading;

};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
void register_mtkcam_module(mtkcam_module_info const* info,
                            NSCam::Int2Type<MTKCAM_MODULE_GROUP_ID>) {
  Store::register_module(info);
}

/******************************************************************************
 *
 ******************************************************************************/
VISIBILITY_PUBLIC extern "C" void* MTKCAM_GET_MODULE_FACTORY(
    unsigned int moduleId) {
  return Store::get_module_factory(moduleId);
}
