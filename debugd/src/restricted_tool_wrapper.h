// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides the RestrictedToolWrapper template class, which helps
// control access to tools that should not always be available for use. Typical
// usage will look something like this:
//
//   // Instantiate the tool wrapper.
//   RestrictedToolWrapper<FooTool>* foo_tool_wrapper =
//       new RestrictedToolWrapper<FooTool>(...);
//
//   // Unwrap and use the tool.
//   DBus::Error error;
//   int result = 0;
//   foo_tool_wrapper_->CallToolFunction([&result, &error] (FooTool* tool) {
//                                         result = tool->ToolFunction(&error);
//                                       },
//                                       &error);
//
// Some advantages of using a wrapper rather than putting the condition check
// inside the tool functions themselves are:
//   1. Conditions are declared in a single location during tool instantiation,
//      rather than being spread around into each tool implementation.
//   2. The compiler prevents forgotten condition checks, since trying to use a
//      wrapper directly will cause compilation errors. This becomes important
//      with multiple access-restricted functions to avoid having to manually
//      put the right condition in each one.
//   3. Reusability - currently only the DevFeaturesTool class is wrapped,
//      but the template wrapper could be applied to future classes without
//      any condition logic in the classes themselves.

#ifndef DEBUGD_SRC_RESTRICTED_TOOL_WRAPPER_H_
#define DEBUGD_SRC_RESTRICTED_TOOL_WRAPPER_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "debugd/src/dev_mode_no_owner_restriction.h"

namespace debugd {

// Templated wrapper to enforce tool access restrictions. See comments at the
// top of the file for usage notes.
template <class T>
class RestrictedToolWrapper {
 public:
  // Tools without a default constructor may need specialized
  // RestrictedToolWrapper classes for additional constructor parameters. If
  // possible, use a tool Initialize() function instead of passing additional
  // parameters to the constructor.
  explicit RestrictedToolWrapper(DBus::Connection* system_dbus)
      : restriction_(system_dbus) {}

  ~RestrictedToolWrapper() = default;

  // Returns a raw pointer to the underlying tool instance if both conditions
  // from the DevModeNoOwnerRestriction class are met:
  //   1. Device is in dev mode.
  //   2. Device has no owner.
  // Otherwise, returns nullptr and |error| is set (if it's non-null).
  //
  // Do not store the direct tool pointer longer than needed for immediate use,
  // to avoid bypassing the wrapper's condition checks. Prefer to use
  // CallToolFunction() when possible to consolidate common access logic.
  T* GetTool(DBus::Error* error) {
    if (restriction_.AllowToolUse(error)) {
      return &tool_;
    }
    return nullptr;
  }

  // Attempts to unwrap the underlying tool and call a function. Typically
  // |function| will be a lambda to perform whatever task is needed; the only
  // restriction is that the function must only take a T* as its argument.
  // |function| will not be called if tool access fails.
  //
  // If access fails, returns false and |error| is set (if it's non-null).
  template <class Func>
  bool CallToolFunction(Func function, DBus::Error* error) {
    T* tool = GetTool(error);
    if (tool) {
      function(tool);
      return true;
    }
    return false;
  }

 private:
  T tool_;
  DevModeNoOwnerRestriction restriction_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedToolWrapper);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_RESTRICTED_TOOL_WRAPPER_H_
