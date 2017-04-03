// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_LOG_LEVEL_H_
#define AUTHPOLICY_LOG_LEVEL_H_

#include "base/logging.h"

namespace authpolicy {

// TODO(ljusten): Set to logging::LOG_INFO after TT. https://crbug.com/666691

// Log level for Samba net commands, only shown if executor output is logged as
// well, see |kLogExecutorOutput|.
const char kNetLogLevel[] = "10";

//
// Switches for debug logs of various systems.
//

// Policy encoder.
const bool kLogEncoder = true;  // Policy values.

// ProcessExecutor.
const bool kLogExecutorCommand = true;  // Command line and exit code.
const bool kLogExecutorOutput = true;   // Stdout and stderr.

// GPO parsing.
const bool kLogGpo = true;  // List of filtered, broken and valid GPOs.

}  // namespace authpolicy

#endif  // AUTHPOLICY_LOG_LEVEL_H_
