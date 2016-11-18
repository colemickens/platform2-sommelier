/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_COMMON_H_
#define HAL_ADAPTER_COMMON_H_

#include <base/logging.h>

#define LOGF(level) LOG(level) << __func__ << ": "

#define VLOGF(level) VLOG(level) << __func__ << ": "

#define VLOGF_ENTER() VLOGF(1) << "enter"
#define VLOGF_EXIT() VLOGF(1) << "exit"

#endif  // HAL_ADAPTER_COMMON_H_
