// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_SERVICE_NAME_H_
#define SOMA_SERVICE_NAME_H_

#include <string>
#include <vector>

namespace base {
class ListValue;
}

namespace soma {
namespace parser {
namespace service_name {

extern const char kListKey[];

std::vector<std::string> ParseList(const base::ListValue* annotations);

}  // namespace service_name
}  // namespace parser
}  // namespace soma

#endif  // SOMA_SERVICE_NAME_H_
