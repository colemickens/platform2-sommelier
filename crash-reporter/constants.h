// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CONSTANTS_H_
#define CRASH_REPORTER_CONSTANTS_H_

namespace constants {

constexpr char kCrashGroupName[] = "crash-access";

// Key in the .meta file. If "false", this crash report was created as part of a
// test and should not be uploaded to the server. Considered "true" (go ahead
// and upload) if not present.
constexpr char kUploadAllowedKey[] = "upload";
}

#endif  // CRASH_REPORTER_CONSTANTS_H_
