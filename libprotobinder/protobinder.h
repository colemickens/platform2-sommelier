// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_PROTOBINDER_H_
#define LIBPROTOBINDER_PROTOBINDER_H_

// TODO(leecam): Make an enum and put somewhere better.
#define SUCCESS 0
// We comment out ERROR temporarily because it conflicts with LOG(ERROR).
// #define ERROR -1
#define ERROR_CMD_PARCEL -2
#define ERROR_REPLY_PARCEL -3
#define ERROR_FAILED_TRANSACTION -4
#define ERROR_DEAD_ENDPOINT -5
#define ERROR_UNKNOWN_CODE -6

#endif  // LIBPROTOBINDER_PROTOBINDER_H_
