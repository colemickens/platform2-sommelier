/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_trace_event.h"

#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

#include <base/files/file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/lock.h>

#include "cros-camera/common.h"

namespace cros {

namespace tracer {

namespace {

pid_t gettid() {
  return syscall(SYS_gettid);
}

}  // namespace

const char kTraceMarkerPath[] = "/sys/kernel/debug/tracing/trace_marker";

EventTracer::EventTracer() = default;

EventTracer* EventTracer::GetInstance() {
  static EventTracer* instance = new EventTracer();
  return instance;
}

void EventTracer::SetEnabled(bool enabled) {
  base::AutoLock l(event_tracer_lock_);
  tracing_enabled_ = enabled;
  begun_tid_.clear();
  if (!tracing_enabled_) {
    trace_file_.Close();
  } else {
    trace_file_ = base::File(base::FilePath(kTraceMarkerPath),
                             base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  }
}

void EventTracer::BeginTrace(base::StringPiece name, base::StringPiece args) {
  VLOGF_ENTER();
  base::AutoLock l(event_tracer_lock_);
  if (!tracing_enabled_) {
    return;
  }
  // We use %.*s here because TotW [1] and the comment of
  // StringPiece.data() [2] say that data() may not return a null
  // terminated buffer.
  // [1] go/totw/1
  // [2] https://cs.chromium.org/chromium/src/base/strings/string_piece.h?l=204
  TracePrintf("B|%d|%.*s|%.*s|camera", getpid(), static_cast<int>(name.size()),
              name.data(), static_cast<int>(args.size()), args.data());
  if (!begun_tid_.insert(gettid()).second) {
    LOGF(WARNING) << "Begin a tracing event " << name
                  << " while pervious event isn't finished."
                  << " Pervious event will be canceled.";
  }
}

void EventTracer::EndTrace(base::StringPiece name, base::StringPiece args) {
  VLOGF_ENTER();
  base::AutoLock l(event_tracer_lock_);
  if (!tracing_enabled_) {
    return;
  }
  TracePrintf("E|%d|%.*s|%.*s|camera", getpid(), static_cast<int>(name.size()),
              name.data(), static_cast<int>(args.size()), args.data());
  if (!begun_tid_.erase(gettid())) {
    LOGF(WARNING) << "Tracing event " << name
                  << " not exists or was canceled by other event.";
  }
}

void EventTracer::AsyncBeginTrace(base::StringPiece name,
                                  int cookie,
                                  base::StringPiece args) {
  VLOGF_ENTER();
  base::AutoLock l(event_tracer_lock_);
  if (!tracing_enabled_) {
    return;
  }
  TracePrintf("S|%d|%.*s|%d|%.*s|camera", getpid(),
              static_cast<int>(name.size()), name.data(), cookie,
              static_cast<int>(args.size()), args.data());
}

void EventTracer::AsyncEndTrace(base::StringPiece name,
                                int cookie,
                                base::StringPiece args) {
  VLOGF_ENTER();
  base::AutoLock l(event_tracer_lock_);
  if (!tracing_enabled_) {
    return;
  }
  TracePrintf("F|%d|%.*s|%d|%.*s|camera", getpid(),
              static_cast<int>(name.size()), name.data(), cookie,
              static_cast<int>(args.size()), args.data());
}

void EventTracer::Counter(base::StringPiece name, int value) {
  VLOGF_ENTER();
  base::AutoLock l(event_tracer_lock_);
  if (!tracing_enabled_) {
    return;
  }
  TracePrintf("C|%d|%.*s|%d|camera", getpid(), static_cast<int>(name.size()),
              name.data(), value);
}

void EventTracer::TracePrintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  if (trace_file_.IsValid()) {
    if (vdprintf(trace_file_.GetPlatformFile(), format, args) < 0) {
      LOGF(WARNING) << "vdprintf error.";
    }
  } else {
    LOGF(WARNING) << "Trace file is invaild.";
  }
  va_end(args);
}

}  // namespace tracer

}  // namespace cros
