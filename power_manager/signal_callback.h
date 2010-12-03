// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_SIGNAL_CALLBACK_H_
#define POWER_MANAGER_SIGNAL_CALLBACK_H_

#define SIGNAL_CALLBACK_0(CLASS, RETURN, METHOD)      \
  static RETURN METHOD ## Thunk(gpointer data) {      \
    return reinterpret_cast<CLASS*>(data)->METHOD();  \
  }                                                   \
                                                      \
  RETURN METHOD();

#endif  // POWER_MANAGER_SIGNAL_CALLBACK_H_
