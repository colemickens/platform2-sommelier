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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_MODULE_MODULE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_MODULE_MODULE_H_
//
/******************************************************************************
 *
 ******************************************************************************/
#include <mtkcam/def/common.h>
#include <mtkcam/main/common/module.h>
#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/*****************************************************************************
 * @brief Get the mtkcam module factory.
 *
 * @details Given a mtkcam module ID, return its corresponding module factory.
 * The real type of module factory depends on the given module ID, and users
 * must cast the output to the real type of module factory.
 *
 * @note
 *
 * @param[in] module_id: mtkcam module ID.
 *
 * @return a module factory corresponding to the given module ID.
 *
 ******************************************************************************/
void* VISIBILITY_PUBLIC getMtkcamModuleFactory(uint32_t module_id);

#define MAKE_MTKCAM_MODULE(_module_id_, _module_factory_type_, ...)     \
  ({                                                                    \
    void* factory = getMtkcamModuleFactory(_module_id_);                \
    (factory ? (((_module_factory_type_)factory)(__VA_ARGS__)) : NULL); \
  })

/*****************************************************************************
 * @brief Get the mtkcam module.
 *
 * @details Given a mtkcam module ID, return its corresponding mtkcam module.
 *
 * @note
 *
 * @param[in] module_id: mtkcam module ID.
 *
 * @param[out] module: a pointer to mtkcam module.
 *
 * @return: 0 == success, <0 == error and *module == NULL
 *
 ******************************************************************************/
int VISIBILITY_PUBLIC getMtkcamModule(uint32_t module_id,
                                      mtkcam_module** module);

#define GET_MTKCAM_MODULE_EXTENSION(_module_id_)                              \
  ({                                                                          \
    mtkcam_module* m = NULL;                                                  \
    ((0 == getMtkcamModule(_module_id_, &m) && m) ? m->get_module_extension() \
                                                  : NULL);                    \
  })

/******************************************************************************
 *
 ******************************************************************************/
__END_DECLS
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_MODULE_MODULE_H_
