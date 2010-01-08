// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_IPC_MESSAGE_H_
#define LOGIN_MANAGER_IPC_MESSAGE_H_

namespace login_manager {

enum IpcMessage {
  EMIT_LOGIN = 'a',
  START_SESSION = 'b',
  STOP_SESSION = 'c',
  FAILED = 'f',
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_IPC_MESSAGE_H_
