// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_LOGGING_H_
#define SHILL_LOGGING_H_

#include <base/logging.h>

#include "shill/memory_log.h"
#include "shill/scope_logger.h"

// How to use:
//
// The SLOG macro and its variants are similar to the VLOG macros
// defined in base/logging.h, except that the SLOG macros take an additional
// |scope| argument to enable logging only if |scope| is enabled. In addition,
// messages sent to SLOG[_IF] are sent to the MemoryLog, whether or not
// that scope is enabled.
//
// Like VLOG, SLOG macros internally map verbosity to LOG severity using
// negative values, i.e. SLOG(scope, 1) corresponds to LOG(-1).
//
// Example usages:
//  SLOG(Service, 1) << "Printed when the 'service' scope is enabled and "
//                      "the verbose level is greater than or equal to 1";
//
//  SLOG_IF(Service, 1, (size > 1024))
//      << "Printed when the 'service' scope is enabled, the verbose level "
//         "is greater than or equal to 1, and size is more than 1024";
//
// Similarly, MLOG and MLOG_IF are equivalents to LOG and LOG_IF that send
// their messages through the MemoryLog on the way to the normal logging system.
//
// Example usages:
//  MLOG(ERROR) << "Message logged at ERROR level";
//
//  MLOG_IF(INFO, tacos < enough) << "Such a sad day";
//
//

#define SLOG_IS_ON(scope, verbose_level) \
  ::shill::ScopeLogger::GetInstance()->IsLogEnabled( \
      ::shill::ScopeLogger::k##scope, verbose_level)

#define MLOG_SCOPE_STREAM(scope, verbose_level) \
    ::shill::MemoryLogMessage(__FILE__, __LINE__, \
                              -verbose_level, \
                              SLOG_IS_ON(scope, verbose_level)).stream()

#define MLOG_SEVERITY_STREAM(severity) \
    ::shill::MemoryLogMessage(__FILE__, __LINE__, \
                              logging::LOG_ ## severity, \
                              LOG_IS_ON(severity)).stream()

#define SLOG(scope, verbose_level) MLOG_SCOPE_STREAM(scope, verbose_level)

#define SLOG_IF(scope, verbose_level, condition) \
    LAZY_STREAM(MLOG_SCOPE_STREAM(scope, verbose_level), condition)

// TODO(wiley) Add SLOG macros (crosbug.com/33416)

// Redefine these macros to route messages through the MemoryLog
#undef LOG
#undef LOG_IF
#undef LOG_STREAM

#define LOG(severity) MLOG_SEVERITY_STREAM(severity)

#define LOG_IF(severity, condition) \
    LAZY_STREAM(MLOG_SEVERITY_STREAM(severity), condition)

// This lets us catch things like CHECK
#define LOG_STREAM(severity) MLOG_SEVERITY_STREAM(severity)

// TODO(wiley) Add MPLOG macros (crosbug.com/33416)

#endif  // SHILL_LOGGING_H_
