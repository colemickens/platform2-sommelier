/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_ADAPTER_CAMERA_TRACE_EVENT_H_
#define CAMERA_HAL_ADAPTER_CAMERA_TRACE_EVENT_H_

#define TRACE_CAMERA_COMBINE_NAME1(X, Y) X##Y
#define TRACE_CAMERA_COMBINE_NAME(X, Y) TRACE_CAMERA_COMBINE_NAME1(X, Y)

#define TRACE_CAMERA_ENABLE(enabled) \
  cros::tracer::EventTracer::GetInstance()->SetEnabled(enabled)
#define TRACE_CAMERA_SCOPED(...)                       \
  cros::tracer::ScopedTrace TRACE_CAMERA_COMBINE_NAME( \
      scoped_trace_, __LINE__)(__FUNCTION__, ##__VA_ARGS__)
#define TRACE_CAMERA_INSTANT(...) \
  (cros::tracer::ScopedTrace(__FUNCTION__, ##__VA_ARGS__))
#define TRACE_CAMERA_BEGIN(name, ...)                   \
  cros::tracer::EventTracer::GetInstance()->BeginTrace( \
      name, cros::tracer::ArgsString(__VA_ARGS__))
#define TRACE_CAMERA_END(name, ...)                   \
  cros::tracer::EventTracer::GetInstance()->EndTrace( \
      name, cros::tracer::ArgsString(__VA_ARGS__))
#define TRACE_CAMERA_ASYNC_BEGIN(name, cookie, ...)          \
  cros::tracer::EventTracer::GetInstance()->AsyncBeginTrace( \
      name, cookie, cros::tracer::ArgsString(__VA_ARGS__))
#define TRACE_CAMERA_ASYNC_END(name, cookie, ...)          \
  cros::tracer::EventTracer::GetInstance()->AsyncEndTrace( \
      name, cookie, cros::tracer::ArgsString(__VA_ARGS__))
#define TRACE_CAMERA_COUNTER(name, value) \
  cros::tracer::EventTracer::GetInstance()->Counter(name, value)

#include <set>
#include <sstream>
#include <string>

#include <base/files/file.h>
#include <base/strings/string_piece.h>
#include <base/synchronization/lock.h>

namespace cros {

namespace tracer {

class EventTracer {
 public:
  EventTracer();

  // Not copyable or movable
  EventTracer(const EventTracer&) = delete;
  EventTracer& operator=(const EventTracer&) = delete;

  static EventTracer* GetInstance();
  void SetEnabled(bool enabled);
  void BeginTrace(base::StringPiece name, base::StringPiece args);
  void EndTrace(base::StringPiece name, base::StringPiece args);
  void AsyncBeginTrace(base::StringPiece name,
                       int cookie,
                       base::StringPiece args);
  void AsyncEndTrace(base::StringPiece name,
                     int cookie,
                     base::StringPiece args);
  void Counter(base::StringPiece name, int value);

 private:
  void TracePrintf(_Printf_format_string_ const char* format, ...);

  bool tracing_enabled_ = false;
  std::set<pid_t> begun_tid_;
  base::File trace_file_;
  base::Lock event_tracer_lock_;
};

inline void AppendArgsString(std::stringstream& args_buf) {}

template <typename ArgName, typename ArgVal, typename... Rest>
void AppendArgsString(std::stringstream& args_buf,
                      ArgName arg_name,
                      ArgVal arg_val,
                      Rest... rest) {
  args_buf << arg_name << "=" << arg_val << ";";
  AppendArgsString(args_buf, rest...);
}

template <typename... Rest>
std::string ArgsString(Rest... rest) {
  std::stringstream args_buf;
  AppendArgsString(args_buf, rest...);
  return args_buf.str();
}

class ScopedTrace {
 public:
  explicit ScopedTrace(base::StringPiece name) : name_(name) {
    EventTracer::GetInstance()->BeginTrace(name_, "");
  }

  template <typename... Rest>
  ScopedTrace(base::StringPiece name, Rest... rest) : name_(name) {
    EventTracer::GetInstance()->BeginTrace(name_, ArgsString(rest...));
  }

  ~ScopedTrace() { EventTracer::GetInstance()->EndTrace(name_, ""); }

 private:
  base::StringPiece name_;
};

}  // namespace tracer

}  // namespace cros

#endif  // CAMERA_HAL_ADAPTER_CAMERA_TRACE_EVENT_H_
