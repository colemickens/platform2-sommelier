// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/service_name.h"

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/values.h>

namespace soma {
namespace parser {
namespace service_name {

const char kListKey[] = "annotations";
const char kNameKey[] = "name";
const char kValueKey[] = "value";

std::vector<std::string> ParseList(const base::ListValue* annotations) {
  DCHECK(annotations);
  std::vector<std::string> to_return(annotations->GetSize());
  // TODO(cmasone): Enforce formatting rules on name and value elements.
  for (const base::Value* annotation_value : *annotations) {
    const base::DictionaryValue* annotation = nullptr;
    if (!annotation_value->GetAsDictionary(&annotation)) {
      LOG(ERROR) << "'annotations' must be a list of dicts.";
      return std::vector<std::string>();
    }

    std::string name, value;
    if (!annotation->GetString(kNameKey, &name) ||
        !annotation->GetString(kValueKey, &value)) {
      LOG(ERROR) << "Each annotation must have 'name' and 'value' fields.";
      return std::vector<std::string>();
    }

    if (StartsWithASCII(name, "service-", false))
      to_return.push_back(value);
  }
  return to_return;
}

}  // namespace service_name
}  // namespace parser
}  // namespace soma
