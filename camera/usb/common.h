/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef USB_COMMON_H_
#define USB_COMMON_H_

#include <base/logging.h>

#define LOGF(level) LOG(level) << __FUNCTION__ << "(): "
#define LOGFID(level, id) LOG(level) << __FUNCTION__ << "(): id: " << id << ": "

#define VLOGF(level) VLOG(level) << __FUNCTION__ << "(): "
#define VLOGFID(level, id) \
  VLOG(level) << __FUNCTION__ << "(): id: " << id << ": "

#endif  // USB_COMMON_H_
