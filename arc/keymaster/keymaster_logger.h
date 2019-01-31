// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_KEYMASTER_KEYMASTER_LOGGER_H_
#define ARC_KEYMASTER_KEYMASTER_LOGGER_H_

#include <base/macros.h>
#include <keymaster/logger.h>

namespace arc {
namespace keymaster {

// Logger implementation that forwards messages to Chrome OS's logging system.
class KeymasterLogger : public ::keymaster::Logger {
 public:
  KeymasterLogger();
  ~KeymasterLogger() override = default;

  int log_msg(LogLevel level, const char* fmt, va_list args) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeymasterLogger);
};

}  // namespace keymaster
}  // namespace arc

#endif  // ARC_KEYMASTER_KEYMASTER_LOGGER_H_
