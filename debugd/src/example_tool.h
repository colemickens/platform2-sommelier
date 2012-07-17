// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is an example of a tool. See </src/example_tool.cc>.

#ifndef EXAMPLE_TOOL_H
#define EXAMPLE_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ExampleTool {
 public:
  ExampleTool();
  ~ExampleTool();

  std::string GetExample(DBus::Error& error); // NOLINT
 private:
  DISALLOW_COPY_AND_ASSIGN(ExampleTool);
};

};  // namespace debugd

#endif  // !EXAMPLE_TOOL_H
