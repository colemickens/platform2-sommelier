// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_ROUTE_TOOL_H_
#define DEBUGD_SRC_ROUTE_TOOL_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <brillo/variant_dictionary.h>

namespace debugd {

class RouteTool {
 public:
  RouteTool() = default;
  ~RouteTool() = default;

  std::vector<std::string> GetRoutes(const brillo::VariantDictionary& options);

 private:
  DISALLOW_COPY_AND_ASSIGN(RouteTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_ROUTE_TOOL_H_
